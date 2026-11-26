import 'dart:io';
import 'dart:ui' as ui;
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:wristfoundry_companion/controller.dart';
import 'package:wristfoundry_companion/mobile_app.dart';
import 'package:wristfoundry_companion/settings_sections.dart';
import 'package:wristfoundry_companion/watch_preview.dart';
import 'controller_test.dart' show Discovery;

void main() {
  testWidgets('welcome graphic and actions fit on one phone screen', (
    tester,
  ) async {
    await tester.runAsync(() async {
      final sdk = Platform.environment['FLUTTER_ROOT'];
      if (sdk != null) {
        for (final entry in {
          'Roboto': 'Roboto-Regular.ttf',
          'MaterialIcons': 'MaterialIcons-Regular.otf',
        }.entries) {
          final file = File(
            '$sdk/bin/cache/artifacts/material_fonts/${entry.value}',
          );
          if (file.existsSync()) {
            final loader = FontLoader(entry.key)
              ..addFont(
                Future.value(ByteData.sublistView(file.readAsBytesSync())),
              );
            await loader.load();
          }
        }
      }
    });
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    for (final size in [const Size(384, 832), const Size(360, 640)]) {
      tester.view.physicalSize = size;
      final c = CompanionController(Discovery());
      const capture = ValueKey('mobile-welcome-capture');
      await tester.pumpWidget(
        RepaintBoundary(
          key: capture,
          child: MobileCompanionApp(controller: c),
        ),
      );
      await tester.pumpAndSettle();
      expect(find.byType(WatchPreview), findsOneWidget);
      final scrollable = tester.state<ScrollableState>(
        find
            .descendant(
              of: find.byKey(const ValueKey('phone-welcome-scroll')),
              matching: find.byType(Scrollable),
            )
            .first,
      );
      expect(scrollable.position.maxScrollExtent, 0);
      expect(find.text('Scan for watches').hitTestable(), findsOneWidget);
      expect(find.text('Try demo').hitTestable(), findsOneWidget);
      expect(tester.takeException(), isNull);
      if (size.width == 384) {
        final boundary = tester.renderObject<RenderRepaintBoundary>(
          find.byKey(capture),
        );
        final image = boundary.toImageSync();
        await tester.runAsync(() async {
          final bytes = await image.toByteData(format: ui.ImageByteFormat.png);
          Directory('review-captures').createSync(recursive: true);
          File(
            'review-captures/10-android-welcome.png',
          ).writeAsBytesSync(bytes!.buffer.asUint8List());
          image.dispose();
        });
      }
      await tester.pumpWidget(const SizedBox());
    }
  });
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
