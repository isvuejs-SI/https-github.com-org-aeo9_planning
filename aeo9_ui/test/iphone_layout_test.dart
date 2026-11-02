import 'package:aeo9_ui/screens/mission_screen.dart';
import 'package:aeo9_ui/theme/aeo9_theme.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('iPhone-sized Mission has no overflow and shows actions', (tester) async {
    // iPhone 15 logical size
    await tester.binding.setSurfaceSize(const Size(393, 852));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    final errors = <FlutterErrorDetails>[];
    final old = FlutterError.onError;
    FlutterError.onError = (details) {
      errors.add(details);
      old?.call(details);
    };
    addTearDown(() => FlutterError.onError = old);

    await tester.pumpWidget(
      MaterialApp(
        theme: buildAeo9Theme(),
        home: const MissionScreen(),
      ),
    );
    await tester.pump(const Duration(milliseconds: 950));

    expect(find.text('AEO-9'), findsOneWidget);
    expect(find.text('HOLD'), findsOneWidget);
    expect(
      errors.where((e) => e.toString().contains('overflowed')),
      isEmpty,
      reason: errors.map((e) => e.exceptionAsString()).join('\n'),
    );
  });
}
