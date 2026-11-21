import 'package:flutter/material.dart';

/// Portable metadata only; never interpret a watch's LVGL glyph as Flutter text.
Widget appIcon(Map<String, dynamic> app) {
  final meta = app['icon'];
  final name = meta is Map && meta['type'] == 'builtin' ? meta['name'] : null;
  final icon = switch (name) {
    'settings' => Icons.settings_outlined,
    'system' => Icons.memory,
    'radar' => Icons.radar,
    _ => null,
  };
  if (icon != null) return Icon(icon, color: const Color(0xFF56DBF4));
  final initials = app['initials'];
  final label = initials is String && initials.isNotEmpty ? initials : '${app['name'] ?? '?'}';
  return SizedBox(width: 32, child: Text(String.fromCharCodes(label.runes.take(2)),
    style: const TextStyle(color: Color(0xFF56DBF4)), textAlign: TextAlign.center));
}
