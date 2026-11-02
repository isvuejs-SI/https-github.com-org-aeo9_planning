import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../models/mission_models.dart';
import '../theme/aeo9_theme.dart';

class MissionMap extends StatefulWidget {
  const MissionMap({super.key, required this.snapshot});

  final MissionSnapshot snapshot;

  @override
  State<MissionMap> createState() => _MissionMapState();
}

class _MissionMapState extends State<MissionMap>
    with SingleTickerProviderStateMixin {
  late final AnimationController _pulse;

  @override
  void initState() {
    super.initState();
    _pulse = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 2200),
    )..repeat(reverse: true);
  }

  @override
  void dispose() {
    _pulse.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: _pulse,
      builder: (context, _) {
        return CustomPaint(
          painter: _MissionMapPainter(
            snapshot: widget.snapshot,
            pulse: _pulse.value,
          ),
          child: const SizedBox.expand(),
        );
      },
    );
  }
}

class _MissionMapPainter extends CustomPainter {
  _MissionMapPainter({required this.snapshot, required this.pulse});

  final MissionSnapshot snapshot;
  final double pulse;

  @override
  void paint(Canvas canvas, Size size) {
    final rect = Offset.zero & size;
    final sky = Paint()
      ..shader = const LinearGradient(
        begin: Alignment.topCenter,
        end: Alignment.bottomCenter,
        colors: [Aeo9Colors.skyTop, Aeo9Colors.skyMid, Aeo9Colors.mapLand],
        stops: [0.0, 0.45, 1.0],
      ).createShader(rect);
    canvas.drawRect(rect, sky);

    // Soft terrain bands
    final band = Paint()..color = Aeo9Colors.mapShade.withValues(alpha: 0.28);
    for (var i = 0; i < 5; i++) {
      final y = size.height * (0.35 + i * 0.12);
      canvas.drawOval(
        Rect.fromCenter(
          center: Offset(size.width * (0.2 + i * 0.15), y),
          width: size.width * 0.7,
          height: 36 + i * 8.0,
        ),
        band,
      );
    }

    final center = Offset(size.width * 0.5, size.height * 0.58);
    final scale = math.min(size.width, size.height) / (snapshot.geofenceRadiusM * 2.4);

    Offset project(double northM, double eastM) {
      return Offset(
        center.dx + eastM * scale,
        center.dy - northM * scale,
      );
    }

    // Geofence
    final fenceR = snapshot.geofenceRadiusM * scale;
    canvas.drawCircle(
      center,
      fenceR,
      Paint()
        ..color = Aeo9Colors.fence
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2,
    );
    canvas.drawCircle(
      center,
      fenceR,
      Paint()..color = Aeo9Colors.link.withValues(alpha: 0.05 + pulse * 0.04),
    );

    // Route
    final route = Path();
    for (var i = 0; i < snapshot.waypoints.length; i++) {
      final wp = snapshot.waypoints[i];
      final p = project(wp.northM, wp.eastM);
      if (i == 0) {
        route.moveTo(p.dx, p.dy);
      } else {
        route.lineTo(p.dx, p.dy);
      }
    }
    canvas.drawPath(
      route,
      Paint()
        ..color = Aeo9Colors.stone.withValues(alpha: 0.55)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2.5
        ..strokeCap = StrokeCap.round
        ..strokeJoin = StrokeJoin.round,
    );

    for (final wp in snapshot.waypoints) {
      final p = project(wp.northM, wp.eastM);
      canvas.drawCircle(
        p,
        wp.active ? 7 : 5,
        Paint()
          ..color = wp.active ? Aeo9Colors.accent : Aeo9Colors.stone.withValues(alpha: 0.7),
      );
      if (wp.active) {
        canvas.drawCircle(
          p,
          12 + pulse * 4,
          Paint()
            ..color = Aeo9Colors.accentSoft.withValues(alpha: 0.25)
            ..style = PaintingStyle.stroke
            ..strokeWidth = 2,
        );
      }
    }

    // Home
    final home = project(0, 0);
    final homePaint = Paint()..color = Aeo9Colors.ink;
    canvas.drawRRect(
      RRect.fromRectAndRadius(
        Rect.fromCenter(center: home, width: 14, height: 14),
        const Radius.circular(2),
      ),
      homePaint,
    );

    // Aircraft
    final craft = project(snapshot.aircraftNorthM, snapshot.aircraftEastM);
    canvas.save();
    canvas.translate(craft.dx, craft.dy);
    canvas.rotate(snapshot.headingDeg * math.pi / 180);
    final body = Path()
      ..moveTo(0, -14)
      ..lineTo(10, 12)
      ..lineTo(0, 6)
      ..lineTo(-10, 12)
      ..close();
    canvas.drawPath(body, Paint()..color = Aeo9Colors.accent);
    canvas.drawCircle(
      Offset.zero,
      18 + pulse * 6,
      Paint()
        ..color = Aeo9Colors.accent.withValues(alpha: 0.18)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2,
    );
    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant _MissionMapPainter oldDelegate) {
    return oldDelegate.pulse != pulse || oldDelegate.snapshot != snapshot;
  }
}
