import '../models/mission_models.dart';

/// Sim-first snapshot aligned with AEO-9 Tusayan staging placeholders.
MissionSnapshot mockMissionSnapshot() {
  return const MissionSnapshot(
    simOnly: true,
    homeName: 'tusayan_staging',
    failsafe: FailsafeState.nominal,
    goNoGo: GoNoGo.go,
    goReason: 'Wind, battery, fence, schedule clear',
    altitudeM: 45.0,
    speedMs: 6.2,
    headingDeg: 18,
    batteryPct: 78,
    energyMwh: 156000,
    reserveMwh: 50000,
    linkPct: 92,
    starlinkUp: true,
    radioUp: true,
    windMs: 4.1,
    obstacleClear: true,
    animalEscapes: 0,
    geofenceRadiusM: 500,
    aircraftNorthM: 120,
    aircraftEastM: 40,
    nightCapable: true,
    lidarOk: true,
    solarDeployed: false,
    waypoints: [
      Waypoint(name: 'launch', northM: 0, eastM: 0, altM: 0, leg: 'launch'),
      Waypoint(
        name: 'transit_n',
        northM: 200,
        eastM: 50,
        altM: 50,
        leg: 'transit',
        active: true,
      ),
      Waypoint(
        name: 'film_a',
        northM: 250,
        eastM: 80,
        altM: 45,
        leg: 'filming',
      ),
      Waypoint(name: 'return_home', northM: 0, eastM: 0, altM: 0, leg: 'return'),
      Waypoint(name: 'overnight', northM: 0, eastM: 0, altM: 0, leg: 'overnight'),
    ],
  );
}
