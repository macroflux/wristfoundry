import 'package:flutter/material.dart';
import 'controller.dart';

/// Retains a local draft across device refreshes. Revision travels with the
/// draft so a concurrent on-watch edit cannot be silently overwritten.
class SettingsEditor extends StatefulWidget {
  const SettingsEditor({super.key, required this.controller});
  final CompanionController controller;
  @override
  State<SettingsEditor> createState() => _SettingsEditorState();
}

class _SettingsEditorState extends State<SettingsEditor> {
  final dim = TextEditingController(), off = TextEditingController();
  String usb = 'stay_on', radio = 'stop';
  int? revision;
  bool dirty = false;
  String? error;
  CompanionController get c => widget.controller;
  void load() {
    final s = c.settings;
    if (s['revision'] is! int) return;
    revision = s['revision'] as int;
    dim.text = '${s['dim_seconds']}';
    off.text = '${s['off_seconds']}';
    usb = s['usb_display'] == 'timeout' ? 'timeout' : 'stay_on';
    radio = s['radio_policy'] == 'finish_bounded' ? 'finish_bounded' : 'stop';
    dirty = false;
    error = null;
  }
  @override
  void initState() { super.initState(); load(); }
  @override
  void didUpdateWidget(covariant SettingsEditor oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (!dirty && revision != c.settings['revision']) load();
  }
  @override
  void dispose() { dim.dispose(); off.dispose(); super.dispose(); }

  Future<void> save({bool reset = false}) async {
    final d = int.tryParse(dim.text), o = int.tryParse(off.text);
    if (!reset && (d == null || o == null || d < 5 || d > 300 ||
        o < 10 || o > 600 || o <= d)) {
      setState(() => error = 'Dim must be 5–300 seconds; display-off must be 10–600 seconds and greater than dim.');
      return;
    }
    final ok = await c.execute(reset ? 'settings.reset' : 'settings.set', {
      'expected_revision': revision,
      if (!reset) ...{'dim_seconds': d, 'off_seconds': o,
        'usb_display': usb, 'radio_policy': radio},
    });
    if (!mounted) return;
    if (ok) setState(load);
  }

  @override
  Widget build(BuildContext context) {
    final supported = c.supports('settings.set') && c.settings['schema_version'] == 1;
    final enabled = supported && c.settings['storage_ready'] == true && !c.busy && revision != null;
    return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      const Text('Power & display', style: TextStyle(fontSize: 20, fontWeight: FontWeight.w600)),
      const SizedBox(height: 12),
      if (!supported) const Text('This firmware supports reading its policy only. Update the watch firmware to edit preferences.'),
      if (supported && c.settings['storage_ready'] != true)
        const Text('Preference storage is unavailable. Changes cannot be saved.'),
      if (supported) ...[
        const Text('Saved on the watch. USB communication stays available even with the display off.'),
        const SizedBox(height: 20),
        DropdownButtonFormField<String>(initialValue: usb,
          key: ValueKey('usb-$revision-$usb'),
          decoration: const InputDecoration(labelText: 'USB display behavior'),
          items: const [DropdownMenuItem(value: 'stay_on', child: Text('Stay on')),
            DropdownMenuItem(value: 'timeout', child: Text('Follow display timeouts'))],
          onChanged: enabled ? (v) => setState(() { usb = v!; dirty = true; }) : null),
        const SizedBox(height: 16),
        TextField(controller: dim, enabled: enabled, keyboardType: TextInputType.number,
          decoration: const InputDecoration(labelText: 'Dim after (seconds, 5–300)'),
          onChanged: (_) => setState(() => dirty = true)),
        const SizedBox(height: 16),
        TextField(controller: off, enabled: enabled, keyboardType: TextInputType.number,
          decoration: const InputDecoration(labelText: 'Display off after (seconds, 10–600)'),
          onChanged: (_) => setState(() => dirty = true)),
        const SizedBox(height: 16),
        DropdownButtonFormField<String>(initialValue: radio,
          key: ValueKey('radio-$revision-$radio'),
          decoration: const InputDecoration(labelText: 'Radios when the display turns off'),
          items: const [DropdownMenuItem(value: 'stop', child: Text('Stop immediately')),
            DropdownMenuItem(value: 'finish_bounded', child: Text('Finish current work (maximum 30 seconds)'))],
          onChanged: enabled ? (v) => setState(() { radio = v!; dirty = true; }) : null),
        const SizedBox(height: 16),
        if (dirty && revision != c.settings['revision'])
          const Text('Settings changed on the watch. Reload before saving to avoid replacing that change.'),
        if (error != null) Text(error!, style: const TextStyle(color: Colors.amber)),
        Wrap(spacing: 12, runSpacing: 12, children: [
          FilledButton(onPressed: enabled ? () => save() : null, child: const Text('Save to watch')),
          OutlinedButton(onPressed: enabled && c.supports('settings.reset') ? () => save(reset: true) : null,
            child: const Text('Restore defaults')),
          OutlinedButton(onPressed: !c.busy ? () async {
            if (await c.execute('settings.get') && mounted) setState(load);
          } : null, child: const Text('Reload settings')),
        ]),
      ],
      if (!supported) Text('${c.settings['policy'] ?? 'Read settings to inspect watch policy.'}'),
      const SizedBox(height: 24),
      Wrap(spacing: 12, runSpacing: 12, children: [
        OutlinedButton(onPressed: () => c.navigate(1), child: const Text('Wi-Fi configuration')),
        OutlinedButton(onPressed: () => c.navigate(2), child: const Text('Timezone & time')),
      ]),
    ]);
  }
}
