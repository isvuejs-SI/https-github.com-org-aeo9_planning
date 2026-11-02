import 'package:aeo9_ui/main.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('AEO-9 app boots into Mission screen', (tester) async {
    await tester.binding.setSurfaceSize(const Size(1280, 800));
    addTearDown(() => tester.binding.setSurfaceSize(null));

    await tester.pumpWidget(const Aeo9App());
    await tester.pump(const Duration(milliseconds: 950));

    expect(find.text('AEO-9'), findsOneWidget);
    expect(find.text('NOMINAL'), findsOneWidget);
  });
}
