import 'dart:typed_data';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:wristfoundry_companion/protocol.dart';
import 'package:wristfoundry_companion/transport.dart';
import 'package:wristfoundry_companion/controller.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  test('BLE terminates the security probe before sending hello', () async {
    List<int>? delivered;
    Uint8List? channelBuffer;
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(AndroidBleDiscovery.channel, (call) async {
          if (call.method == 'write') {
            channelBuffer = call.arguments as Uint8List;
            delivered = List<int>.of(channelBuffer!);
          }
          return null;
        });
    final transport = AndroidBleTransport();
    final subscription = transport.bytes.listen((_) {});
    final frame = Uint8List.fromList(
      '$protocolPrefix{"v":1,"id":"h","cmd":"hello","params":{}}\n'.codeUnits,
    );
    await transport.write(frame);
    expect(
      delivered!.first,
      10,
    ); // Completes an RS-only probe from old native builds.
    expect(delivered!.sublist(1), frame);
    expect(FrameDecoder().add(delivered!).single['cmd'], 'hello');
    await transport.close();
    await subscription.cancel();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(AndroidBleDiscovery.channel, null);
  });
  test('Bluetooth timeout guidance does not refer to a USB cable', () {
    expect(describeFailure('ble_handshake_timeout'), contains('Bluetooth'));
    expect(describeFailure('ble_handshake_timeout'), isNot(contains('cable')));
    expect(describeFailure('ble_status_timeout'), contains('identified'));
  });
}
