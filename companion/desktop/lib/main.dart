import 'dart:async';
import 'package:flutter/material.dart';

import 'package:flutter/services.dart';
import 'controller.dart';
import 'timezones.dart';
import 'transport.dart';

const ink = Color(0xFF03070B), panel = Color(0xFF0B1820), raised = Color(0xFF102832);
const cyan = Color(0xFF56DBF4), text = Color(0xFFEAF2F7), muted = Color(0xFF8296A1);
const line = Color(0xFF214652), green = Color(0xFF63E6A5), amber = Color(0xFFFFCB69);
const captureKey = ValueKey('companion-capture');

void main() => runApp(CompanionApp(controller: CompanionController(WindowsDiscovery())));

class CompanionApp extends StatefulWidget {
  const CompanionApp({super.key, required this.controller, this.startDiscovery = true});
  final CompanionController controller;
  final bool startDiscovery;
  @override
  State<CompanionApp> createState() => _CompanionAppState();
}
class _CompanionAppState extends State<CompanionApp> {
  final ssid = TextEditingController(), password = TextEditingController();
  final manual = TextEditingController(text: DateTime.now().toUtc().toIso8601String().substring(0,19).replaceFirst('T',' '));
  WatchZone zone = watchZones.first;
  WatchZone manualZone = watchZones.first;
  bool showManualPorts = false;
  String? localError;
  CompanionController get c => widget.controller;
  @override
  void initState() {
    super.initState();
    c.addListener(_changed);
    if (widget.startDiscovery) c.start();
  }
  void _changed() {
    if (!c.ready) password.clear();
    if (mounted) setState(() {});
  }
  @override
  void dispose() {
    c.removeListener(_changed);
    c.dispose(); ssid.dispose(); password.dispose(); manual.dispose();
    super.dispose();
  }
  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'WristFoundry Companion', debugShowCheckedModeBanner: false,
    theme: ThemeData(brightness: Brightness.dark, useMaterial3: true,
      scaffoldBackgroundColor: ink, fontFamily: 'Segoe UI',
      colorScheme: const ColorScheme.dark(primary: cyan, surface: panel,
        onSurface: text, secondary: green, error: amber),
      dividerColor: line, textTheme: const TextTheme(
        bodyMedium: TextStyle(fontSize: 14, height: 1.5, color: text),
        bodySmall: TextStyle(fontSize: 12, height: 1.5, color: muted)),
      inputDecorationTheme: InputDecorationTheme(filled: true, fillColor: ink,
        border: OutlineInputBorder(borderRadius: BorderRadius.circular(12), borderSide: const BorderSide(color: line)),
        enabledBorder: OutlineInputBorder(borderRadius: BorderRadius.circular(12), borderSide: const BorderSide(color: line)),
        contentPadding: const EdgeInsets.all(16)),
      filledButtonTheme: FilledButtonThemeData(style: FilledButton.styleFrom(
        backgroundColor: cyan, foregroundColor: ink, padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 19),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)))),
      outlinedButtonTheme: OutlinedButtonThemeData(style: OutlinedButton.styleFrom(
        padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 18),
        side: const BorderSide(color: line), shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)))),
    ),
    home: RepaintBoundary(key: captureKey, child: Scaffold(body: SafeArea(child: Column(children: [
      _header(),
      if (c.demo) Container(width: double.infinity, color: const Color(0xFF332B19),
        padding: const EdgeInsets.symmetric(vertical: 7, horizontal: 24),
        child: const Text('DEMO MODE  /  Simulated device and telemetry. No watch is connected or changed.',
          style: TextStyle(color: amber, fontSize: 12))),
      Expanded(child: c.ready ? _dashboard() : _welcome()),
      Container(padding: const EdgeInsets.symmetric(horizontal: 28, vertical: 12),
        decoration: const BoxDecoration(border: Border(top: BorderSide(color: line))),
        child: Row(children: [const Text('WRISTFOUNDRY  /  COMPANION 0.1', style: TextStyle(color: muted, fontSize: 10, letterSpacing: 1.4)),
          const Spacer(), Text(c.ready ? 'USB PROTOCOL v1  ·  ${c.demo ? 'SIMULATED' : 'DEVICE ACKNOWLEDGED'}' : 'A SMALL SCREEN. A LARGER WORKBENCH.',
            style: const TextStyle(color: muted, fontSize: 10, letterSpacing: 1))])),
    ])))),
  );

  Widget _header() => Container(padding: const EdgeInsets.symmetric(horizontal: 28, vertical: 19),
    decoration: const BoxDecoration(border: Border(bottom: BorderSide(color: line))),
    child: Row(children: [
      Container(padding: const EdgeInsets.all(9), decoration: BoxDecoration(color: raised,
        borderRadius: BorderRadius.circular(11), border: Border.all(color: line)),
        child: const Icon(Icons.watch_outlined, color: cyan, size: 24)),
      const SizedBox(width: 13), const Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text('WristFoundry', style: TextStyle(fontWeight: FontWeight.w700, fontSize: 19, letterSpacing: -.5)),
        Text('C O M P A N I O N', style: TextStyle(color: muted, fontSize: 10)),
      ]), const Spacer(),
      _badge(c.ready ? (c.demo ? 'Demo device' : 'Connected · ${c.selected?.port}') : c.connecting ? 'Identifying device' : 'Awaiting connection',
        c.ready ? green : muted),
      if (c.ready) ...[const SizedBox(width: 16), TextButton.icon(onPressed: () => c.disconnect(),
        icon: const Icon(Icons.link_off, size: 17), label: const Text('Disconnect'))],
    ]));

  Widget _welcome() => LayoutBuilder(builder: (context, constraints) {
    final intro = Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      const Text('YOUR WATCH, AT THE WORKBENCH', style: TextStyle(color: cyan, fontSize: 11, letterSpacing: 2)),
      const SizedBox(height: 18), const Text('Small device.\nRoom to build.',
        style: TextStyle(fontSize: 44, fontWeight: FontWeight.w700, height: 1.12, letterSpacing: -1.6)),
      const SizedBox(height: 18), const Text('Set up your watch, bring its services into view,\nand keep experimenting.', style: TextStyle(color: muted, height: 1.7)),
      const SizedBox(height: 28), const SizedBox(height: 235, child: Center(child: WatchPreview())),
      const SizedBox(height: 18), Wrap(spacing: 22, runSpacing: 8, children: [
        _hint(Icons.usb, 'Tethered by USB'), _hint(Icons.memory, 'Built on your firmware'),
      ]),
    ]);
    final connect = _card(Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      _badge('01  /  CONNECT', cyan), const SizedBox(height: 25),
      const Text('Connect your WristFoundry', style: TextStyle(fontSize: 25, fontWeight: FontWeight.w600, letterSpacing: -.5)),
      const SizedBox(height: 12), const Text('Plug in your watch with a USB data cable.\nSelect a port, then connect to verify its identity.', style: TextStyle(color: muted, height: 1.6)),
      const SizedBox(height: 26),
      Row(children: [const Icon(Icons.usb, size: 17, color: cyan), const SizedBox(width: 8),
        Expanded(child: Text(c.connecting ? _stageLabel(c.stage) : c.devices.where((p) => p.candidate).isEmpty
          ? 'No candidate watches detected' : '${c.devices.where((p) => p.candidate).length} candidate device(s) detected',
          style: const TextStyle(fontSize: 12, color: muted))),
        TextButton.icon(onPressed: c.connecting ? null : c.refresh,
          icon: const Icon(Icons.refresh, size: 16), label: const Text('Refresh'))]),
      const SizedBox(height: 10),
      if (c.devices.any((p) => p.candidate || showManualPorts)) DropdownButtonFormField<String>(
        key: ValueKey('${c.selected?.port}-$showManualPorts-${c.devices.length}'),
        initialValue: c.selected?.port,
        isExpanded: true, decoration: const InputDecoration(labelText: 'Device / serial port'),
        items: c.devices.where((p) => p.candidate || showManualPorts || p.port == c.selected?.port)
          .map((p) => DropdownMenuItem(value: p.port, child: Text('${p.label}  ·  ${p.port}', overflow: TextOverflow.ellipsis))).toList(),
        onChanged: c.connecting ? null : (port) => c.select(c.devices.firstWhere((p) => p.port == port)),
      ) else _inset('Use a data-capable cable, check the USB connection, and refresh. Charge-only cables will not expose a port.'),
      SwitchListTile(contentPadding: EdgeInsets.zero, title: const Text('Show other serial ports', style: TextStyle(fontSize: 12)),
        subtitle: const Text('Manual selection only. No ports are probed.', style: TextStyle(fontSize: 11, color: muted)),
        value: showManualPorts, onChanged: c.connecting ? null : (v) => setState(() => showManualPorts = v)),
      if (c.errorCode != null) Padding(padding: const EdgeInsets.only(bottom: 16), child: _inset(describeFailure(c.errorCode), warning: true)),
      if (c.stage == ConnectionStage.disconnected) ...[
        _inset('Connection lost. Last identified watch: ${c.lastDevice?.label ?? 'WristFoundry'}. Live values have been cleared.', warning: true),
        CheckboxListTile(contentPadding: EdgeInsets.zero, value: c.autoReconnect,
          title: const Text('Reconnect to this watch automatically', style: TextStyle(fontSize: 12)),
          onChanged: (v) => setState(() => c.autoReconnect = v ?? false)),
      ],
      const SizedBox(height: 10), SizedBox(width: double.infinity, child: FilledButton.icon(
        onPressed: c.connecting || c.selected == null ? null : () => c.connect(),
        icon: c.connecting ? const SizedBox(width: 16,height: 16,child: CircularProgressIndicator(strokeWidth: 2)) : const Icon(Icons.link, size: 18),
        label: Text(c.connecting ? _stageLabel(c.stage) : 'Connect to watch'))),
      if (c.connecting) TextButton(onPressed: () => c.disconnect(), child: const Text('Cancel connection')),
      const SizedBox(height: 22), const Divider(), const SizedBox(height: 14),
      Row(children: [const Expanded(child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text('Just looking around?', style: TextStyle(fontSize: 13)),
        Text('Explore with a simulated watch.', style: TextStyle(color: muted, fontSize: 11)),
      ])), OutlinedButton(onPressed: c.connecting ? null : () => c.connect(tryDemo: true), child: const Text('Try demo'))]),
    ]));
    return SingleChildScrollView(padding: const EdgeInsets.fromLTRB(48, 38, 48, 36), child: Center(child: ConstrainedBox(
      constraints: const BoxConstraints(maxWidth: 1160),
      child: constraints.maxWidth >= 960 ? Row(crossAxisAlignment: CrossAxisAlignment.center, children: [
        Expanded(child: intro), const SizedBox(width: 60), Expanded(child: connect),
      ]) : Column(children: [connect, const SizedBox(height: 28), intro]),
    )));
  });

  Widget _dashboard() => LayoutBuilder(builder: (context, size) => Row(children: [
    NavigationRail(extended: size.maxWidth >= 1100, backgroundColor: panel, selectedIndex: c.page,
      onDestinationSelected: c.navigate, minWidth: 76, minExtendedWidth: 196,
      labelType: size.maxWidth >= 1100 ? NavigationRailLabelType.none : NavigationRailLabelType.selected,
      groupAlignment: -.85, indicatorColor: raised,
      destinations: const [
        NavigationRailDestination(icon: Icon(Icons.dashboard_outlined), label: Text('Overview')),
        NavigationRailDestination(icon: Icon(Icons.wifi), label: Text('Wi-Fi')),
        NavigationRailDestination(icon: Icon(Icons.schedule), label: Text('Time')),
        NavigationRailDestination(icon: Icon(Icons.tune), label: Text('Settings')),
        NavigationRailDestination(icon: Icon(Icons.apps), label: Text('Apps')),
        NavigationRailDestination(icon: Icon(Icons.monitor_heart_outlined), label: Text('Diagnostics')),
      ]),
    const VerticalDivider(width: 1, color: line),
    Expanded(child: SingleChildScrollView(padding: const EdgeInsets.all(32), child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      Text(['DEVICE OVERVIEW','NETWORK SERVICE','CLOCK & SYNC','WATCH CONFIGURATION','FIRMWARE APPLICATIONS','SERVICE DIAGNOSTICS'][c.page],
        style: const TextStyle(color: cyan, letterSpacing: 1.8, fontSize: 10)),
      const SizedBox(height: 10), Text(['Your watch, in view.','A connection on your terms.','The right time. On its own.','Shared services. Honest controls.','Built into WristFoundry.','A clearer view of the system.'][c.page],
        style: const TextStyle(fontSize: 29, fontWeight: FontWeight.w600, letterSpacing: -.8)),
      const SizedBox(height: 8), Text('${c.identity['model']}  ·  Firmware ${c.identity['firmware']}',style: const TextStyle(color: muted, fontSize: 12)),
      const SizedBox(height: 26),
      if (c.operationMessage != null) Padding(padding: const EdgeInsets.only(bottom: 18), child: _inset(c.operationMessage!)),
      if (c.busy) const Padding(padding: EdgeInsets.only(bottom: 16), child: LinearProgressIndicator(minHeight: 2)),
      if (c.page == 0) _overview(size.maxWidth),
      if (c.page == 1) _wifi(),
      if (c.page == 2) _time(),
      if (c.page == 3) _settings(),
      if (c.page == 4) _apps(),
      if (c.page == 5) _diagnostics(),
    ]))),
  ]));

  Widget _overview(double width) => Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
    Wrap(spacing: 16, runSpacing: 16, children: [
      _metric('BATTERY', c.snapshot['battery_percent'] == null ? 'Unknown' : '${c.snapshot['battery_percent']}%',
        c.snapshot['charging'] == true ? 'Charging via USB' : c.snapshot['charging'] == false ? 'On battery' : 'Charge status unknown', Icons.battery_charging_full),
      _metric('WI-FI', '${c.snapshot['wifi_state'] ?? 'Unknown'}',c.snapshot['ssid']?.toString().isNotEmpty == true ? '${c.snapshot['ssid']}' : 'Radio on demand', Icons.wifi),
      _metric('WATCH TIME', _clock(), '${c.snapshot['timezone'] ?? 'Timezone unknown'}', Icons.schedule),
    ]),
    const SizedBox(height: 22),
    _card(Row(crossAxisAlignment: CrossAxisAlignment.center, children: [
      Expanded(child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        _badge(c.demo ? 'SIMULATED WATCH' : 'VERIFIED PROTOCOL IDENTITY', green),
        const SizedBox(height: 20), const Text('One framework.\nA little more room.', style: TextStyle(fontSize: 26,fontWeight: FontWeight.w600,height: 1.25)),
        const SizedBox(height: 14), Text('Connection  ${c.demo ? 'Demo transport' : c.selected?.port ?? 'USB'}\nStorage  ${_persistence()}\nTime source  ${c.snapshot['time_source'] ?? 'Unknown'}',
          style: const TextStyle(color: muted, height: 1.9, fontSize: 12)),
        const SizedBox(height: 20), OutlinedButton.icon(onPressed: c.busy ? null : () => c.execute('device.get'),
          icon: const Icon(Icons.refresh, size: 16),label: const Text('Refresh device')),
      ])), if (width >= 900) SizedBox(width: 230,height: 280,child: WatchPreview(clock:_clock(), battery:c.snapshot['battery_percent'] as num?, connected:true)),
    ])),
    const SizedBox(height: 20),
    if (!c.setupDone) _card(Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      const Text('Make it yours',style: TextStyle(fontSize: 18,fontWeight: FontWeight.w600)),const SizedBox(height: 8),
      const Text('Confirm a timezone, optionally sync from this PC, or configure Wi-Fi. Every change is explicit.', style: TextStyle(color: muted)),
      const SizedBox(height: 16), Wrap(spacing: 12,runSpacing: 12,children:[
        OutlinedButton(onPressed:() => c.navigate(2),child: const Text('Set up time')),
        OutlinedButton(onPressed:() => c.navigate(1),child: const Text('Configure Wi-Fi')),
        TextButton(onPressed:c.finishSetup,child: const Text('Done / skip setup')),
      ]),
    ])),
  ]);

  String _clock() { final s=c.snapshot['rtc_local']; return s is String && s.length >= 19 ? s.substring(11,19) : 'Unknown'; }
  String _persistence() => c.snapshot['persistence'] == 'encrypted_nvs' ? 'Encrypted NVS on watch' : 'RAM only · cleared on reboot';
  Widget _wifi() => _card(Column(crossAxisAlignment: CrossAxisAlignment.start,children:[
    _badge('${c.snapshot['wifi_state']}  /  ${c.snapshot['wifi_result']}',cyan),const SizedBox(height:18),
    Text(_persistence(),style: const TextStyle(color:amber)),const SizedBox(height:8),
    const Text('Passwords travel over the local USB connection. They are never saved by this app or included in diagnostic exports.',style:TextStyle(color:muted)),
    const SizedBox(height:22),Wrap(spacing:12,runSpacing:12,children:[
      OutlinedButton.icon(onPressed:_action('wifi.scan'),icon:const Icon(Icons.radar,size:18),label:Text(c.snapshot['scan_busy']==true ? 'Scanning on watch…' : 'Scan on watch')),
      if(c.snapshot['scan_result'] != null) Padding(padding:const EdgeInsets.all(12),child:Text('${c.snapshot['scan_result']}',style:const TextStyle(color:muted,fontSize:12))),
    ]),
    const SizedBox(height:12),
    for(final ap in (c.snapshot['networks'] as List? ?? [])) ListTile(contentPadding:EdgeInsets.zero,
      leading:Icon(ap['secured']==true ? Icons.wifi_lock : Icons.wifi,color:cyan,size:21),
      title:Text('${ap['ssid']}'),trailing:Text('${ap['rssi']} dBm',style:const TextStyle(color:muted)),
      onTap:() => setState(() => ssid.text=ap['ssid'] as String)),
    const SizedBox(height:12),TextField(controller:ssid,enabled:!c.busy,decoration:const InputDecoration(labelText:'Network SSID',counterText:''),maxLength:32),
    const SizedBox(height:14),TextField(controller:password,enabled:!c.busy,obscureText:true,enableSuggestions:false,autocorrect:false,
      decoration:const InputDecoration(labelText:'Password (empty for an open network)',counterText:''),maxLength:64),
    const SizedBox(height:22),Wrap(spacing:12,runSpacing:12,children:[
      FilledButton(onPressed:c.supports('wifi.configure') && !c.busy ? () async {
        final params=<String,dynamic>{'ssid':ssid.text,'password':password.text};
        password.clear();
        await c.execute('wifi.configure',params);
        params.clear();
      }:null,child:const Text('Save to watch')),
      OutlinedButton(onPressed:_action('wifi.connect'),child:const Text('Connect Wi-Fi')),
      OutlinedButton(onPressed:_action('wifi.disconnect'),child:const Text('Disconnect Wi-Fi')),
    ]),const SizedBox(height:18),
    const Text('Save stores configuration; Connect starts the radio. “Accepted” is not an online result. The status above comes from the watch. Display-off may stop the radio under the existing power policy.',style:TextStyle(color:muted,fontSize:12)),
  ]));

  Widget _time() => Column(children:[
    _card(Column(crossAxisAlignment:CrossAxisAlignment.start,children:[
      Text(_clock(),style:const TextStyle(fontSize:42,fontWeight:FontWeight.w300,letterSpacing:1)),
      Text('Watch timezone: ${c.snapshot['timezone']}  ·  RTC ${c.snapshot['rtc_online']==true ? 'online' : 'unavailable'}',style:const TextStyle(color:muted)),
      const SizedBox(height:20),DropdownButtonFormField<WatchZone>(initialValue:zone,isExpanded:true,
        decoration:const InputDecoration(labelText:'Watch timezone (date-aware DST rules)'),
        items:watchZones.map((z)=>DropdownMenuItem(value:z,child:Text(z.name))).toList(),
        onChanged:c.busy ? null : (z)=>setState(()=>zone=z!)),const SizedBox(height:16),
      Wrap(spacing:12,runSpacing:12,children:[
        OutlinedButton(onPressed:c.supports('timezone.set') && !c.busy ? ()=>c.execute('timezone.set',{'timezone':zone.rule}):null,child:const Text('Apply timezone')),
        FilledButton.icon(onPressed:c.supports('time.set') && !c.busy ? ()=>c.execute('time.set',{'epoch_utc':DateTime.now().toUtc().millisecondsSinceEpoch~/1000}):null,
          icon:const Icon(Icons.sync,size:18),label:const Text('Sync from this PC')),
      ]),const SizedBox(height:12),const Text('PC sync sends UTC. Timezone is an independent watch setting. The watch updates its local RTC and system clock; RTC failures are reported.',style:TextStyle(color:muted,fontSize:12)),
    ])),const SizedBox(height:18),
    _card(Column(crossAxisAlignment:CrossAxisAlignment.start,children:[
      const Text('Set time manually',style:TextStyle(fontSize:18,fontWeight:FontWeight.w600)),const SizedBox(height:16),
      TextField(controller:manual,decoration:const InputDecoration(labelText:'Local date / time',hintText:'YYYY-MM-DD HH:MM:SS')),
      const SizedBox(height:14),DropdownButtonFormField<WatchZone>(initialValue:manualZone,isExpanded:true,
        decoration:const InputDecoration(labelText:'Timezone of the entered time'),
        items:watchZones.map((z)=>DropdownMenuItem(value:z,child:Text(z.name))).toList(),
        onChanged:c.busy ? null : (z)=>setState(()=>manualZone=z!)),
      if(localError!=null) Padding(padding:const EdgeInsets.only(top:12),child:Text(localError!,style:const TextStyle(color:amber))),
      const SizedBox(height:16),OutlinedButton(onPressed:c.supports('time.set') && !c.busy ? () {
        try {
          final raw=manual.text.trim();
          if(!RegExp(r'^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$').hasMatch(raw)) throw const FormatException('Use YYYY-MM-DD HH:MM:SS.');
          final wall=DateTime.parse('${raw.replaceFirst(' ','T')}Z');
          if(wall.toIso8601String().substring(0,19)!=raw.replaceFirst(' ','T')) throw const FormatException('Enter a valid calendar date.');
          final utc=manualZone.wallToUtc(wall);
          if(utc.year<2024 || utc.year>2099) throw const FormatException('Supported range is 2024–2099 UTC.');
          setState(()=>localError=null);
          unawaited(c.execute('time.set',{'epoch_utc':utc.millisecondsSinceEpoch~/1000}));
        } on FormatException catch(e) { setState(()=>localError=e.message); }
      }:null,child:const Text('Set watch time')),
    ])),const SizedBox(height:18),
    _card(Column(crossAxisAlignment:CrossAxisAlignment.start,children:[
      const Text('Network time',style:TextStyle(fontSize:18,fontWeight:FontWeight.w600)),const SizedBox(height:10),
      Text('${c.snapshot['sync_state']} · ${c.snapshot['sync_status']}',style:const TextStyle(color:cyan)),
      Text('${c.snapshot['sync_detail'] ?? ''}',style:const TextStyle(color:muted)),const SizedBox(height:16),
      Wrap(spacing:12,runSpacing:12,children:[OutlinedButton(onPressed:_action('ntp.sync'),child:const Text('Request NTP sync')),
        TextButton(onPressed:_action('ntp.cancel'),child:const Text('Cancel sync'))]),
      const SizedBox(height:12),const Text('Uses the watch’s Wi-Fi/NTP service and pool.ntp.org. The RTC keeps local wall time offline. Existing on-watch auto-sync and power policy remain in control.',style:TextStyle(color:muted,fontSize:12)),
    ])),
  ]);

  Widget _settings()=>_card(Column(crossAxisAlignment:CrossAxisAlignment.start,children:[
    const Text('Firmware-supported settings',style:TextStyle(fontSize:20,fontWeight:FontWeight.w600)),const SizedBox(height:14),
    Text('${c.settings['policy'] ?? 'Read settings to inspect the watch power policy.'}',style:const TextStyle(color:muted)),
    const SizedBox(height:20),_inset('Brightness and sleep intervals are compile-time policy in this firmware. This companion does not expose unimplemented setters.'),
    const SizedBox(height:18),Wrap(spacing:12,runSpacing:12,children:[
      OutlinedButton(onPressed:_action('settings.get'),child:const Text('Read settings')),
      OutlinedButton(onPressed:()=>c.navigate(2),child:const Text('Timezone & time')),
      OutlinedButton(onPressed:()=>c.navigate(1),child:const Text('Wi-Fi configuration'))]),
  ]));
  Widget _apps()=>_card(Column(crossAxisAlignment:CrossAxisAlignment.start,children:[
    const Text('Compiled into your firmware',style:TextStyle(fontSize:20,fontWeight:FontWeight.w600)),const SizedBox(height:12),
    const Text('These are the registered application descriptors reported by the watch. Opening an app uses the same navigation owner as on-watch controls.',style:TextStyle(color:muted)),
    const SizedBox(height:16),for(final a in c.apps) ListTile(contentPadding:EdgeInsets.zero,
      leading:const Icon(Icons.widgets_outlined,color:cyan),title:Text('${a['name']}'),subtitle:Text('${a['description']}',style:const TextStyle(color:muted)),
      trailing:OutlinedButton(onPressed:c.supports('apps.open') && !c.busy ? ()=>c.execute('apps.open',{'app_id':a['id']}):null,child:const Text('Open on watch'))),
    const SizedBox(height:16),Wrap(spacing:12,runSpacing:12,children:[
      OutlinedButton(onPressed:_action('apps.list'),child:const Text('Refresh app list')),
      OutlinedButton(onPressed:_action('apps.home'),child:const Text('Return watch home')),
      const OutlinedButton(onPressed:null,child:Text('Install app · unavailable')),
    ]),const SizedBox(height:18),_inset('Individual app installation and removal need a future package runtime. Today, apps ship with firmware. Firmware flashing is not app installation.'),
  ]));
  Widget _diagnostics()=>_card(Column(crossAxisAlignment:CrossAxisAlignment.start,children:[
    const Text('Useful evidence. No credentials.',style:TextStyle(fontSize:20,fontWeight:FontWeight.w600)),const SizedBox(height:12),
    const Text('A bounded local event history and an allowlisted device summary. Reports exclude raw serial logs, request bodies, SSIDs, IP addresses, device identifiers, and passwords.',style:TextStyle(color:muted)),
    const SizedBox(height:20),Wrap(spacing:12,runSpacing:12,children:[
      OutlinedButton(onPressed:_action('diagnostics.get'),child:const Text('Read device diagnostics')),
      OutlinedButton.icon(onPressed:() async { await Clipboard.setData(ClipboardData(text:c.exportDiagnostics())); },icon:const Icon(Icons.copy,size:16),label:const Text('Copy redacted report')),
    ]),const SizedBox(height:20),Container(width:double.infinity,padding:const EdgeInsets.all(20),decoration:BoxDecoration(color:ink,borderRadius:BorderRadius.circular(12)),
      child:SelectableText(c.exportDiagnostics(),style:const TextStyle(fontFamily:'Consolas',color:muted,fontSize:12,height:1.7))),
  ]));
  VoidCallback? _action(String command)=>c.supports(command) && !c.busy ? ()=>c.execute(command):null;
}

String _stageLabel(ConnectionStage stage)=>switch(stage){
  ConnectionStage.connecting=>'Opening selected port…',ConnectionStage.identifying=>'Verifying WristFoundry…',
  ConnectionStage.loading=>'Loading device services…',_=>'Connect to watch',
};
Widget _card(Widget child)=>Container(width:double.infinity,padding:const EdgeInsets.all(26),
  decoration:BoxDecoration(color:panel,border:Border.all(color:line),borderRadius:BorderRadius.circular(20)),child:child);
Widget _badge(String label,Color color)=>Row(mainAxisSize:MainAxisSize.min,children:[
  Container(width:6,height:6,decoration:BoxDecoration(color:color,shape:BoxShape.circle)),const SizedBox(width:8),
  Flexible(child:Text(label,overflow:TextOverflow.ellipsis,style:TextStyle(color:color,fontSize:10,letterSpacing:1))),
]);
Widget _hint(IconData icon,String label)=>Row(mainAxisSize:MainAxisSize.min,children:[Icon(icon,color:muted,size:16),const SizedBox(width:8),Text(label,style:const TextStyle(color:muted,fontSize:11))]);
Widget _inset(String message,{bool warning=false})=>Container(width:double.infinity,padding:const EdgeInsets.all(15),
  decoration:BoxDecoration(color:warning ? const Color(0xFF282319) : raised,borderRadius:BorderRadius.circular(10)),
  child:Text(message,style:TextStyle(color:warning ? amber : muted,fontSize:12,height:1.6)));
Widget _metric(String label,String value,String detail,IconData icon)=>Container(width:242,padding:const EdgeInsets.all(22),
  decoration:BoxDecoration(color:panel,border:Border.all(color:line),borderRadius:BorderRadius.circular(17)),
  child:Column(crossAxisAlignment:CrossAxisAlignment.start,children:[Row(children:[Text(label,style:const TextStyle(color:muted,fontSize:10,letterSpacing:1.4)),const Spacer(),Icon(icon,color:cyan,size:18)]),
    const SizedBox(height:18),Text(value,style:const TextStyle(fontSize:29,fontWeight:FontWeight.w400)),const SizedBox(height:5),
    Text(detail,overflow:TextOverflow.ellipsis,style:const TextStyle(color:muted,fontSize:11))]));

class WatchPreview extends StatelessWidget {
  const WatchPreview({super.key,this.clock='--:--',this.battery,this.connected=false});
  final String clock;
  final num? battery;
  final bool connected;
  @override
  Widget build(BuildContext context)=>CustomPaint(painter:_WatchOrbit(),child:Center(child:Container(
    width:156,height:207,padding:const EdgeInsets.symmetric(horizontal:14,vertical:20),
    decoration:BoxDecoration(color:ink,borderRadius:BorderRadius.circular(36),border:Border.all(color:line,width:4),
      boxShadow:const[BoxShadow(color:Color(0x2056DBF4),blurRadius:45,spreadRadius:5)]),
    child:Column(crossAxisAlignment:CrossAxisAlignment.start,children:[
      const Text('WRISTFOUNDRY',style:TextStyle(color:cyan,fontSize:8,letterSpacing:1.6)),const SizedBox(height:27),
      Text(clock,style:TextStyle(color:text,fontSize:clock.length>5 ? 24:32,fontWeight:FontWeight.w300,letterSpacing:-1)),
      const SizedBox(height:6),Text(connected ? 'DEVICE RTC' : 'READY TO CONNECT',style:const TextStyle(color:muted,fontSize:7,letterSpacing:1.4)),
      const Spacer(),Container(padding:const EdgeInsets.all(9),decoration:BoxDecoration(color:raised,borderRadius:BorderRadius.circular(10)),
        child:Row(children:[const Icon(Icons.memory,size:12,color:cyan),const SizedBox(width:6),Text(battery==null ? 'USB / v1' : 'BATTERY  $battery%',style:const TextStyle(color:cyan,fontSize:7))])),
    ]),
  )));
}
class _WatchOrbit extends CustomPainter {
  @override
  void paint(Canvas canvas,Size size){
    final center=Offset(size.width/2,size.height/2);
    final paint=Paint()..color=line..style=PaintingStyle.stroke..strokeWidth=.6;
    canvas.drawOval(Rect.fromCenter(center:center,width:270,height:150),paint);
    canvas.drawOval(Rect.fromCenter(center:center,width:330,height:205),paint);
    canvas.drawLine(center-const Offset(185,0),center+const Offset(185,0),paint);
    paint..color=cyan..style=PaintingStyle.fill;
    canvas.drawCircle(center+const Offset(138,-49),3,paint);
    canvas.drawCircle(center-const Offset(138,-49),2,paint);
    paint.color=green;canvas.drawCircle(center+const Offset(120,66),2,paint);
  }
  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate)=>false;
}
