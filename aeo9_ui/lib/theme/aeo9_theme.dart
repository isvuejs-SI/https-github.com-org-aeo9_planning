import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

/// Field-ops palette for AEO-9 (Tusayan staging / canyon light).
/// Not a generic purple SaaS theme and not a cream/terracotta landing page.
class Aeo9Colors {
  static const skyTop = Color(0xFFB7C9D6);
  static const skyMid = Color(0xFFD8E2E8);
  static const ground = Color(0xFFC4B49A);
  static const stone = Color(0xFF2F3A42);
  static const ink = Color(0xFF1B242B);
  static const panel = Color(0xE6F4F7F8);
  static const panelSolid = Color(0xFFF4F7F8);
  static const accent = Color(0xFF0F6E56);
  static const accentSoft = Color(0xFF1F8A6A);
  static const warn = Color(0xFFB86E1C);
  static const danger = Color(0xFFB42318);
  static const link = Color(0xFF1B6B9A);
  static const fence = Color(0x662F6B8A);
  static const mapLand = Color(0xFFD9CDB8);
  static const mapShade = Color(0xFFB9A88C);
}

ThemeData buildAeo9Theme() {
  final display = GoogleFonts.soraTextTheme();
  final body = GoogleFonts.sourceSans3TextTheme();

  return ThemeData(
    useMaterial3: true,
    brightness: Brightness.light,
    colorScheme: ColorScheme.light(
      primary: Aeo9Colors.accent,
      secondary: Aeo9Colors.link,
      surface: Aeo9Colors.panelSolid,
      onSurface: Aeo9Colors.ink,
      error: Aeo9Colors.danger,
    ),
    scaffoldBackgroundColor: Colors.transparent,
    textTheme: body.copyWith(
      displayLarge: display.displayLarge?.copyWith(
        color: Aeo9Colors.ink,
        fontWeight: FontWeight.w700,
        letterSpacing: -1.2,
      ),
      displayMedium: display.displayMedium?.copyWith(
        color: Aeo9Colors.ink,
        fontWeight: FontWeight.w700,
        letterSpacing: -0.8,
      ),
      headlineMedium: display.headlineMedium?.copyWith(
        color: Aeo9Colors.ink,
        fontWeight: FontWeight.w600,
      ),
      titleLarge: display.titleLarge?.copyWith(
        color: Aeo9Colors.ink,
        fontWeight: FontWeight.w600,
      ),
      titleMedium: display.titleMedium?.copyWith(
        color: Aeo9Colors.ink,
        fontWeight: FontWeight.w600,
      ),
      labelLarge: body.labelLarge?.copyWith(
        color: Aeo9Colors.stone,
        fontWeight: FontWeight.w600,
        letterSpacing: 0.4,
      ),
    ),
  );
}
