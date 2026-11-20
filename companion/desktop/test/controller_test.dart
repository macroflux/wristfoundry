import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:wristfoundry_companion/controller.dart';
import 'package:wristfoundry_companion/demo.dart';
import 'package:wristfoundry_companion/protocol.dart';
import 'package:wristfoundry_companion/transport.dart';

const watchA=Candidate('COM3','USB watch A','usb-a',true);
const watchB=Candidate('COM7','USB watch B','usb-b',true);
class Discovery implements DeviceDiscovery {
  List<Candidate> ports=[];
  DeviceFailure? error;
  Transport Function()? factory;
  final opened=<String>[];
  @override
  Future<List<Candidate>> enumerate() async=>ports;
  @override
  Future<Transport> open(Candidate c) async {
    opened.add(c.port);
    if(error!=null) throw error!;
    return factory?.call() ?? DemoTransport();
  }
}
class ChangedReply extends DemoTransport {
  ChangedReply(this.change);
  final Map<String,dynamic> Function(Map<String,dynamic>) change;
  @override
  Stream<Uint8List> get bytes=>super.bytes.map((bytes){
    final q=jsonDecode(utf8.decode(bytes).substring(protocolPrefix.length)) as Map<String,dynamic>;
    return Uint8List.fromList(utf8.encode('$protocolPrefix${jsonEncode(change(q))}\n'));
  });
}
void main(){
  test('launch with no watch, hotplug one, multiple require selection',()async{
    final d=Discovery(),c=CompanionController(Discovery());c.dispose();
    final model=CompanionController(d);
    await model.refresh();expect(model.stage,ConnectionStage.idle);expect(d.opened,isEmpty);
    d.ports=[watchA,watchB];await model.refresh();expect(model.selected,isNull);
    d.ports=[watchA];await model.refresh();expect(model.selected,watchA);expect(d.opened,isEmpty);
    model.dispose();
  });
  test('identified snapshot, acknowledged configuration, unplug clears stale values',()async{
    final d=Discovery()..ports=[watchA];final c=CompanionController(d);
    await c.refresh();await c.connect();expect(c.ready,true);
    expect(await c.execute('wifi.configure',{'ssid':'test','password':'secret-value'}),true);
    expect(c.snapshot['ssid'],'test');expect(c.exportDiagnostics(),isNot(contains('secret-value')));
    expect(c.exportDiagnostics(),isNot(contains('"ssid"')));
    d.ports=[];await c.refresh();expect(c.stage,ConnectionStage.disconnected);expect(c.snapshot,isEmpty);
    expect(c.supports('time.set'),false);c.dispose();
  });
  test('busy port reports actionable error; retry succeeds',()async{
    final d=Discovery()..ports=[watchA]..error=const DeviceFailure('port_in_use');final c=CompanionController(d);
    await c.refresh();await c.connect();expect(c.errorCode,'port_in_use');expect(c.snapshot,isEmpty);
    d.error=null;await c.connect();expect(c.ready,true);c.dispose();
  });
  test('wrong serial product is rejected',()async{
    final d=Discovery()..ports=[watchA]..factory=()=>ChangedReply((q){
      if(q['result'] is Map && q['result']['product']!=null) q['result']['product']='Another device';return q;
    });final c=CompanionController(d);await c.refresh();await c.connect();
    expect(c.ready,false);expect(c.errorCode,'wrong_device');c.dispose();
  });
  test('unsupported firmware capability rejects dashboard',()async{
    final d=Discovery()..ports=[watchA]..factory=()=>ChangedReply((q){
      if(q['result'] is Map && q['result']['capabilities']!=null) q['result']['capabilities']=[];return q;
    });final c=CompanionController(d);await c.refresh();await c.connect();
    expect(c.errorCode,'incompatible_firmware');c.dispose();
  });
  test('unsupported envelope version shows incompatible firmware error',()async{
    final d=Discovery()..ports=[watchA]..factory=()=>ChangedReply((q){q['v']=2;return q;});
    final c=CompanionController(d);await c.refresh();await c.connect();
    expect(c.stage,ConnectionStage.error);expect(c.errorCode,'unsupported_version');c.dispose();
  });
  test('stale snapshots rejected; unsupported writes never sent',()async{
    final d=Discovery()..ports=[watchA]..factory=()=>ChangedReply((q){
      if(q['result'] is Map && q['result']['sequence']!=null) q['result']['sequence']=1;return q;
    });final c=CompanionController(d);await c.refresh();await c.connect();
    expect(await c.execute('brightness.set'),false);
    expect(await c.execute('time.set',{'epoch_utc':1795185000}),false);
    expect(c.operationMessage,contains('not confirmed'));c.dispose();
  });
  test('removal during save cancels operation and clears snapshot',()async{
    final d=Discovery()..ports=[watchA];final c=CompanionController(d);
    await c.refresh();await c.connect();final save=c.execute('wifi.configure',{'ssid':'test','password':'do-not-retain'});
    await c.disconnect(lost:true);expect(await save,false);expect(c.snapshot,isEmpty);expect(c.busy,false);c.dispose();
  });
  test('reconnect only to selected USB identity and reject changed watch identity',()async{
    final d=Discovery()..ports=[watchA];final c=CompanionController(d);
    await c.refresh();await c.connect();c.page=2;
    await c.disconnect(lost:true);c.autoReconnect=true;d.ports=[watchB];
    await c.refresh();expect(c.ready,false);expect(d.opened,['COM3']);
    d.ports=[watchA];await c.refresh();expect(c.ready,true);expect(c.page,2);
    await c.disconnect(lost:true);c.autoReconnect=true;
    d.factory=()=>ChangedReply((q){if(q['result'] is Map && q['result']['device_id']!=null)q['result']['device_id']='replacement';return q;});
    await c.refresh();expect(c.ready,false);expect(c.errorCode,'identity_changed');c.dispose();
  });
  test('cancel prevents late identification from opening dashboard',()async{
    final d=Discovery()..ports=[watchA];final c=CompanionController(d);
    await c.refresh();final connect=c.connect();await Future<void>.delayed(Duration.zero);
    await c.disconnect();await connect;expect(c.ready,false);expect(c.snapshot,isEmpty);c.dispose();
  });
  test('RTC failure is not reported as success',()async{
    final d=Discovery()..ports=[watchA]..factory=()=>ChangedReply((q){
      if(q['id']=='3'){q.remove('result');q['error']={'code':'rtc_write_failed'};}return q;
    });final c=CompanionController(d);await c.refresh();await c.connect();
    expect(await c.execute('time.set',{'epoch_utc':1795185000}),false);
    expect(c.operationMessage,contains('not confirmed'));c.dispose();
  });
}
