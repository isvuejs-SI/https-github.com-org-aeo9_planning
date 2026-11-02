import 'package:flutter/material.dart';

import 'screens/mission_screen.dart';
import 'theme/aeo9_theme.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const Aeo9App());
}

class Aeo9App extends StatelessWidget {
  const Aeo9App({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'AEO-9',
      debugShowCheckedModeBanner: false,
      theme: buildAeo9Theme(),
      home: const MissionScreen(),
    );
  }
}
