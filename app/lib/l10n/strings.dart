import 'dart:convert';

import 'package:flutter/foundation.dart' show SynchronousFuture, visibleForTesting;
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

/// Strings from design/ui/strings_{zh,en}.json (copied to assets/i18n by
/// tool/sync_strings.sh) plus a few app-only keys in [_extra].
///
/// Placeholders use `{name}` exactly as in the design string table.
class S {
  S(this.languageCode, this._map);

  final String languageCode;
  final Map<String, String> _map;

  static const supportedLocales = [Locale('zh'), Locale('en')];

  static S of(BuildContext context) => Localizations.of<S>(context, S)!;

  bool get isZh => languageCode == 'zh';

  /// Translate [key]; missing keys return the key itself (visible in UI and caught by tests).
  String t(String key, [Map<String, Object?> params = const {}]) {
    var s = _map[key] ?? _extra[languageCode]?[key] ?? key;
    params.forEach((k, v) => s = s.replaceAll('{$k}', '$v'));
    return s;
  }

  bool has(String key) => _map.containsKey(key) || (_extra[languageCode]?.containsKey(key) ?? false);

  static final Map<String, S> _cache = {};

  static Future<S> load(String languageCode, {AssetBundle? bundle}) {
    final code = languageCode == 'en' ? 'en' : 'zh';
    final cached = _cache[code];
    if (cached != null) return SynchronousFuture(cached);
    return (bundle ?? rootBundle).loadString('assets/i18n/$code.json').then((raw) {
      final map = (jsonDecode(raw) as Map<String, dynamic>).map((k, v) => MapEntry(k, v as String));
      return _cache[code] = S(code, map);
    });
  }

  /// Loads both languages up front (main() and tests) so locale switches are synchronous.
  static Future<void> preload() async {
    await load('zh');
    await load('en');
  }

  /// App-only strings not (yet) in the design string table.
  static const Map<String, Map<String, String>> _extra = {
    'zh': {
      'x.demo_banner': '演示模式：未找到 stl2stp-core 转换引擎，转换结果只是占位文件。',
      'x.engine': '转换引擎',
      'x.open_path.title': '输入 STL 文件路径',
      'x.open_path.hint': '/path/to/model.stl',
      'x.open_path.picker_failed': '系统文件选择器不可用，请直接输入路径。',
      'x.output_path': '输出位置',
      'x.copied': '已复制',
      'x.copy_path': '复制路径',
      'x.saved_to': '已保存到 {path}',
      'x.share_unavailable': '此平台不支持系统分享，文件路径已复制到剪贴板。',
      'x.language.zh': '中文',
      'x.language.en': 'English',
      'x.verify_failed': '校验未通过，建议换“更严格”方式再转一次',
      'x.not_solid': '模型不封闭，已按曲面导出（没有实体）',
      'x.repaired': '输入模型有瑕疵，已自动修复',
      'x.clear_confirm': '确定清空所有转换记录吗？',
      'x.sample': '用示例模型试试',
    },
    'en': {
      'x.demo_banner': 'Demo mode: stl2stp-core was not found, outputs are placeholders.',
      'x.engine': 'Engine',
      'x.open_path.title': 'Enter STL file path',
      'x.open_path.hint': '/path/to/model.stl',
      'x.open_path.picker_failed': 'The system file picker is unavailable. Type a path instead.',
      'x.output_path': 'Saved at',
      'x.copied': 'Copied',
      'x.copy_path': 'Copy path',
      'x.saved_to': 'Saved to {path}',
      'x.share_unavailable': 'Sharing is not supported here. The file path was copied.',
      'x.language.zh': '中文',
      'x.language.en': 'English',
      'x.verify_failed': 'Verification failed. Try the Strict preset.',
      'x.not_solid': 'The model is open, exported as surfaces (no solid)',
      'x.repaired': 'The input had defects and was repaired',
      'x.clear_confirm': 'Clear all conversion history?',
      'x.sample': 'Try a sample model',
    },
  };
}

class SDelegate extends LocalizationsDelegate<S> {
  const SDelegate();
  @override
  bool isSupported(Locale locale) => true;
  @override
  Future<S> load(Locale locale) => S.load(locale.languageCode);
  @override
  bool shouldReload(SDelegate old) => false;
}

@visibleForTesting
Map<String, Map<String, String>> get extraStringsForTest => S._extra;
