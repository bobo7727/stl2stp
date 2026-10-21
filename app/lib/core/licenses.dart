import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

/// OCCT licence files bundled from the OCCT 8.0.1 install
/// (`share/doc/opencascade/`), shown in Settings › About › 开源许可 and in
/// Flutter's licence page next to the Dart/Flutter package licences.
const occtLicenseAssets = [
  'assets/licenses/occt/LICENSE_LGPL_21.txt',
  'assets/licenses/occt/OCCT_LGPL_EXCEPTION.txt',
];
const occtPackageName = 'Open CASCADE Technology (OCCT) 8.0.1';

bool _registered = false;

/// Adds the OCCT licence texts to [LicenseRegistry] once (packages from
/// pubspec are collected by Flutter automatically).
void registerAppLicenses({AssetBundle? bundle}) {
  if (_registered) return;
  _registered = true;
  LicenseRegistry.addLicense(() => occtLicenseEntries(bundle: bundle));
}

/// The OCCT entries handed to [LicenseRegistry] (one per bundled text).
Stream<LicenseEntry> occtLicenseEntries({AssetBundle? bundle}) async* {
  final b = bundle ?? rootBundle;
  for (final asset in occtLicenseAssets) {
    yield LicenseEntryWithLineBreaks([occtPackageName], await b.loadString(asset));
  }
}

/// Both OCCT texts, LGPL first, separated by a blank line.
Future<String> loadOcctLicenseText({AssetBundle? bundle}) async {
  final b = bundle ?? rootBundle;
  final parts = [for (final a in occtLicenseAssets) await b.loadString(a)];
  return parts.join('\n\n${'—' * 20}\n\n');
}
