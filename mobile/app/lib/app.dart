import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';

import 'core/engine.dart';
import 'l10n/strings.dart';
import 'screens/core_load_error_screen.dart';
import 'screens/flow.dart';
import 'screens/home_screen.dart';
import 'screens/onboarding_screen.dart';
import 'state/app_state.dart';
import 'theme/tokens.dart';

/// Wraps the whole UI so tests/tools can capture screenshots of it.
final screenshotKey = GlobalKey();
final navigatorKey = GlobalKey<NavigatorState>();

class Stl2StpApp extends StatelessWidget {
  const Stl2StpApp({super.key, required this.state, this.openOnStart, this.homeOverride});
  final AppState state;

  /// Tests: start on a specific screen instead of home/onboarding.
  final Widget? homeOverride;

  /// File passed on the command line / "open with" (opened right after start).
  final String? openOnStart;

  @override
  Widget build(BuildContext context) {
    return AppScope(
      state: state,
      child: Builder(builder: (context) {
        final st = AppScope.of(context);
        return MaterialApp(
          title: 'STL2STP',
          debugShowCheckedModeBanner: false,
          navigatorKey: navigatorKey,
          theme: buildTheme(Brightness.light),
          darkTheme: buildTheme(Brightness.dark),
          themeMode: st.themeMode,
          locale: st.locale,
          supportedLocales: S.supportedLocales,
          localizationsDelegates: const [
            SDelegate(),
            GlobalMaterialLocalizations.delegate,
            GlobalWidgetsLocalizations.delegate,
            GlobalCupertinoLocalizations.delegate,
          ],
          localeResolutionCallback: (locale, supported) =>
              locale != null && locale.languageCode == 'en' ? const Locale('en') : const Locale('zh'),
          builder: (context, child) => RepaintBoundary(key: screenshotKey, child: child),
          home: homeOverride ??
              (st.engine is UnavailableEngine
                  ? CoreLoadErrorScreen(detail: (st.engine as UnavailableEngine).detail)
                  : (st.onboardingSeen ? _Home(openOnStart: openOnStart) : const OnboardingScreen())),
        );
      }),
    );
  }
}

class _Home extends StatefulWidget {
  const _Home({this.openOnStart});
  final String? openOnStart;
  @override
  State<_Home> createState() => _HomeState();
}

class _HomeState extends State<_Home> {
  @override
  void initState() {
    super.initState();
    final path = widget.openOnStart;
    if (path != null) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) openModel(context, path);
      });
    }
  }

  @override
  Widget build(BuildContext context) => const HomeScreen();
}
