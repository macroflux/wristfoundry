import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:wristfoundry_companion/protocol.dart';

class Wire implements Transport {
  final input = StreamController<Uint8List>();
  final writes = <Uint8List>[];
  @override
  Stream<Uint8List> get bytes => input.stream;
  @override
  Future<void> write(Uint8List bytes) async { writes.add(Uint8List.fromList(bytes)); }
  void send(Object message) => input.add(Uint8List.fromList(utf8.encode('$protocolPrefix${jsonEncode(message)}\n')));
  @override
  Future<void> close() => input.close();
}
void main() {
  test('fragmented, coalesced, console and injected log text', () {
    final d = FrameDecoder();
    expect(d.add(utf8.encode('# {"v":1,"id":"1"}\n\x1eWF')),isEmpty);
    expect(d.add(utf8.encode('1 {"v":1,"id":"1","result":{}}\n$protocolPrefix{"v":1,"event":"device.status"}\n')).length,2);
  });
  test('malformed, oversized and truncated frames recover at next RS', () {
    final d = FrameDecoder();
    expect(d.add(utf8.encode('$protocolPrefix{bad}\n$protocolPrefix${'x'*9000}\n')),isEmpty);
    expect(d.add(utf8.encode('${protocolPrefix}truncated$protocolPrefix{"v":1,"id":"2"}\n')).single['id'],'2');
  });
  test('mismatched IDs do not complete requests; acknowledgement does', () async {
    final wire=Wire(), client=ProtocolClient(Wire());
    await client.close();
    final p=ProtocolClient(wire);
    var done=false;
    final request=p.request('hello').then((value){done=true;return value;});
    wire.send({'v':1,'id':'wrong','result':{}});
    await Future<void>.delayed(Duration.zero);
    expect(done,false);
    wire.send({'v':1,'id':'1','result':{'product':'WristFoundry'}});
    expect((await request)['product'],'WristFoundry');
    await p.close();
  });
  test('request timeout and late reply', () async {
    final wire=Wire();final p=ProtocolClient(wire,timeout:const Duration(milliseconds:10));
    await expectLater(p.request('hello'),throwsA(isA<DeviceFailure>().having((e)=>e.code,'code','timeout')));
    wire.send({'v':1,'id':'1','result':{}});
    await p.close();
  });
  test('unsupported version fails outstanding operations', () async {
    final wire=Wire();final p=ProtocolClient(wire);
    final pending=p.request('hello');
    final check=expectLater(pending,throwsA(isA<DeviceFailure>().having((e)=>e.code,'code','unsupported_version')));
    wire.send({'v':2,'id':'1','result':{}});
    await check;await p.close();
  });
  test('unplug fails pending save and enforces pending bound', () async {
    final wire=Wire();final p=ProtocolClient(wire);
    final checks=List.generate(4,(_)=>expectLater(p.request('wifi.configure',{'password':'test-secret'}),throwsA(isA<DeviceFailure>())));
    await expectLater(p.request('hello'),throwsA(isA<DeviceFailure>().having((e)=>e.code,'code','busy')));
    p.fail(const DeviceFailure('disconnected'));
    await Future.wait(checks);await p.close();
  });
  test('UTF-8 size bound rejects request without writing', () async {
    final wire=Wire();final p=ProtocolClient(wire);
    await expectLater(p.request('wifi.configure',{'ssid':'🧪'*400}),throwsA(isA<DeviceFailure>()));
    expect(wire.writes,isEmpty);await p.close();
  });
}
