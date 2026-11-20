/// Protocol-v1 named zones, matching main/timezone_rules.h. POSIX rules are
/// evaluated for the entered date, never derived from today's PC UTC offset.
class WatchZone {
  const WatchZone(this.name, this.rule, this.standardHours, this.dst);
  final String name;
  final String rule;
  final int standardHours;
  final String dst;

  int offsetAt(DateTime utc) {
    if (dst.isEmpty) return standardHours;
    final y = utc.year;
    late DateTime start, end;
    if (dst == 'us') {
      start = DateTime.utc(y, 3, _sunday(y, 3, 2), 2 - standardHours);
      end = DateTime.utc(y, 11, _sunday(y, 11, 1), 1 - standardHours);
    } else {
      start = DateTime.utc(y, 3, _lastSunday(y, 3), 1);
      end = DateTime.utc(y, 10, _lastSunday(y, 10), 1);
    }
    return standardHours + (!utc.isBefore(start) && utc.isBefore(end) ? 1 : 0);
  }

  /// Reject nonexistent spring times and ambiguous autumn times. The UI asks
  /// for UTC in the fold rather than silently choosing the wrong occurrence.
  DateTime wallToUtc(DateTime wall) {
    final nominal = DateTime.utc(
      wall.year,
      wall.month,
      wall.day,
      wall.hour,
      wall.minute,
      wall.second,
    );
    final matches = <DateTime>[];
    for (final offset in {
      standardHours,
      if (dst.isNotEmpty) standardHours + 1,
    }) {
      final utc = nominal.subtract(Duration(hours: offset));
      if (offsetAt(utc) == offset) matches.add(utc);
    }
    if (matches.length != 1)
      throw const FormatException(
        'DST gap or repeated hour. Select UTC and enter UTC time.',
      );
    return matches.single;
  }
}

int _sunday(int y, int m, int ordinal) =>
    1 + (7 - DateTime.utc(y, m, 1).weekday) % 7 + (ordinal - 1) * 7;
int _lastSunday(int y, int m) {
  final last = DateTime.utc(y, m + 1, 0);
  return last.day - last.weekday % 7;
}

const watchZones = [
  WatchZone('UTC', 'UTC0', 0, ''),
  WatchZone('US Eastern', 'EST5EDT,M3.2.0,M11.1.0', -5, 'us'),
  WatchZone('US Central', 'CST6CDT,M3.2.0,M11.1.0', -6, 'us'),
  WatchZone('US Mountain', 'MST7MDT,M3.2.0,M11.1.0', -7, 'us'),
  WatchZone('US Pacific', 'PST8PDT,M3.2.0,M11.1.0', -8, 'us'),
  WatchZone('Arizona', 'MST7', -7, ''),
  WatchZone('London', 'GMT0BST,M3.5.0/1,M10.5.0', 0, 'eu'),
  WatchZone('Central Europe', 'CET-1CEST,M3.5.0,M10.5.0/3', 1, 'eu'),
];
