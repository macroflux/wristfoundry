import 'dart:async';
import 'package:flutter/services.dart';
import 'protocol.dart';

class Candidate {
  const Candidate(this.port, this.label, this.usbId, this.candidate);
  final String port;
  final String label;
  final String usbId;
  final bool candidate;
}

abstract interface class DeviceDiscovery {
  Future<List<Candidate>> enumerate();
  Future<Transport> open(Candidate device);
}

/// Native Windows USB/COM bridge uses Win32 + SetupAPI, shipped in the runner.
/// No external serial DLL, subprocess, arbitrary port probing, or reset pulses.
class WindowsDiscovery implements DeviceDiscovery {
  static const channel = MethodChannel('io.wristfoundry/usb');
  @override
  Future<List<Candidate>> enumerate() async {
    final ports = await channel.invokeListMethod<dynamic>('enumerate') ?? [];
    return ports
        .map(
          (p) => Candidate(
            p['port'] as String,
            p['label'] as String,
            p['usb_id'] as String,
            p['candidate'] as bool,
          ),
        )
        .toList();
  }

  @override
  Future<Transport> open(Candidate device) async {
    try {
      await channel.invokeMethod<void>('open', {'port': device.port});
      return WindowsTransport();
    } on PlatformException catch (e) {
      throw DeviceFailure(e.code);
    }
  }
}

class WindowsTransport implements Transport {
  WindowsTransport() {
    _timer = Timer.periodic(const Duration(milliseconds: 100), (_) => _read());
  }
  final _bytes = StreamController<Uint8List>();
  late final Timer _timer;
  bool _reading = false;
  bool _closed = false;
  @override
  Stream<Uint8List> get bytes => _bytes.stream;
  Future<void> _read() async {
    if (_reading || _closed) return;
    _reading = true;
    try {
      final data = await WindowsDiscovery.channel.invokeMethod<Uint8List>(
        'read',
      );
      if (!_closed && data != null && data.isNotEmpty) _bytes.add(data);
    } catch (_) {
      if (!_closed) _bytes.addError(const DeviceFailure('disconnected'));
    } finally {
      _reading = false;
    }
  }

  @override
  Future<void> write(Uint8List bytes) async {
    if (_closed) throw const DeviceFailure('disconnected');
    await WindowsDiscovery.channel.invokeMethod<void>('write', bytes);
  }

  @override
  Future<void> close() async {
    if (_closed) return;
    _closed = true;
    _timer.cancel();
    await WindowsDiscovery.channel.invokeMethod<void>('close');
    await _bytes.close();
  }
}
