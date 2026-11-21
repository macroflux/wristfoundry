import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';
import 'protocol.dart';

class DemoTransport implements Transport {
  final _bytes = StreamController<Uint8List>();
  bool _closed = false;
  int _sequence = 0;
  String zone = 'UTC0';
  String wifi = 'OFF';
  String ssid = '';
  int settingsRevision = 1;
  Map<String, dynamic> preferences = {
    'schema_version': 1, 'revision': 1, 'storage_ready': true, 'saved': false,
    'dim_seconds': 10, 'off_seconds': 25, 'usb_display': 'stay_on',
    'radio_policy': 'stop', 'radio_grace_seconds': 30,
    'brightness_set': false, 'sleep_set': true,
  };
  DateTime clock = DateTime.utc(2026, 11, 20, 14, 32);
  final caps = [
    'device.get',
    'wifi.scan',
    'wifi.configure',
    'wifi.connect',
    'wifi.disconnect',
    'time.set',
    'timezone.set',
    'ntp.sync',
    'ntp.cancel',
    'settings.get',
    'settings.set',
    'settings.reset',
    'apps.list',
    'apps.open',
    'apps.home',
    'diagnostics.get',
  ];
  Map<String, dynamic> get snapshot => {
    'sequence': ++_sequence,
    'settings_revision': settingsRevision,
    'battery_percent': 82,
    'charging': true,
    'rtc_local': clock.toIso8601String().replaceAll('T', ' ').substring(0, 19),
    'rtc_valid': true,
    'rtc_online': true,
    'timezone': zone,
    'wifi_state': wifi,
    'wifi_result': wifi,
    'configured': ssid.isNotEmpty,
    'persistence': 'ram_only',
    'ssid': ssid,
    'ip': wifi == 'ONLINE' ? '192.0.2.20' : '',
    'scan_busy': false,
    'scan_generation': 1,
    'scan_result': 'COMPLETE',
    'sync_busy': false,
    'sync_state': 'IDLE',
    'sync_status': 'Simulated time service',
    'sync_detail': 'Demo only; no hardware clock was changed',
    'time_source': 'DEMO',
    'networks': [
      {'ssid': 'Workshop (demo)', 'rssi': -48, 'secured': true},
      {'ssid': 'Guest (demo)', 'rssi': -67, 'secured': false},
    ],
  };
  @override
  Stream<Uint8List> get bytes => _bytes.stream;
  @override
  Future<void> write(Uint8List bytes) async {
    final q =
        jsonDecode(utf8.decode(bytes).substring(protocolPrefix.length))
            as Map<String, dynamic>;
    await Future<void>.delayed(const Duration(milliseconds: 80));
    if (_closed) return;
    final p = q['params'] as Map<String, dynamic>;
    dynamic result = {'status': 'accepted'};
    String? error;
    switch (q['cmd']) {
      case 'hello':
        result = {
          'product': 'WristFoundry',
          'device_id': 'demo-watch',
          'model': 'Waveshare ESP32-S3 AMOLED 2.06',
          'firmware': '0.2.0 · demo',
          'capabilities': caps,
          'app_packages': false,
        };
      case 'device.get' || 'diagnostics.get':
        result = snapshot;
      case 'wifi.configure':
        ssid = p['ssid'] as String;
      case 'wifi.connect':
        if (ssid.isEmpty) {
          error = 'not_configured';
        } else {
          wifi = 'ONLINE';
        }
      case 'wifi.disconnect':
        wifi = 'OFF';
      case 'timezone.set':
        zone = p['timezone'] as String;
      case 'time.set':
        clock = DateTime.fromMillisecondsSinceEpoch(
          (p['epoch_utc'] as int) * 1000,
          isUtc: true,
        );
      case 'settings.get':
        result = Map<String, dynamic>.of(preferences);
      case 'settings.set' || 'settings.reset':
        final reset = q['cmd'] == 'settings.reset';
        final d = reset ? 10 : p['dim_seconds'];
        final o = reset ? 25 : p['off_seconds'];
        if (p['expected_revision'] != settingsRevision) {
          error = 'settings_conflict';
        } else if (d is! int || o is! int || d < 5 || d > 300 || o < 10 || o > 600 || o <= d ||
            (!reset && (!['stay_on', 'timeout'].contains(p['usb_display']) ||
              !['stop', 'finish_bounded'].contains(p['radio_policy'])))) {
          error = 'invalid_argument';
        } else {
          preferences = {...preferences, 'dim_seconds': d, 'off_seconds': o,
            'usb_display': reset ? 'stay_on' : p['usb_display'],
            'radio_policy': reset ? 'stop' : p['radio_policy'],
            'saved': true, 'revision': ++settingsRevision};
          result = Map<String, dynamic>.of(preferences);
        }
      case 'apps.list':
        result = [
          {
            'id': 'settings',
            'name': 'Settings',
            'icon': {'type': 'builtin', 'name': 'settings'}, 'initials': 'SE',
            'description': 'Shared network and clock controls',
          },
          {
            'id': 'ble_radar',
            'name': 'BLE Radar',
            'icon': {'type': 'builtin', 'name': 'radar'}, 'initials': 'BR',
            'description': 'Bounded advertisement scanner',
          },
          {
            'id': 'system',
            'name': 'System',
            'icon': {'type': 'builtin', 'name': 'system'}, 'initials': 'SY',
            'description': 'Board health and diagnostics',
          },
        ];
      case 'wifi.scan' ||
          'ntp.sync' ||
          'ntp.cancel' ||
          'apps.open' ||
          'apps.home':
        break;
      default:
        error = 'unsupported_command';
    }
    _bytes.add(
      Uint8List.fromList(
        utf8.encode(
          '$protocolPrefix${jsonEncode({
            'v': 1,
            'id': q['id'],
            if (error != null) 'error': {'code': error} else 'result': result,
          })}\n',
        ),
      ),
    );
  }

  @override
  Future<void> close() async {
    _closed = true;
    await _bytes.close();
  }
}
