enum FailsafeState { nominal, hold, rtl, land }

enum GoNoGo { go, wait, skip }

class Waypoint {
  const Waypoint({
    required this.name,
    required this.northM,
    required this.eastM,
    required this.altM,
    required this.leg,
    this.active = false,
  });

  final String name;
  final double northM;
  final double eastM;
  final double altM;
  final String leg;
  final bool active;
}

class MissionSnapshot {
  const MissionSnapshot({
    required this.simOnly,
    required this.homeName,
    required this.failsafe,
    required this.goNoGo,
    required this.goReason,
    required this.altitudeM,
    required this.speedMs,
    required this.headingDeg,
    required this.batteryPct,
    required this.energyMwh,
    required this.reserveMwh,
    required this.linkPct,
    required this.starlinkUp,
    required this.radioUp,
    required this.windMs,
    required this.obstacleClear,
    required this.animalEscapes,
    required this.geofenceRadiusM,
    required this.aircraftNorthM,
    required this.aircraftEastM,
    required this.waypoints,
    required this.nightCapable,
    required this.lidarOk,
    required this.solarDeployed,
  });

  final bool simOnly;
  final String homeName;
  final FailsafeState failsafe;
  final GoNoGo goNoGo;
  final String goReason;
  final double altitudeM;
  final double speedMs;
  final double headingDeg;
  final double batteryPct;
  final int energyMwh;
  final int reserveMwh;
  final double linkPct;
  final bool starlinkUp;
  final bool radioUp;
  final double windMs;
  final bool obstacleClear;
  final int animalEscapes;
  final double geofenceRadiusM;
  final double aircraftNorthM;
  final double aircraftEastM;
  final List<Waypoint> waypoints;
  final bool nightCapable;
  final bool lidarOk;
  final bool solarDeployed;

  String get failsafeLabel => switch (failsafe) {
        FailsafeState.nominal => 'NOMINAL',
        FailsafeState.hold => 'HOLD',
        FailsafeState.rtl => 'RTL',
        FailsafeState.land => 'LAND',
      };
}
