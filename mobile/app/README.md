# STL2STP Flutter App

使用 Flutter 3.38.7 / Dart 3.10.7，通过 dart:ffi 调用相邻 core/ 的真实转换核心。

首次构建见 [QUICKSTART.md](../QUICKSTART.md)。从 mobile/ 运行 `bash tools/bootstrap-occt.sh`、`bash tools/verify.sh --cad`，然后在本目录执行 `flutter pub get`、`flutter analyze`、`flutter test`。Linux 运行时需先生成 build/core-shared/libstl2stp-core.so。

iOS 最低版本为 14。Mac 上先执行 `bash app/tool/ios/build_ios_core.sh`（工作目录 mobile/），再进入 app/ 准备 Flutter/Pods 和运行模拟器。后台与内存通道已集成，实际构建、签名和真机验证见 [Mac 指南](../docs/mac-setup-guide.md)。

找不到真实核心时 App 会退回演示模式；端到端验收必须确认真实导出和回读成功。
