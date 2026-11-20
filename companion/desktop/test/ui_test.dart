import 'dart:io';
import 'dart:ui' as ui;
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:wristfoundry_companion/controller.dart';
import 'package:wristfoundry_companion/main.dart';
import 'controller_test.dart' show Discovery,watchA,watchB;

Future<void> capture(WidgetTester tester,String name)async{
  await tester.pumpAndSettle();
  final boundary=tester.renderObject<RenderRepaintBoundary>(find.byKey(captureKey));
  await tester.runAsync(()async{
    final image=await boundary.toImage(pixelRatio:1);
    final bytes=await image.toByteData(format:ui.ImageByteFormat.png);
    final directory=Directory('review-captures')..createSync(recursive:true);
    File('${directory.path}/$name.png').writeAsBytesSync(bytes!.buffer.asUint8List());
    image.dispose();
  });
}
void main(){
  testWidgets('welcome, discovery picker, demo dashboard and acknowledged forms render',(tester)async{
    tester.view.physicalSize=const Size(1440,1000);tester.view.devicePixelRatio=1;
    addTearDown(tester.view.resetPhysicalSize);addTearDown(tester.view.resetDevicePixelRatio);
    final discovery=Discovery(),c=CompanionController(Discovery());c.dispose();
    final model=CompanionController(discovery);
    await tester.pumpWidget(CompanionApp(controller:model,startDiscovery:false));
    expect(find.text('Connect your WristFoundry'),findsOneWidget);
    expect(find.text('Try demo'),findsOneWidget);
    await capture(tester,'01-welcome');
    discovery.ports=[watchA,watchB];await model.refresh();await tester.pumpAndSettle();
    await capture(tester,'02-multiple-devices');
    await tester.tap(find.text('Try demo'));await tester.pumpAndSettle(const Duration(milliseconds:250));
    expect(find.textContaining('DEMO MODE'),findsOneWidget);
    expect(find.text('Your watch, in view.'),findsOneWidget);
    await capture(tester,'03-demo-dashboard');
    model.navigate(1);await tester.pumpAndSettle();
    await tester.enterText(find.byType(TextField).at(0),'Workshop (demo)');
    await tester.enterText(find.byType(TextField).at(1),'password-not-exported');
    await tester.tap(find.text('Save to watch'));await tester.pumpAndSettle(const Duration(milliseconds:250));
    expect(model.snapshot['ssid'],'Workshop (demo)');
    expect(model.exportDiagnostics(),isNot(contains('password-not-exported')));
    await capture(tester,'04-demo-wifi');
    model.navigate(2);await tester.pumpAndSettle();await capture(tester,'05-demo-time');
    await model.disconnect(lost:true);await tester.pumpAndSettle();
    expect(find.textContaining('Connection lost.'),findsOneWidget);
    await capture(tester,'06-disconnected');
    expect(tester.takeException(),isNull);
  });
}
