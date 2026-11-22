import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'demo.dart';
import 'protocol.dart';
import 'transport.dart';

enum ConnectionStage {
  idle,
  discovering,
  candidates,
  connecting,
  identifying,
  loading,
  ready,
  error,
  disconnected,
}

class CompanionController extends ChangeNotifier {
  CompanionController(this.discovery);
  final DeviceDiscovery discovery;
  ConnectionStage stage = ConnectionStage.idle;
  List<Candidate> devices = [];
  Candidate? selected;
  Map<String, dynamic> identity = {};
  Map<String, dynamic> snapshot = {};
  List<String> capabilities = [];
  List<Map<String, dynamic>> apps = [];
  Map<String, dynamic> settings = {};
  String? errorCode;
  String? operationMessage;
  String? lastDeviceId;
  Candidate? lastDevice;
  DateTime? receivedAt;
  bool demo = false, busy = false, autoReconnect = false, setupDone = false;
  int page = 0;
  ProtocolClient? _client;
  Timer? _watcher;
  StreamSubscription<Map<String, dynamic>>? _events;
  int _epoch = 0;
  int _sequence = -1;
  bool _refreshing = false, _polling = false, _disposed = false;
  final List<String> _audit = [];
  bool get ready => stage == ConnectionStage.ready;
  bool get connecting => {
    ConnectionStage.connecting,
    ConnectionStage.identifying,
    ConnectionStage.loading,
  }.contains(stage);
  bool supports(String cmd) => ready && capabilities.contains(cmd);
  void _notify() {
    if (!_disposed) notifyListeners();
  }

  void _record(String code) {
    _audit.add('${DateTime.now().toUtc().toIso8601String()} $code');
    if (_audit.length > 100) _audit.removeAt(0);
  }

  void start() {
    unawaited(refresh());
    _watcher ??= Timer.periodic(const Duration(seconds: 2), (_) async {
      await refresh();
      if (ready && !_polling) {
        _polling = true;
        final epoch = _epoch;
        try {
          final before = _sequence;
          final s = await _client!.request('device.get');
          if (epoch == _epoch && !_acceptFresh(s, before))
            await disconnect(lost: true);
          if (epoch == _epoch && supports('settings.set') && !busy &&
              snapshot['settings_revision'] != settings['revision']) {
            final value = await _client!.request('settings.get');
            if (epoch == _epoch && value is Map<String, dynamic>) {
              settings = value;
              _notify();
            }
          }
        } catch (_) {
          if (epoch == _epoch) await disconnect(lost: true);
        } finally {
          _polling = false;
        }
      }
    });
  }

  Future<void> refresh() async {
    if (_refreshing || _disposed || demo || connecting) return;
    _refreshing = true;
    final epoch = _epoch;
    if (!ready &&
        stage != ConnectionStage.disconnected &&
        stage != ConnectionStage.error)
      stage = ConnectionStage.discovering;
    _notify();
    try {
      final found = await discovery.enumerate();
      if (_disposed || epoch != _epoch) return;
      devices = found;
      if (ready &&
          selected != null &&
          !found.any(
            (p) => p.port == selected!.port && p.usbId == selected!.usbId,
          )) {
        await disconnect(lost: true);
        return;
      }
      if (!ready) {
        if (selected != null && !found.any((p) => p.port == selected!.port && p.usbId == selected!.usbId))
          selected = null;
        final candidates = found.where((p) => p.candidate).toList();
        if (selected == null && candidates.length == 1)
          selected = candidates.single;
        if (stage == ConnectionStage.discovering)
          stage = candidates.isEmpty
              ? ConnectionStage.idle
              : ConnectionStage.candidates;
        if (autoReconnect &&
            stage == ConnectionStage.disconnected &&
            lastDevice != null &&
            lastDeviceId != null) {
          final same = found.where(
            (p) => p.usbId.isNotEmpty && p.usbId == lastDevice!.usbId,
          );
          if (same.length == 1) {
            selected = same.single;
            await connect(reconnect: true);
          }
        }
      }
    } catch (_) {
      if (!ready && epoch == _epoch) {
        stage = ConnectionStage.error;
        errorCode = 'discovery_failed';
      }
    } finally {
      _refreshing = false;
      _notify();
    }
  }

  void select(Candidate device) {
    if (!connecting && !ready) {
      selected = device;
      _notify();
    }
  }

  Future<void> connect({bool tryDemo = false, bool reconnect = false}) async {
    if (connecting || ready || (!tryDemo && selected == null)) return;
    final epoch = ++_epoch;
    demo = tryDemo;
    stage = ConnectionStage.connecting;
    errorCode = null;
    operationMessage = null;
    snapshot = {};
    identity = {};
    capabilities = [];
    apps = [];
    settings = {};
    _sequence = -1;
    receivedAt = null;
    _notify();
    ProtocolClient? client;
    try {
      final transport = tryDemo
          ? DemoTransport()
          : await discovery.open(selected!);
      if (_disposed || epoch != _epoch) {
        await transport.close();
        return;
      }
      client = ProtocolClient(transport);
      _client = client;
      _events = client.events.listen((event) {
        if (epoch != _epoch) return;
        if (event['event'] == 'connection.lost' && !connecting) {
          unawaited(disconnect(lost: true));
        } else if (ready && event['event'] == 'device.status') {
          _accept(event['data']);
        }
      });
      stage = ConnectionStage.identifying;
      _notify();
      final hello = await client.request('hello');
      if (epoch != _epoch) return;
      if (hello is! Map<String, dynamic> ||
          hello['product'] != 'WristFoundry' ||
          hello['device_id'] is! String ||
          hello['capabilities'] is! List) {
        throw const DeviceFailure('wrong_device');
      }
      if (reconnect && hello['device_id'] != lastDeviceId)
        throw const DeviceFailure('identity_changed');
      identity = hello;
      capabilities = (hello['capabilities'] as List)
          .whereType<String>()
          .toList();
      if (!capabilities.contains('device.get'))
        throw const DeviceFailure('incompatible_firmware');
      stage = ConnectionStage.loading;
      _notify();
      final initial = await client.request('device.get');
      if (epoch != _epoch) return;
      if (!_accept(initial)) throw const DeviceFailure('invalid_snapshot');
      if (!demo) {
        lastDeviceId = identity['device_id'] as String;
        lastDevice = selected;
      }
      stage = ConnectionStage.ready;
      _record(demo ? 'demo_connected' : 'identified');
      _notify();
    } catch (e) {
      if (epoch != _epoch || _disposed) return;
      await _events?.cancel();
      _events = null;
      await client?.close();
      _client = null;
      stage = ConnectionStage.error;
      errorCode = e is DeviceFailure ? e.code : 'connection_failed';
      _record('connection_error');
      snapshot = {};
      identity = {};
      capabilities = [];
      _notify();
    }
  }

  bool _accept(dynamic value) {
    if (value is! Map<String, dynamic> ||
        value['sequence'] is! num ||
        value['wifi_state'] is! String)
      return false;
    final seq = (value['sequence'] as num).toInt();
    if (seq <= _sequence) return false;
    _sequence = seq;
    final battery = value['battery_percent'];
    snapshot = {
      ...value,
      'battery_percent':
          battery is num && battery.isFinite && battery >= 0 && battery <= 100
          ? battery
          : null,
      'charging': value['charging'] is bool ? value['charging'] : null,
      'rtc_local': value['rtc_local'] is String ? value['rtc_local'] : '',
      'networks':
          (value['networks'] is List ? value['networks'] as List : const [])
              .whereType<Map>()
              .where(
                (ap) =>
                    ap['ssid'] is String &&
                    ap['rssi'] is num &&
                    ap['secured'] is bool,
              )
              .take(8)
              .toList(),
    };
    receivedAt = DateTime.now();
    _notify();
    return true;
  }

  bool _acceptFresh(dynamic value, int before) {
    if (value is! Map<String, dynamic> ||
        value['sequence'] is! num ||
        value['wifi_state'] is! String ||
        (value['sequence'] as num).toInt() <= before)
      return false;
    // An event can carry an even newer snapshot in the same received chunk.
    _accept(value);
    return true;
  }

  Future<void> disconnect({bool lost = false}) async {
    ++_epoch;
    final client = _client;
    _client = null;
    final events = _events;
    _events = null;
    snapshot = {};
    identity = {};
    capabilities = [];
    apps = [];
    settings = {};
    receivedAt = null;
    busy = false;
    operationMessage = null;
    stage = lost ? ConnectionStage.disconnected : ConnectionStage.idle;
    if (lost) _record('disconnected');
    if (!lost) autoReconnect = false;
    demo = false;
    _notify();
    try {
      await events?.cancel();
      await client?.close();
    } catch (_) {
      /* Port may already be removed. */
    }
  }

  Future<bool> execute(
    String cmd, [
    Map<String, dynamic> params = const {},
  ]) async {
    if (!supports(cmd) || busy) return false;
    final epoch = _epoch;
    busy = true;
    operationMessage = null;
    _notify();
    try {
      final result = await _client!.request(cmd, params);
      if (epoch != _epoch) return false;
      if (cmd == 'apps.list')
        apps = (result as List)
            .map((a) => Map<String, dynamic>.from(a as Map))
            .toList();
      if (cmd == 'settings.get' || cmd == 'settings.set' || cmd == 'settings.reset')
        settings = Map<String, dynamic>.from(result as Map);
      final before = _sequence;
      final fresh = await _client!.request('device.get');
      if (epoch != _epoch) return false;
      if (!_acceptFresh(fresh, before))
        throw const DeviceFailure('invalid_snapshot');
      operationMessage =
          'Acknowledged by ${demo ? 'demo device' : 'watch'}. Status refreshed.';
      _record('acknowledged');
      return true;
    } catch (e) {
      if (epoch == _epoch) {
        operationMessage = describeFailure(
          e is DeviceFailure ? e.code : 'operation_failed',
        );
        _record('operation_failed');
      }
      return false;
    } finally {
      if (epoch == _epoch) {
        busy = false;
        _notify();
      }
    }
  }

  void navigate(int index) {
    page = index;
    operationMessage = null;
    _notify();
    if (index == 3 && supports('settings.get'))
      unawaited(execute('settings.get'));
    if (index == 4 && supports('apps.list')) unawaited(execute('apps.list'));
  }

  void finishSetup() {
    setupDone = true;
    _notify();
  }

  String exportDiagnostics() => const JsonEncoder.withIndent('  ').convert({
    'product': 'WristFoundry Companion', 'protocol': 1, 'demo': demo,
    'connection': stage.name, 'firmware': identity['firmware'],
    // Allowlist only: no raw logs, SSIDs, IPs, device IDs, ports, params or passwords.
    'device': {
      for (final k in [
        'battery_percent',
        'charging',
        'rtc_online',
        'rtc_valid',
        'wifi_state',
        'sync_state',
        'persistence',
      ])
        k: snapshot[k],
    },
    'events': List<String>.of(_audit),
  });
  @override
  void dispose() {
    _disposed = true;
    ++_epoch;
    _watcher?.cancel();
    unawaited(_events?.cancel());
    unawaited(_client?.close());
    super.dispose();
  }
}

String describeFailure(String? code) => switch (code) {
  'access_denied' =>
    'Port access denied or in use. Close serial monitors and check Windows permissions, then retry.',
  'port_in_use' => 'Port is busy. Close the other application, then retry.',
  'timeout' =>
    'The watch did not reply in time. Check the data cable and compatible firmware, then retry.',
  'wrong_device' || 'identity_changed' =>
    'This port did not identify as the selected WristFoundry. Select the correct watch.',
  'unsupported_version' || 'incompatible_firmware' =>
    'Firmware protocol is incompatible. See docs/COMPANION.md for the manual upgrade route.',
  'busy' =>
    'The watch service is busy. Let scanning or synchronization finish, then retry.',
  'display_asleep' || 'ui_unavailable' =>
    'Wake the watch with its power button, then retry. USB actions do not override its display-off power policy.',
  'rtc_write_failed' || 'rtc_or_storage_failed' =>
    'The watch could not update its RTC or storage. Time was not confirmed. Check device diagnostics.',
  'not_configured' => 'Configure a Wi-Fi network on the watch first.',
  'discovery_failed' =>
    'USB discovery is unavailable. Check the Windows runner build and refresh.',
  'disconnected' =>
    'The watch was disconnected. Reconnect the same watch and retry.',
  'invalid_argument' =>
    'The watch rejected this value. Check the entered settings.',
  'settings_conflict' => 'Settings changed on the watch. Reload settings and review your changes.',
  'storage_unavailable' => 'Settings were not saved. Watch preference storage is unavailable.',
  _ =>
    'The operation was not confirmed. Check the cable and firmware, then retry.',
};
