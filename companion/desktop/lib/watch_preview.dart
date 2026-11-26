import 'package:flutter/material.dart';

const _ink = Color(0xFF03070B),
    _raised = Color(0xFF102832),
    _cyan = Color(0xFF56DBF4),
    _text = Color(0xFFEAF2F7),
    _muted = Color(0xFF8296A1),
    _line = Color(0xFF214652),
    _green = Color(0xFF63E6A5);

class WatchPreview extends StatelessWidget {
  const WatchPreview({
    super.key,
    this.clock = '--:--',
    this.battery,
    this.connected = false,
    this.linkLabel = 'USB / v1',
  });
  final String clock;
  final num? battery;
  final bool connected;
  final String linkLabel;
  @override
  Widget build(BuildContext context) => CustomPaint(
    painter: _WatchOrbit(),
    child: Center(
      child: Container(
        width: 156,
        height: 207,
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 20),
        decoration: BoxDecoration(
          color: _ink,
          borderRadius: BorderRadius.circular(36),
          border: Border.all(color: _line, width: 4),
          boxShadow: const [
            BoxShadow(
              color: Color(0x2056DBF4),
              blurRadius: 45,
              spreadRadius: 5,
            ),
          ],
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'WRISTFOUNDRY',
              style: TextStyle(color: _cyan, fontSize: 8, letterSpacing: 1.6),
            ),
            const SizedBox(height: 27),
            FittedBox(
              fit: BoxFit.scaleDown,
              alignment: Alignment.centerLeft,
              child: Text(
                clock,
                maxLines: 1,
                style: TextStyle(
                  color: _text,
                  fontSize: clock.length > 5 ? 24 : 32,
                  fontWeight: FontWeight.w300,
                  letterSpacing: -1,
                ),
              ),
            ),
            const SizedBox(height: 6),
            Text(
              connected ? 'DEVICE RTC' : 'READY TO CONNECT',
              style: const TextStyle(
                color: _muted,
                fontSize: 7,
                letterSpacing: 1.4,
              ),
            ),
            const Spacer(),
            Container(
              padding: const EdgeInsets.all(9),
              decoration: BoxDecoration(
                color: _raised,
                borderRadius: BorderRadius.circular(10),
              ),
              child: Row(
                children: [
                  const Icon(Icons.memory, size: 12, color: _cyan),
                  const SizedBox(width: 6),
                  Expanded(
                    child: Text(
                      battery == null ? linkLabel : 'BATTERY  $battery%',
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: const TextStyle(color: _cyan, fontSize: 7),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    ),
  );
}

class _WatchOrbit extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final paint = Paint()
      ..color = _line
      ..style = PaintingStyle.stroke
      ..strokeWidth = .6;
    canvas.drawOval(
      Rect.fromCenter(center: center, width: 270, height: 150),
      paint,
    );
    canvas.drawOval(
      Rect.fromCenter(center: center, width: 330, height: 205),
      paint,
    );
    canvas.drawLine(
      center - const Offset(185, 0),
      center + const Offset(185, 0),
      paint,
    );
    paint
      ..color = _cyan
      ..style = PaintingStyle.fill;
    canvas.drawCircle(center + const Offset(138, -49), 3, paint);
    canvas.drawCircle(center - const Offset(138, -49), 2, paint);
    paint.color = _green;
    canvas.drawCircle(center + const Offset(120, 66), 2, paint);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}
