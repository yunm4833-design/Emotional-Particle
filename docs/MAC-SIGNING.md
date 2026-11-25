# macOS 签名与公证

GitHub 常规构建提供临时签名的 AU 和 VST3 ZIP。AU 通过 `auval` 说明它能在测试机器上通过 Apple 的 Audio Unit 验证。下载文件的信任由 Gatekeeper 单独检查。要分发通常可以直接安装的 Mac 版本，需要 Apple Developer Program 团队的 Developer ID 证书和 Apple 公证。

发布者在 Mac 上准备 **Developer ID Application**、**Developer ID Installer** 两种证书及对应私钥，并用 `xcrun notarytool store-credentials` 在钥匙串中设置公证配置。凭据从发布者的钥匙串读取。

```sh
./scripts/notarize-macos.sh EmotionalParticle-0.6-macOS-Universal-VST3-AU.zip EmotionalParticle-0.6-macOS.pkg \
  'Developer ID Application: YOUR NAME (TEAMID)' \
  'Developer ID Installer: YOUR NAME (TEAMID)' \
  'YOUR-NOTARY-PROFILE'
```

脚本签署两个插件，打包签名安装器，等待 Apple 接受公证，附加公证票据，再检查签名和 Gatekeeper 结果。生成的安装器把 AU 和 VST3 放在系统的 `/Library/Audio/Plug-Ins/` 下。发布前用干净的 Mac 下载安装器并验证 Logic 中的 AU 扫描。

如果 Logic 的插件管理器显示 **验证失败**，查看该次 Logic 验证的错误。这个状态来自 AU 验证，能与 macOS 的开发者或文件安全提示分别诊断。
