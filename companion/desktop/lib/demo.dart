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
    'apps.list',
    'apps.open',
    'apps.home',
    'diagnostics.get',
  ];
  Map<String, dynamic> get snapshot => {
    'sequence': ++_sequence,
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
          'firmware': '0.1.0 · demo',
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
        result = {
          'policy': '10s dim / 25s off; radios stop at display off',
          'brightness_set': false,
          'sleep_set': false,
        };
      case 'apps.list':
        result = [
          {
            'id': 'settings',
            'name': 'Settings',
            'description': 'Shared network and clock controls',
          },
          {
            'id': 'ble_radar',
            'name': 'BLE Radar',
            'description': 'Bounded advertisement scanner',
          },
          {
            'id': 'system',
            'name': 'System',
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
