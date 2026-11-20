import 'dart:async';
import 'dart:convert';
import 'dart:math';
import 'dart:typed_data';

const protocolPrefix = '\x1eWF1 ';
const requestMax = 1024;
const responseMax = 8192;

class DeviceFailure implements Exception {
  const DeviceFailure(this.code);
  final String code;
  @override
  String toString() => code;
}

abstract interface class Transport {
  Stream<Uint8List> get bytes;
  Future<void> write(Uint8List bytes);
  Future<void> close();
}

/// A bounded byte parser. Console output is discarded, never exported.
/// Resynchronizes at RS after truncated frames and drops oversized tails.
class FrameDecoder {
  final List<int> _buffer = [];
  bool _overflow = false;

  List<Map<String, dynamic>> add(List<int> bytes) {
    final frames = <Map<String, dynamic>>[];
    for (final byte in bytes) {
      if (byte == 30) {
        _buffer.clear();
        _overflow = false;
      }
      if (byte == 10) {
        if (!_overflow && _buffer.isNotEmpty && _buffer.first == 30) {
          try {
            final text = utf8.decode(_buffer);
            if (text.startsWith(protocolPrefix)) {
              final value = jsonDecode(text.substring(protocolPrefix.length));
              if (value is Map<String, dynamic>) frames.add(value);
            }
          } on FormatException {
            /* Invalid frames are never treated as replies. */
          }
        }
        _buffer.clear();
        _overflow = false;
      } else if (!_overflow) {
        if (_buffer.length >= responseMax) {
          _overflow = true;
          _buffer.clear();
        } else {
          _buffer.add(byte);
        }
      }
    }
    return frames;
  }
}

class ProtocolClient {
  ProtocolClient(this.transport, {this.timeout = const Duration(seconds: 5)}) {
    _subscription = transport.bytes.listen(
      _receive,
      onError: (_) => fail(const DeviceFailure('disconnected')),
      onDone: () => fail(const DeviceFailure('disconnected')),
    );
  }
  final Transport transport;
  final Duration timeout;
  final _decoder = FrameDecoder();
  final _pending = <String, Completer<dynamic>>{};
  final _events = StreamController<Map<String, dynamic>>.broadcast();
  late final StreamSubscription<Uint8List> _subscription;
  int _counter = 0;
  final String _session = '${DateTime.now().microsecondsSinceEpoch.toRadixString(16)}-${Random.secure().nextInt(0x100000000).toRadixString(16)}';
  bool _closed = false;
  Stream<Map<String, dynamic>> get events => _events.stream;

  Future<dynamic> request(
    String command, [
    Map<String, dynamic> params = const {},
  ]) async {
    if (_closed) throw const DeviceFailure('disconnected');
    if (_pending.length >= 4) throw const DeviceFailure('busy');
    final id = '$_session-${++_counter}';
    final wire = Uint8List.fromList(
      utf8.encode(
        '$protocolPrefix${jsonEncode({'v': 1, 'id': id, 'cmd': command, 'params': params})}\n',
      ),
    );
    if (wire.length - 1 > requestMax)
      throw const DeviceFailure('frame_too_large');
    final completer = Completer<dynamic>();
    _pending[id] = completer;
    // Attach the timeout before writing so unplug during write is handled.
    final reply = completer.future.timeout(
      timeout,
      onTimeout: () => throw const DeviceFailure('timeout'),
    );
    unawaited(
      transport
          .write(wire)
          .catchError((Object _) {
            if (!completer.isCompleted)
              completer.completeError(const DeviceFailure('write_failed'));
          })
          .whenComplete(() => wire.fillRange(0, wire.length, 0)),
    );
    try {
      return await reply;
    } finally {
      _pending.remove(id);
    }
  }

  void _receive(Uint8List bytes) {
    for (final message in _decoder.add(bytes)) {
      if (message['v'] != 1) {
        fail(const DeviceFailure('unsupported_version'));
        return;
      }
      if (message['event'] is String) {
        _events.add(message);
        continue;
      }
      final completer = _pending[message['id']];
      if (completer == null || completer.isCompleted) continue;
      if (message['error'] is Map) {
        final code = message['error']['code'];
        // Only bounded error codes are retained; never arbitrary device text.
        completer.completeError(
          DeviceFailure(
            code is String && RegExp(r'^[a-z_]{1,48}$').hasMatch(code)
                ? code
                : 'device_error',
          ),
        );
      } else if (message.containsKey('result')) {
        completer.complete(message['result']);
      }
    }
  }

  void fail(DeviceFailure error) {
    if (_closed) return;
    _closed = true;
    for (final p in _pending.values) {
      if (!p.isCompleted) p.completeError(error);
    }
    _events.add({'event': 'connection.lost', 'code': error.code});
  }

  Future<void> close() async {
    fail(const DeviceFailure('disconnected'));
    await _subscription.cancel();
    await transport.close();
    await _events.close();
  }
}
