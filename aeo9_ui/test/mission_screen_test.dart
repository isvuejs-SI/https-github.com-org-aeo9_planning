import 'package:aeo9_ui/screens/mission_screen.dart';
import 'package:aeo9_ui/theme/aeo9_theme.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('Mission screen shows brand, state, and actions', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1280, 860));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    await tester.pumpWidget(
      MaterialApp(
        theme: buildAeo9Theme(),
        home: const MissionScreen(),
      ),
    );
    await tester.pump(const Duration(milliseconds: 950));

    expect(find.text('AEO-9'), findsOneWidget);
    expect(find.text('Mission'), findsOneWidget);
    expect(find.text('NOMINAL'), findsOneWidget);
    expect(find.text('SIM ONLY'), findsOneWidget);
    expect(find.text('HOLD'), findsOneWidget);
    expect(find.text('RTL'), findsOneWidget);
    expect(find.text('LAND'), findsOneWidget);

    await tester.ensureVisible(find.text('HOLD'));
    await tester.tap(find.text('HOLD'));
    await tester.pump();
    expect(find.textContaining('Sim request: HOLD'), findsOneWidget);
  });
}
