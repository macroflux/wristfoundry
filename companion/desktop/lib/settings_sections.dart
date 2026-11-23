import 'package:flutter/material.dart';
import 'controller.dart';
import 'settings_editor.dart';

class SettingsSections extends StatefulWidget {
  const SettingsSections({
    super.key,
    required this.controller,
    required this.onWifi,
    required this.onTime,
  });
  final CompanionController controller;
  final VoidCallback onWifi, onTime;
  @override
  State<SettingsSections> createState() => _SettingsSectionsState();
}

class _SettingsSectionsState extends State<SettingsSections> {
  String? section;
  CompanionController get c => widget.controller;
  @override
  Widget build(BuildContext context) {
    if (section == 'power') {
      return Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          TextButton.icon(
            onPressed: () => setState(() => section = null),
            icon: const Icon(Icons.arrow_back),
            label: const Text('All settings'),
          ),
          SettingsEditor(controller: c),
        ],
      );
    }
    if (section == 'phone') {
      return Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          TextButton.icon(
            onPressed: () => setState(() => section = null),
            icon: const Icon(Icons.arrow_back),
            label: const Text('All settings'),
          ),
          const Text('Phone connection', style: TextStyle(fontSize: 22)),
          if (!c.supports('phone.get'))
            const Text(
              'Phone connections require compatible BLE firmware (0.3.0 or later).',
            ),
          const SizedBox(height: 16),
          const Text(
            'On the watch, open Settings → Phone connection → Discover for 60 seconds. Enter the watch code in Android’s pairing dialog.',
          ),
          const SizedBox(height: 12),
          const Text(
            'The display stays on during an authenticated session. PWR can blank it. Five minutes without user interaction ends the session and restores normal power behavior.',
          ),
          if (c.phone.isNotEmpty)
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 16),
              child: Text(
                c.phone['active'] == true
                    ? 'Connected · ${c.phone['seconds_remaining']} seconds until idle timeout'
                    : c.phone['advertising'] == true
                    ? 'Discoverable · ${c.phone['seconds_remaining']} seconds remaining'
                    : 'Bluetooth off or pairing',
              ),
            ),
          Wrap(
            spacing: 12,
            runSpacing: 12,
            children: [
              if (!c.wireless)
                OutlinedButton(
                  onPressed: c.supports('phone.discover') && !c.busy
                      ? () => c.execute('phone.discover')
                      : null,
                  child: const Text('Discover for 60 seconds'),
                ),
              OutlinedButton(
                onPressed: c.supports('phone.get') && !c.busy
                    ? () => c.execute('phone.get')
                    : null,
                child: const Text('Refresh status'),
              ),
              OutlinedButton(
                onPressed:
                    !c.wireless && (!c.supports('phone.disconnect') || c.busy)
                    ? null
                    : () {
                        if (c.wireless) {
                          c.disconnect();
                        } else {
                          c.execute('phone.disconnect');
                        }
                      },
                child: Text(
                  c.wireless ? 'Disconnect watch' : 'Stop phone connection',
                ),
              ),
            ],
          ),
        ],
      );
    }
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        const Text('Choose a settings section', style: TextStyle(fontSize: 20)),
        const SizedBox(height: 16),
        _section(
          Icons.tune,
          'Power & display',
          'Display timeouts, USB behavior and radio policy',
          () => setState(() => section = 'power'),
        ),
        _section(Icons.wifi, 'Wi-Fi', 'Network configuration', widget.onWifi),
        _section(
          Icons.schedule,
          'Time & timezone',
          'Clock and synchronization',
          widget.onTime,
        ),
        _section(
          Icons.bluetooth,
          'Phone connection',
          'Discovery, pairing and session status',
          () {
            setState(() => section = 'phone');
            if (c.supports('phone.get')) c.execute('phone.get');
          },
        ),
      ],
    );
  }

  Widget _section(
    IconData icon,
    String title,
    String subtitle,
    VoidCallback action,
  ) => Padding(
    padding: const EdgeInsets.only(bottom: 12),
    child: Card(
      child: ListTile(
        contentPadding: const EdgeInsets.all(16),
        leading: Icon(icon),
        title: Text(title),
        subtitle: Text(subtitle),
        trailing: const Icon(Icons.chevron_right),
        onTap: action,
      ),
    ),
  );
}
