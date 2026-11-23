import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:wristfoundry_companion/controller.dart';
import 'package:wristfoundry_companion/mobile_app.dart';
import 'package:wristfoundry_companion/settings_sections.dart';
import 'controller_test.dart' show Discovery;

void main() {
  testWidgets('settings opens cards before the power form', (tester) async {
    final c = CompanionController(Discovery());
    await tester.runAsync(() async {
      await c.connect(tryDemo: true);
      await c.execute('settings.get');
    });
    var wifi = false, time = false;
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: SingleChildScrollView(
            child: SettingsSections(
              controller: c,
              onWifi: () => wifi = true,
              onTime: () => time = true,
            ),
          ),
        ),
      ),
    );
    expect(find.text('Choose a settings section'), findsOneWidget);
    expect(find.text('Save to watch'), findsNothing);
    await tester.tap(find.text('Wi-Fi'));
    expect(wifi, true);
    await tester.tap(find.text('Time & timezone'));
    expect(time, true);
    await tester.tap(find.text('Power & display'));
    await tester.pump();
    expect(find.text('Save to watch'), findsOneWidget);
    await tester.tap(find.text('All settings'));
    await tester.pump();
    expect(find.text('Phone connection'), findsOneWidget);
    await tester.tap(find.text('Phone connection'));
    await tester.pump();
    // Legacy/demo capabilities must not expose working-looking BLE setters.
    expect(
      tester
          .widget<OutlinedButton>(
            find.widgetWithText(OutlinedButton, 'Discover for 60 seconds'),
          )
          .onPressed,
      isNull,
    );
    expect(
      tester
          .widget<OutlinedButton>(
            find.widgetWithText(OutlinedButton, 'Stop phone connection'),
          )
          .onPressed,
      isNull,
    );
    await tester.pumpWidget(const SizedBox());
    c.dispose();
  });

  testWidgets('phone settings and time fit a narrow Android screen', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(360, 800);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final c = CompanionController(Discovery());
    await tester.runAsync(() async {
      await c.connect(tryDemo: true);
      await c.execute('settings.get');
    });
    await tester.pumpWidget(MobileCompanionApp(controller: c));
    await tester.tap(find.text('Settings'));
    await tester.pumpAndSettle();
    expect(find.text('Choose a settings section'), findsOneWidget);
    await tester.tap(find.text('Time & timezone'));
    await tester.pumpAndSettle();
    expect(find.text('Sync from this phone'), findsOneWidget);
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
  });
}
