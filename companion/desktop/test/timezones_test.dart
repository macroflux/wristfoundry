import 'package:flutter_test/flutter_test.dart';
import 'package:wristfoundry_companion/timezones.dart';
void main(){
  test('PC UTC epoch does not use PC timezone offset',(){
    final pc=DateTime.parse('2026-11-20T09:30:00-05:00');
    expect(pc.toUtc(),DateTime.utc(2026,11,20,14,30));
  });
  test('US date-dependent DST rules and boundaries',(){
    final eastern=watchZones[1];
    expect(eastern.wallToUtc(DateTime.utc(2026,1,15,9)),DateTime.utc(2026,1,15,14));
    expect(eastern.wallToUtc(DateTime.utc(2026,7,15,9)),DateTime.utc(2026,7,15,13));
    expect(eastern.offsetAt(DateTime.utc(2026,3,8,6,59)),-5);
    expect(eastern.offsetAt(DateTime.utc(2026,3,8,7)),-4);
    expect(()=>eastern.wallToUtc(DateTime.utc(2026,3,8,2,30)),throwsFormatException);
    expect(()=>eastern.wallToUtc(DateTime.utc(2026,11,1,1,30)),throwsFormatException);
  });
  test('European DST boundaries and no-DST Arizona',(){
    final london=watchZones[6],berlin=watchZones[7];
    expect(london.offsetAt(DateTime.utc(2026,3,29,0,59)),0);
    expect(london.offsetAt(DateTime.utc(2026,3,29,1)),1);
    expect(berlin.offsetAt(DateTime.utc(2026,10,25,1)),1);
    expect(watchZones[5].offsetAt(DateTime.utc(2026,7,15)),-7);
    expect(watchZones.first.wallToUtc(DateTime.utc(2026,1,1)),DateTime.utc(2026,1,1));
  });
}
