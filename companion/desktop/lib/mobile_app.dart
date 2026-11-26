import 'dart:async';
import 'package:flutter/material.dart';
import 'controller.dart';
import 'settings_sections.dart';
import 'timezones.dart';
import 'transport.dart';
import 'app_icon.dart';
import 'watch_preview.dart';

/// Phone layout shares the controller, contracts, settings editor and icon map.
class MobileCompanionApp extends StatefulWidget {
  const MobileCompanionApp({super.key, required this.controller});
  final CompanionController controller;
  @override
  State<MobileCompanionApp> createState() => _MobileCompanionAppState();
}

class _MobileCompanionAppState extends State<MobileCompanionApp>
    with WidgetsBindingObserver {
  CompanionController get c => widget.controller;
  final ssid = TextEditingController(), password = TextEditingController();
  final manual = TextEditingController();
  String? manualError;
  WatchZone zone = watchZones.first;
  int tab = 0;
  String? subpage;
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    c.addListener(changed);
    // Discovery is explicit, so Android permissions are requested after tapping Scan.
  }

  void changed() {
    if (!c.ready) password.clear();
    if (mounted) setState(() {});
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    // Android permission/pairing dialogs can make us inactive; only background closes.
    if (state == AppLifecycleState.paused && c.ready) unawaited(c.disconnect());
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    c.removeListener(changed);
    c.dispose();
    ssid.dispose();
    password.dispose();
    manual.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'WristFoundry Companion',
    debugShowCheckedModeBanner: false,
    theme: ThemeData(
      brightness: Brightness.dark,
      useMaterial3: true,
      scaffoldBackgroundColor: const Color(0xFF03070B),
      colorScheme: const ColorScheme.dark(
        primary: Color(0xFF56DBF4),
        surface: Color(0xFF0B1820),
      ),
    ),
    home: Listener(
      onPointerDown: (_) => unawaited(c.noteInteraction()),
      child: Scaffold(
        appBar: AppBar(
          title: const Text('WristFoundry'),
          actions: [
            if (c.ready)
              IconButton(
                tooltip: 'Disconnect',
                onPressed: () => c.disconnect(),
                icon: const Icon(Icons.link_off),
              ),
          ],
        ),
        body: SafeArea(
          child: !c.ready
              ? welcome()
              : SingleChildScrollView(
                  padding: const EdgeInsets.all(20),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      if (c.demo) const Text('DEMO · Simulated watch'),
                      if (c.operationMessage != null)
                        Padding(
                          padding: const EdgeInsets.only(bottom: 16),
                          child: Text(c.operationMessage!),
                        ),
                      if (c.busy || c.connecting)
                        const LinearProgressIndicator(),
                      ...content(),
                    ],
                  ),
                ),
        ),
        bottomNavigationBar: !c.ready
            ? null
            : NavigationBar(
                selectedIndex: tab,
                onDestinationSelected: (i) {
                  c.noteInteraction();
                  setState(() {
                    tab = i;
                    subpage = null;
                  });
                  if (i == 1) c.execute('settings.get');
                  if (i == 2) c.execute('apps.list');
                },
                destinations: const [
                  NavigationDestination(
                    icon: Icon(Icons.watch),
                    label: 'Overview',
                  ),
                  NavigationDestination(
                    icon: Icon(Icons.tune),
                    label: 'Settings',
                  ),
                  NavigationDestination(icon: Icon(Icons.apps), label: 'Apps'),
                  NavigationDestination(
                    icon: Icon(Icons.monitor_heart),
                    label: 'Diagnostics',
                  ),
                ],
              ),
      ),
    ),
  );
  Widget welcome() => LayoutBuilder(
    builder: (context, bounds) {
      final graphicHeight = (bounds.maxHeight * .25).clamp(100.0, 180.0);
      return SingleChildScrollView(
        key: const ValueKey('phone-welcome-scroll'),
        padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 16),
        child: Center(
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 420),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: connect(graphicHeight),
            ),
          ),
        ),
      );
    },
  );
  List<Widget> connect(double graphicHeight) => [
    const Text(
      'Your watch, within reach.',
      style: TextStyle(fontSize: 27, fontWeight: FontWeight.bold),
    ),
    const SizedBox(height: 8),
    SizedBox(
      height: graphicHeight,
      child: const FittedBox(
        fit: BoxFit.contain,
        child: SizedBox(
          width: 380,
          height: 235,
          child: WatchPreview(linkLabel: 'BLE / v1'),
        ),
      ),
    ),
    const SizedBox(height: 12),
    const Text(
      'On your watch: Settings → Phone connection → Discover for 60 seconds.\nScan here, then enter the watch code when Android asks.',
    ),
    const SizedBox(height: 16),
    FilledButton.icon(
      onPressed: c.connecting
          ? null
          : () {
              c.start();
              c.refresh();
            },
      icon: const Icon(Icons.bluetooth_searching),
      label: const Text('Scan for watches'),
    ),
    if (c.errorCode != null) Text(describeFailure(c.errorCode)),
    if (c.devices.isNotEmpty)
      ConstrainedBox(
        constraints: const BoxConstraints(maxHeight: 160),
        child: ListView(
          shrinkWrap: true,
          children: [
            for (final Candidate device in c.devices)
              Card(
                child: ListTile(
                  title: Text(device.label),
                  subtitle: const Text('Tap to connect'),
                  onTap: c.connecting
                      ? null
                      : () {
                          c.select(device);
                          c.connect();
                        },
                ),
              ),
          ],
        ),
      ),
    if (c.connecting)
      TextButton(onPressed: () => c.disconnect(), child: const Text('Cancel')),
    TextButton(
      onPressed: () => c.connect(tryDemo: true),
      child: const Text('Try demo'),
    ),
  ];
  List<Widget> content() {
    if (tab == 0)
      return [
        Text('${c.identity['model']}', style: const TextStyle(fontSize: 22)),
        Text(
          'Firmware ${c.identity['firmware']} · ${c.demo ? 'Demo' : 'Bluetooth connected'}',
        ),
        const SizedBox(height: 20),
        Card(
          child: ListTile(
            title: const Text('Battery'),
            trailing: Text('${c.snapshot['battery_percent']}%'),
          ),
        ),
        Card(
          child: ListTile(
            title: const Text('Wi-Fi'),
            trailing: Text('${c.snapshot['wifi_state']}'),
          ),
        ),
        const Text(
          'Watch stays awake while connected. Sessions end after five minutes without interaction.',
        ),
        OutlinedButton(
          onPressed: () => c.execute('device.get'),
          child: const Text('Refresh watch'),
        ),
      ];
    if (tab == 1 && subpage == null)
      return [
        SettingsSections(
          controller: c,
          onWifi: () => setState(() => subpage = 'wifi'),
          onTime: () => setState(() => subpage = 'time'),
        ),
      ];
    if (tab == 1)
      return [
        TextButton.icon(
          onPressed: () => setState(() => subpage = null),
          icon: const Icon(Icons.arrow_back),
          label: const Text('All settings'),
        ),
        if (subpage == 'wifi') ...[
          const Text('Wi-Fi', style: TextStyle(fontSize: 24)),
          OutlinedButton(
            onPressed: c.busy ? null : () => c.execute('wifi.scan'),
            child: const Text('Scan networks'),
          ),
          for (final network in (c.snapshot['networks'] as List? ?? const []))
            Card(
              child: ListTile(
                title: Text('${network['ssid']}'),
                subtitle: Text(
                  '${network['rssi']} dBm · ${network['secured'] == true ? 'Secured' : 'Open'}',
                ),
                onTap: () =>
                    setState(() => ssid.text = network['ssid'] as String),
              ),
            ),
          TextField(
            controller: ssid,
            decoration: const InputDecoration(labelText: 'Network name'),
          ),
          TextField(
            controller: password,
            obscureText: true,
            decoration: const InputDecoration(labelText: 'Password'),
          ),
          FilledButton(
            onPressed: c.busy
                ? null
                : () async {
                    await c.execute('wifi.configure', {
                      'ssid': ssid.text,
                      'password': password.text,
                    });
                    password.clear();
                  },
            child: const Text('Save to watch'),
          ),
          OutlinedButton(
            onPressed: c.busy ? null : () => c.execute('wifi.connect'),
            child: const Text('Connect Wi-Fi'),
          ),
          OutlinedButton(
            onPressed: c.busy ? null : () => c.execute('wifi.disconnect'),
            child: const Text('Disconnect Wi-Fi'),
          ),
        ] else ...[
          const Text('Time & timezone', style: TextStyle(fontSize: 24)),
          DropdownButtonFormField<WatchZone>(
            initialValue: zone,
            isExpanded: true,
            items: watchZones
                .map((z) => DropdownMenuItem(value: z, child: Text(z.name)))
                .toList(),
            onChanged: (value) => setState(() => zone = value!),
          ),
          FilledButton(
            onPressed: c.busy
                ? null
                : () => c.execute('timezone.set', {'timezone': zone.rule}),
            child: const Text('Apply timezone'),
          ),
          FilledButton(
            onPressed: c.busy
                ? null
                : () => c.execute('time.set', {
                    'epoch_utc':
                        DateTime.now().toUtc().millisecondsSinceEpoch ~/ 1000,
                  }),
            child: const Text('Sync from this phone'),
          ),
          OutlinedButton(
            onPressed: c.busy ? null : () => c.execute('ntp.sync'),
            child: const Text('Sync with NTP'),
          ),
          TextField(
            controller: manual,
            decoration: const InputDecoration(
              labelText: 'Manual date/time (YYYY-MM-DD HH:MM:SS)',
              helperText: 'Interpreted in the selected timezone',
            ),
          ),
          if (manualError != null) Text(manualError!),
          OutlinedButton(
            onPressed: c.busy
                ? null
                : () {
                    try {
                      final raw = manual.text.trim();
                      if (!RegExp(
                        r'^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$',
                      ).hasMatch(raw))
                        throw const FormatException('Use YYYY-MM-DD HH:MM:SS.');
                      final wall = DateTime.parse(
                        '${raw.replaceFirst(' ', 'T')}Z',
                      );
                      if (wall.toIso8601String().substring(0, 19) !=
                          raw.replaceFirst(' ', 'T'))
                        throw const FormatException(
                          'Enter a valid calendar date.',
                        );
                      final utc = zone.wallToUtc(wall);
                      if (utc.year < 2024 || utc.year > 2099)
                        throw const FormatException(
                          'Supported range is 2024–2099 UTC.',
                        );
                      setState(() => manualError = null);
                      c.execute('time.set', {
                        'epoch_utc': utc.millisecondsSinceEpoch ~/ 1000,
                      });
                    } on FormatException catch (e) {
                      setState(() => manualError = e.message);
                    }
                  },
            child: const Text('Set manual time'),
          ),
        ],
      ];
    if (tab == 2)
      return [
        const Text('Built into your firmware', style: TextStyle(fontSize: 24)),
        for (final app in c.apps)
          Card(
            child: ListTile(
              leading: appIcon(app),
              title: Text('${app['name']}'),
              subtitle: Text('${app['description']}'),
              onTap: c.busy
                  ? null
                  : () => c.execute('apps.open', {'app_id': app['id']}),
            ),
          ),
        OutlinedButton(
          onPressed: c.busy ? null : () => c.execute('apps.home'),
          child: const Text('Return watch home'),
        ),
        const Text(
          'Apps currently ship with firmware. Individual installation is unavailable.',
        ),
      ];
    return [
      const Text('Diagnostics', style: TextStyle(fontSize: 24)),
      OutlinedButton(
        onPressed: c.busy ? null : () => c.execute('diagnostics.get'),
        child: const Text('Read diagnostics'),
      ),
      SelectableText(c.exportDiagnostics()),
    ];
  }
}
