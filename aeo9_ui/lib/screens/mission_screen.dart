import 'package:flutter/material.dart';

import '../data/mock_mission.dart';
import '../models/mission_models.dart';
import '../theme/aeo9_theme.dart';
import '../widgets/mission_map.dart';
import '../widgets/status_chip.dart';

/// Screen 1 of 5 — Mission.
/// Primary home for Flight & Autonomy (features 1–6) with glanceable
/// sensing / power / link signals from the rest of the 32-feature set.
class MissionScreen extends StatefulWidget {
  const MissionScreen({super.key, this.snapshot});

  final MissionSnapshot? snapshot;

  @override
  State<MissionScreen> createState() => _MissionScreenState();
}

class _MissionScreenState extends State<MissionScreen>
    with SingleTickerProviderStateMixin {
  late MissionSnapshot _snap;
  late final AnimationController _intro;
  String? _lastRequest;

  @override
  void initState() {
    super.initState();
    _snap = widget.snapshot ?? mockMissionSnapshot();
    _intro = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 900),
    )..forward();
  }

  @override
  void dispose() {
    _intro.dispose();
    super.dispose();
  }

  void _request(String action) {
    setState(() => _lastRequest = action);
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text('Sim request: $action (PX4 owns final accept/reject)'),
        behavior: SnackBarBehavior.floating,
        backgroundColor: Aeo9Colors.stone,
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final wide = MediaQuery.sizeOf(context).width >= 900;
    final fade = CurvedAnimation(parent: _intro, curve: Curves.easeOutCubic);

    return Scaffold(
      body: DecoratedBox(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [
              Color(0xFF9EB6C4),
              Color(0xFFD5DEE5),
              Color(0xFFC8B79A),
            ],
            stops: [0.0, 0.55, 1.0],
          ),
        ),
        child: SafeArea(
          child: FadeTransition(
            opacity: fade,
            child: SlideTransition(
              position: Tween<Offset>(
                begin: const Offset(0, 0.04),
                end: Offset.zero,
              ).animate(fade),
              child: Padding(
                padding: const EdgeInsets.fromLTRB(16, 12, 16, 16),
                child: wide ? _WideMission(snap: _snap, onRequest: _request, lastRequest: _lastRequest) : _CompactMission(snap: _snap, onRequest: _request, lastRequest: _lastRequest),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _WideMission extends StatelessWidget {
  const _WideMission({
    required this.snap,
    required this.onRequest,
    required this.lastRequest,
  });

  final MissionSnapshot snap;
  final void Function(String) onRequest;
  final String? lastRequest;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        _TopBar(snap: snap),
        const SizedBox(height: 12),
        Expanded(
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Expanded(
                flex: 3,
                child: _MapPanel(snap: snap),
              ),
              const SizedBox(width: 12),
              SizedBox(
                width: 320,
                child: _SidePanel(
                  snap: snap,
                  onRequest: onRequest,
                  lastRequest: lastRequest,
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _CompactMission extends StatelessWidget {
  const _CompactMission({
    required this.snap,
    required this.onRequest,
    required this.lastRequest,
  });

  final MissionSnapshot snap;
  final void Function(String) onRequest;
  final String? lastRequest;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        _TopBar(snap: snap),
        const SizedBox(height: 10),
        Expanded(flex: 5, child: _MapPanel(snap: snap)),
        const SizedBox(height: 10),
        Expanded(
          flex: 4,
          child: _SidePanel(
            snap: snap,
            onRequest: onRequest,
            lastRequest: lastRequest,
          ),
        ),
      ],
    );
  }
}

class _TopBar extends StatelessWidget {
  const _TopBar({required this.snap});

  final MissionSnapshot snap;

  @override
  Widget build(BuildContext context) {
    final stateColor = switch (snap.failsafe) {
      FailsafeState.nominal => Aeo9Colors.accent,
      FailsafeState.hold => Aeo9Colors.warn,
      FailsafeState.rtl => Aeo9Colors.warn,
      FailsafeState.land => Aeo9Colors.danger,
    };

    return Row(
      children: [
        Text(
          'AEO-9',
          style: Theme.of(context).textTheme.headlineMedium?.copyWith(
                fontWeight: FontWeight.w800,
                letterSpacing: -1.0,
              ),
        ),
        const SizedBox(width: 10),
        Text(
          'Mission',
          style: Theme.of(context).textTheme.titleMedium?.copyWith(
                color: Aeo9Colors.stone,
                fontWeight: FontWeight.w500,
              ),
        ),
        const Spacer(),
        if (snap.simOnly)
          Container(
            margin: const EdgeInsets.only(right: 10),
            padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
            decoration: BoxDecoration(
              color: Aeo9Colors.warn.withValues(alpha: 0.15),
              borderRadius: BorderRadius.circular(8),
              border: Border.all(color: Aeo9Colors.warn.withValues(alpha: 0.35)),
            ),
            child: Text(
              'SIM ONLY',
              style: Theme.of(context).textTheme.labelLarge?.copyWith(
                    color: Aeo9Colors.warn,
                    fontWeight: FontWeight.w800,
                  ),
            ),
          ),
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
          decoration: BoxDecoration(
            color: stateColor.withValues(alpha: 0.12),
            borderRadius: BorderRadius.circular(8),
            border: Border.all(color: stateColor.withValues(alpha: 0.4)),
          ),
          child: Text(
            snap.failsafeLabel,
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
                  color: stateColor,
                  fontWeight: FontWeight.w800,
                  letterSpacing: 1.2,
                ),
          ),
        ),
      ],
    );
  }
}

class _MapPanel extends StatelessWidget {
  const _MapPanel({required this.snap});

  final MissionSnapshot snap;

  @override
  Widget build(BuildContext context) {
    return ClipRRect(
      borderRadius: BorderRadius.circular(18),
      child: Stack(
        fit: StackFit.expand,
        children: [
          MissionMap(snapshot: snap),
          Positioned(
            left: 14,
            bottom: 14,
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
              decoration: BoxDecoration(
                color: Aeo9Colors.panel,
                borderRadius: BorderRadius.circular(12),
              ),
              child: Text(
                '${snap.homeName}  ·  fence ${snap.geofenceRadiusM.toStringAsFixed(0)} m',
                style: Theme.of(context).textTheme.labelLarge?.copyWith(
                      color: Aeo9Colors.ink,
                    ),
              ),
            ),
          ),
          Positioned(
            right: 14,
            top: 14,
            child: Wrap(
              spacing: 8,
              children: [
                _MiniFlag(
                  label: 'LiDAR',
                  ok: snap.lidarOk,
                ),
                _MiniFlag(
                  label: 'Obstacle',
                  ok: snap.obstacleClear,
                ),
                _MiniFlag(
                  label: 'Night',
                  ok: snap.nightCapable,
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _MiniFlag extends StatelessWidget {
  const _MiniFlag({required this.label, required this.ok});

  final String label;
  final bool ok;

  @override
  Widget build(BuildContext context) {
    final color = ok ? Aeo9Colors.accent : Aeo9Colors.danger;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: Aeo9Colors.panel,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: color.withValues(alpha: 0.35)),
      ),
      child: Text(
        '$label ${ok ? 'OK' : 'CHK'}',
        style: Theme.of(context).textTheme.labelLarge?.copyWith(color: color),
      ),
    );
  }
}

class _SidePanel extends StatelessWidget {
  const _SidePanel({
    required this.snap,
    required this.onRequest,
    required this.lastRequest,
  });

  final MissionSnapshot snap;
  final void Function(String) onRequest;
  final String? lastRequest;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: Aeo9Colors.panel,
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: Aeo9Colors.stone.withValues(alpha: 0.08)),
      ),
      child: LayoutBuilder(
        builder: (context, constraints) {
          return SingleChildScrollView(
            child: ConstrainedBox(
              constraints: BoxConstraints(minHeight: constraints.maxHeight),
              child: IntrinsicHeight(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Text(
                      'Flight picture',
                      style: Theme.of(context).textTheme.titleLarge,
                    ),
                    const SizedBox(height: 4),
                    Text(
                      'Hexacopter · Pixhawk · autonomous legs',
                      style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                            color: Aeo9Colors.stone,
                          ),
                    ),
                    const SizedBox(height: 12),
                    Wrap(
                      spacing: 8,
                      runSpacing: 8,
                      children: [
                        StatusChip(
                          label: 'Altitude',
                          value: '${snap.altitudeM.toStringAsFixed(0)} m',
                          tone: StatusTone.good,
                        ),
                        StatusChip(
                          label: 'Speed',
                          value: '${snap.speedMs.toStringAsFixed(1)} m/s',
                        ),
                        StatusChip(
                          label: 'Battery',
                          value: '${snap.batteryPct.toStringAsFixed(0)}%',
                          tone: snap.batteryPct < 30
                              ? StatusTone.warn
                              : StatusTone.good,
                        ),
                        StatusChip(
                          label: 'Link',
                          value: '${snap.linkPct.toStringAsFixed(0)}%',
                          tone: snap.linkPct < 60
                              ? StatusTone.warn
                              : StatusTone.good,
                        ),
                        StatusChip(
                          label: 'Wind',
                          value: '${snap.windMs.toStringAsFixed(1)} m/s',
                        ),
                        StatusChip(
                          label: 'Escapes',
                          value: '${snap.animalEscapes}/3',
                        ),
                      ],
                    ),
                    const SizedBox(height: 14),
                    _GoNoGoCard(snap: snap),
                    const SizedBox(height: 12),
                    _LinkRow(snap: snap),
                    const SizedBox(height: 12),
                    Text(
                      'Supervisor requests',
                      style: Theme.of(context).textTheme.titleMedium,
                    ),
                    const SizedBox(height: 8),
                    Row(
                      children: [
                        Expanded(
                          child: _ActionButton(
                            label: 'HOLD',
                            onPressed: () => onRequest('HOLD'),
                          ),
                        ),
                        const SizedBox(width: 8),
                        Expanded(
                          child: _ActionButton(
                            label: 'RTL',
                            onPressed: () => onRequest('RTL'),
                            emphasize: true,
                          ),
                        ),
                        const SizedBox(width: 8),
                        Expanded(
                          child: _ActionButton(
                            label: 'LAND',
                            onPressed: () => onRequest('LAND'),
                            danger: true,
                          ),
                        ),
                      ],
                    ),
                    if (lastRequest != null) ...[
                      const SizedBox(height: 8),
                      Text(
                        'Last request: $lastRequest',
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(
                              color: Aeo9Colors.stone,
                            ),
                      ),
                    ],
                    const Spacer(),
                    const SizedBox(height: 12),
                    Text(
                      'Screen 1/5 · features 1–6 + glanceables from sensing, power, link',
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                            color: Aeo9Colors.stone.withValues(alpha: 0.8),
                          ),
                    ),
                  ],
                ),
              ),
            ),
          );
        },
      ),
    );
  }
}

class _GoNoGoCard extends StatelessWidget {
  const _GoNoGoCard({required this.snap});

  final MissionSnapshot snap;

  @override
  Widget build(BuildContext context) {
    final label = switch (snap.goNoGo) {
      GoNoGo.go => 'GO',
      GoNoGo.wait => 'WAIT',
      GoNoGo.skip => 'SKIP',
    };
    final color = switch (snap.goNoGo) {
      GoNoGo.go => Aeo9Colors.accent,
      GoNoGo.wait => Aeo9Colors.warn,
      GoNoGo.skip => Aeo9Colors.danger,
    };

    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.08),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: color.withValues(alpha: 0.3)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            'Go / No-Go  ·  $label',
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
                  color: color,
                  fontWeight: FontWeight.w800,
                ),
          ),
          const SizedBox(height: 4),
          Text(
            snap.goReason,
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: Aeo9Colors.ink,
                ),
          ),
        ],
      ),
    );
  }
}

class _LinkRow extends StatelessWidget {
  const _LinkRow({required this.snap});

  final MissionSnapshot snap;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        _LinkDot(label: 'Starlink', up: snap.starlinkUp),
        const SizedBox(width: 12),
        _LinkDot(label: 'Radio', up: snap.radioUp),
        const SizedBox(width: 12),
        _LinkDot(label: 'Solar stowed', up: !snap.solarDeployed),
      ],
    );
  }
}

class _LinkDot extends StatelessWidget {
  const _LinkDot({required this.label, required this.up});

  final String label;
  final bool up;

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(
          width: 8,
          height: 8,
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            color: up ? Aeo9Colors.accent : Aeo9Colors.danger,
          ),
        ),
        const SizedBox(width: 6),
        Text(
          label,
          style: Theme.of(context).textTheme.labelLarge?.copyWith(
                color: Aeo9Colors.ink,
              ),
        ),
      ],
    );
  }
}

class _ActionButton extends StatelessWidget {
  const _ActionButton({
    required this.label,
    required this.onPressed,
    this.emphasize = false,
    this.danger = false,
  });

  final String label;
  final VoidCallback onPressed;
  final bool emphasize;
  final bool danger;

  @override
  Widget build(BuildContext context) {
    final bg = danger
        ? Aeo9Colors.danger
        : emphasize
            ? Aeo9Colors.link
            : Aeo9Colors.stone;

    return FilledButton(
      style: FilledButton.styleFrom(
        backgroundColor: bg,
        foregroundColor: Colors.white,
        padding: const EdgeInsets.symmetric(vertical: 14),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
      ),
      onPressed: onPressed,
      child: Text(
        label,
        style: const TextStyle(fontWeight: FontWeight.w800, letterSpacing: 0.8),
      ),
    );
  }
}
