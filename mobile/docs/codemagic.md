# GitHub → Codemagic → TestFlight

源码仓库：https://github.com/bobo7727/stl2stp 。仓库根的 codemagic.yaml 包含两个手动启动的工作流，项目工作目录固定为 mobile/app。

| 工作流 | 用途 | Apple 发布凭据 |
| --- | --- | --- |
| ios-simulator | Mac 上编译真实 OCCT/C++ 核心、运行 Flutter 分析及测试、生成 arm64 模拟器 App ZIP | 不需要签名 |
| ios-testflight | 签名 Release IPA，验证原生 C API 符号后上传 App Store Connect/TestFlight | 需要下述配置 |

两者都固定 Flutter 3.38.7、最低 iOS 14，使用 Codemagic 当前 latest Xcode 并输出实际版本。首次会从固定 URL 下载并校验 OCCT 8.0.1，再为 iPhone 和模拟器构建。只缓存下载包/pub 缓存，不缓存可能与 Xcode/架构不匹配的原生 build。工作流没有自动 push 触发器，先手动验证。

## 1. 连接 GitHub 仓库

在 Codemagic 用自己的账号登录，添加 GitHub 应用并授权访问 bobo7727/stl2stp。选择 master 和仓库根的 codemagic.yaml。GitHub 私有仓库需要给 Codemagic GitHub 集成相应访问权限；本地的个人 SSH 私钥不需要上传给 Codemagic。

先启动 ios-simulator，确认 Mac 工具链、插件和原生链接真正通过，再启动发布工作流。模拟器 ZIP 不是可安装到 iPhone 的签名 IPA。

## 2. 准备 App Store Connect

在 Apple Developer/App Store Connect 中确认 Bundle ID 为 `com.stl2stp.stl2stpApp`，并创建对应的 App 记录。记录 App Information 中的 **Apple ID（纯数字）**。如果使用自己的 Bundle ID，先同步修改 Xcode Runner 配置和 codemagic.yaml 中的 bundle_identifier。

创建专用于 CI 的 App Store Connect API Key，按官方要求给予 App Manager 权限。在 Codemagic 的 Apple Developer Portal/App Store Connect 集成中保存 Issuer ID、Key ID 和 .p8，集成名称填写：

```text
stl2stp-app-store-connect
```

这些私密值只配置在 Codemagic，不写 YAML、不提交 Git、不发到聊天。要求和流程见 [Codemagic 发布文档](https://docs.codemagic.io/yaml-publishing/app-store-connect/)。

## 3. 配置签名身份与环境变量

在 Codemagic Team settings → codemagic.yaml settings → Code signing identities 中准备匹配 Bundle ID 的 Apple Distribution 证书及 App Store provisioning profile，可通过已连接的 Apple 集成获取/生成，再确认身份可用。YAML 的 ios_signing 按 app_store 类型和 Bundle ID 取这些身份；仅在 YAML 写 bundle_identifier 不代表证书已经准备好。参见 [官方签名说明](https://docs.codemagic.io/yaml-code-signing/signing-ios/)。

创建变量组 `ios_release`，添加：

| 变量 | 值 |
| --- | --- |
| APP_STORE_APPLE_ID | 对应 App 的数字 Apple ID；不是 Bundle ID、Apple 账号邮箱或 Team ID |

该组已由 ios-testflight 引用。不要将 Apple API 私钥明文放进这个仓库。

## 4. 执行 TestFlight 工作流

选择 master 的最新提交，手动启动 ios-testflight。它会先运行符号回归与 Flutter 测试，然后构建真实核心、安装 Pods、应用签名配置，从 App Store Connect 查询递增的 build number，最后生成 IPA。

发布前会对归档中的 Runner 执行实际符号检查，确认包含全部 C API（包括 stl2stp_weld_vertices），避免 App 因缺少真实核心符号而退回演示模式。测试失败、依赖构建失败或符号缺失都会停止工作流。

Codemagic 成功上传后，还要在 App Store Connect 查看 Apple 的处理/测试审核结果，并配置 TestFlight 测试人员。当前 YAML 只提交 TestFlight，`submit_to_app_store` 为 false；正式 App Store 审核在功能、商店资料及真机验收就绪后进行。上传成功不等于 Apple 已完成处理或发布。

## 5. 首次构建遇到问题

从失败步骤查看日志；原生库生成在 mobile/app/ios/Stl2StpCore，生成的 xcconfig 已被 Runner 的 Debug/Release 配置包含。证书/描述文件/商店应用不匹配时修正 Codemagic 和 Apple 账号配置。不要为了通过构建开启 DemoEngine 或忽略失败的测试。

本配置需在真实 Codemagic Mac 上运行后才能确认 iOS 构建通过。WSL 的脚本/单元验证不代替 Swift、CocoaPods、签名、设备和 TestFlight 验证。

参考：[Flutter 工作流](https://docs.codemagic.io/yaml-quick-start/building-a-flutter-app/)、[YAML 工作目录与校验](https://docs.codemagic.io/yaml-basic-configuration/yaml-getting-started/)。
