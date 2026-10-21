import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../theme/tokens.dart';

enum BannerKind { warn, info, success, neutral }

/// Coloured hint bar (ui-spec §6.1 warn/info colours; always icon + text).
class InfoBanner extends StatelessWidget {
  const InfoBanner({super.key, required this.kind, required this.text, this.action, this.onAction, this.icon});
  final BannerKind kind;
  final String text;
  final String? action;
  final VoidCallback? onAction;
  final IconData? icon;

  @override
  Widget build(BuildContext context) {
    final t = Tokens.of(context);
    final (bg, fg, ic) = switch (kind) {
      BannerKind.warn => (t.warnBg, t.warnFg, Icons.warning_amber_rounded),
      BannerKind.info => (t.infoBg, t.infoFg, Icons.info_outline),
      BannerKind.success => (t.success.withValues(alpha: 0.14), t.success, Icons.verified_user_outlined),
      BannerKind.neutral => (t.fill, t.label2, Icons.info_outline),
    };
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      decoration: BoxDecoration(color: bg, borderRadius: BorderRadius.circular(12)),
      child: Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Icon(icon ?? ic, color: fg, size: 20),
        const SizedBox(width: 8),
        Expanded(
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Text(text, style: TextStyle(color: fg, fontSize: 15, height: 1.33)),
            if (action != null)
              Padding(
                padding: const EdgeInsets.only(top: 6),
                child: OutlinedButton(
                  onPressed: onAction,
                  style: OutlinedButton.styleFrom(
                      foregroundColor: fg, side: BorderSide(color: fg.withValues(alpha: 0.5)), visualDensity: VisualDensity.compact),
                  child: Text(action!),
                ),
              ),
          ]),
        ),
      ]),
    );
  }
}

/// Small grey caption above a grouped section.
class SectionHeader extends StatelessWidget {
  const SectionHeader(this.text, {super.key, this.trailing});
  final String text;
  final Widget? trailing;
  @override
  Widget build(BuildContext context) {
    final t = Tokens.of(context);
    return Padding(
      padding: const EdgeInsets.fromLTRB(4, 20, 4, 8),
      child: Row(children: [
        Expanded(child: Text(text, style: TextStyle(fontSize: 13, color: t.label2))),
        ?trailing,
      ]),
    );
  }
}

class Surface extends StatelessWidget {
  const Surface({super.key, required this.child, this.padding = const EdgeInsets.all(12), this.radius = 12});
  final Widget child;
  final EdgeInsets padding;
  final double radius;
  @override
  Widget build(BuildContext context) {
    final t = Tokens.of(context);
    return Container(
      padding: padding,
      decoration: BoxDecoration(color: t.surface, borderRadius: BorderRadius.circular(radius)),
      child: child,
    );
  }
}

/// Label + big tabular number cell.
class StatCell extends StatelessWidget {
  const StatCell({super.key, required this.label, required this.value});
  final String label;
  final String value;
  @override
  Widget build(BuildContext context) {
    final t = Tokens.of(context);
    return Surface(
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(label, style: TextStyle(fontSize: 12, color: t.label2)),
        const SizedBox(height: 4),
        FittedBox(
          fit: BoxFit.scaleDown,
          alignment: Alignment.centerLeft,
          child: Text(value,
              style: TextStyle(fontSize: 20, fontWeight: FontWeight.w600, color: t.label, fontFeatures: tabular)),
        ),
      ]),
    );
  }
}

class StatGrid extends StatelessWidget {
  const StatGrid({super.key, required this.cells, this.columns = 2});
  final List<StatCell> cells;
  final int columns;
  @override
  Widget build(BuildContext context) {
    final rows = <Widget>[];
    for (var i = 0; i < cells.length; i += columns) {
      rows.add(Padding(
        padding: EdgeInsets.only(top: i == 0 ? 0 : 8),
        child: Row(children: [
          for (var j = i; j < i + columns; j++) ...[
            if (j > i) const SizedBox(width: 8),
            Expanded(child: j < cells.length ? cells[j] : const SizedBox()),
          ],
        ]),
      ));
    }
    return Column(children: rows);
  }
}

/// Coloured format tag (STEP blue, IGES orange).
class FormatTag extends StatelessWidget {
  const FormatTag(this.label, {super.key, this.size = 40});
  final String label;
  final double size;
  @override
  Widget build(BuildContext context) {
    final t = Tokens.of(context);
    final step = label == 'STEP';
    return Container(
      width: size,
      height: size,
      alignment: Alignment.center,
      decoration: BoxDecoration(color: step ? t.infoBg : t.warnBg, borderRadius: BorderRadius.circular(8)),
      child: Text(label,
          style: TextStyle(fontSize: size * 0.24, fontWeight: FontWeight.w700, color: step ? t.infoFg : t.warnFg)),
    );
  }
}

/// Progress ring (168pt, stroke 12) with an integer percent in the middle.
class ProgressRing extends StatelessWidget {
  const ProgressRing({super.key, required this.value, this.size = 168});
  final double value;
  final double size;
  @override
  Widget build(BuildContext context) {
    final t = Tokens.of(context);
    final pct = (value * 100).floor().clamp(0, 100);
    return Semantics(
      label: 'progress',
      value: '$pct%',
      child: SizedBox(
        width: size,
        height: size,
        child: CustomPaint(
          painter: _RingPainter(value, t.accent, t.fill),
          child: Center(
            child: Text('$pct%',
                style: TextStyle(fontSize: 34, fontWeight: FontWeight.w700, color: t.label, fontFeatures: tabular)),
          ),
        ),
      ),
    );
  }
}

class _RingPainter extends CustomPainter {
  _RingPainter(this.v, this.fg, this.bg);
  final double v;
  final Color fg, bg;
  @override
  void paint(Canvas canvas, Size size) {
    const w = 12.0;
    final rect = Rect.fromLTWH(w / 2, w / 2, size.width - w, size.height - w);
    canvas.drawArc(rect, 0, math.pi * 2, false, Paint()
      ..color = bg
      ..style = PaintingStyle.stroke
      ..strokeWidth = w);
    canvas.drawArc(rect, -math.pi / 2, math.pi * 2 * v.clamp(0, 1), false, Paint()
      ..color = fg
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round
      ..strokeWidth = w);
  }

  @override
  bool shouldRepaint(_RingPainter o) => o.v != v || o.fg != fg;
}

/// Icon + title + body + buttons, used by done/error screens.
class StatusIcon extends StatelessWidget {
  const StatusIcon({super.key, required this.icon, required this.color, this.size = 88});
  final IconData icon;
  final Color color;
  final double size;
  @override
  Widget build(BuildContext context) => Container(
        width: size,
        height: size,
        decoration: BoxDecoration(color: color.withValues(alpha: 0.15), shape: BoxShape.circle),
        child: Icon(icon, color: color, size: size * 0.5),
      );
}
