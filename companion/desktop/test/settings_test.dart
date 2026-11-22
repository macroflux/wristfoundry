import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:wristfoundry_companion/app_icon.dart';
import 'package:wristfoundry_companion/controller.dart';
import 'package:wristfoundry_companion/settings_editor.dart';
import 'controller_test.dart' show Discovery;

void main() {
  test('settings save, stale revision conflict and reset are acknowledged', () async {
    final c = CompanionController(Discovery());
    await c.connect(tryDemo: true);
    expect(await c.execute('settings.get'), true);
    final revision = c.settings['revision'];
    expect(await c.execute('settings.set', {
      'expected_revision': revision, 'dim_seconds': 15, 'off_seconds': 60,
      'usb_display': 'timeout', 'radio_policy': 'finish_bounded',
    }), true);
    expect(c.settings['dim_seconds'], 15);
    expect(c.settings['saved'], true);
    expect(await c.execute('settings.reset', {'expected_revision': revision}), false);
    expect(c.operationMessage, contains('changed on the watch'));
    expect(c.settings['dim_seconds'], 15);
    expect(await c.execute('settings.reset', {'expected_revision': c.settings['revision']}), true);
    expect(c.settings['dim_seconds'], 10);
    expect(c.settings['usb_display'], 'stay_on');
    c.dispose();
  });

  testWidgets('draft survives refresh and invalid timeouts cannot be saved', (tester) async {
    final c = CompanionController(Discovery());
    // Demo transport uses real delayed replies. Initialize outside the widget
    // test's fake clock before rendering; awaiting those timers directly hangs.
    await tester.runAsync(() async {
      await c.connect(tryDemo: true);
      await c.execute('settings.get');
    });
    Future<void> render() => tester.pumpWidget(MaterialApp(home: Scaffold(
      body: SingleChildScrollView(child: SettingsEditor(controller: c)))));
    await render();
    await tester.enterText(find.byType(TextField).first, '99');
    c.settings = {...c.settings, 'revision': 7, 'dim_seconds': 20};
    await render();
    expect(find.text('99'), findsOneWidget);
    expect(find.textContaining('Settings changed on the watch'), findsOneWidget);
    await tester.ensureVisible(find.text('Save to watch'));
    await tester.tap(find.text('Save to watch'));
    await tester.pump();
    expect(find.textContaining('Dim must be'), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
    c.dispose();
  });

  testWidgets('builtin icon and unknown-icon initials fallback', (tester) async {
    await tester.pumpWidget(MaterialApp(home: Column(children: [
      appIcon({'name': 'Settings', 'icon': {'type': 'builtin', 'name': 'settings'}}),
      appIcon({'name': 'Experimental', 'initials': 'EX', 'icon': {'type': 'builtin', 'name': 'future'}}),
      appIcon({'name': 'Legacy'}),
    ])));
    expect(find.byIcon(Icons.settings_outlined), findsOneWidget);
    expect(find.text('EX'), findsOneWidget);
    expect(find.text('Le'), findsOneWidget);
  });
}
