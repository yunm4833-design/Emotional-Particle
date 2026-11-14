# Emotional Particle 0.5

实时颗粒音频效果器：8 种心情、128 个固定颗粒声部、立体声 4 秒历史缓冲与冻结、800ms 参数过渡、12 个细节旋钮、响应输入声音的粒子界面。

## 本次改动

- 界面参数名称由约 11–13px 提高到 18–20px，旋钮数值由 15px 提高到 20px，心情按钮提高到 20px；增加文字对比度和控件空间。
- 防止相同程序切换反复通知宿主，增加非有限参数保护和宿主旁路处理。
- 旋钮进度弧改为填充环形扇区，避开报告栈中出现的 JUCE 路径描边调用；这是针对性缓解措施，尚不能证明报告中最初的堆损坏来源已被完全修复。
- Windows 使用 Visual Studio 2022/MSVC 正式构建，输出调试符号供后续诊断。
- macOS 提供 Apple Silicon + Intel 通用 VST3/AU 构建流程。
- 增加可选 AAX 编译目标；需要自行取得 Avid SDK、完成 PACE 签名和 Pro Tools 实测。

## 在 GitHub 下载

已发布的预览版可在 [GitHub Releases](https://github.com/yunm4833-design/Emotional-Particle/releases) 下载；预览版仍需在 Live 中复测。每次提交的构建产物可进入仓库 **Actions → Build and validate → 成功的运行记录 → Artifacts**：

- `EmotionalParticle-Windows`：Windows VST3 ZIP。
- `EmotionalParticle-macOS`：通用 VST3 + AU ZIP。
- `Validation-*`：测试结果、真实界面截图及 Windows 调试符号。

GitHub 的 Artifacts 可能需要登录才能下载，且有保留期限。失败的运行不代表已经生成可用版本。公共构建不包含 AAX；正式发布的预览版与这些临时构建产物分开保存。

Windows 安装：关闭 Live，将完整的 `Emotional Particle.vst3` 文件夹放入 `C:\Program Files\Common Files\VST3`，覆盖旧版本后重新扫描。保留完整文件夹结构。默认插件 ID 与 0.4 一致，旧工程可识别。

Mac 安装：VST3 放入 `~/Library/Audio/Plug-Ins/VST3/`，AU 的 `.component` 放入 `~/Library/Audio/Plug-Ins/Components/`。macOS 最低构建目标 11.0。CI 采用临时签名，尚未完成 Apple Developer ID 签名和公证，不能称为已公证的商业安装包。

## 本地编译

Windows：安装 Visual Studio 2022 的 C++ 桌面开发组件及 CMake，在 PowerShell 运行 `./build-windows.ps1`。

macOS：安装 Xcode 和命令行工具，运行 `./build-macos.sh`。生成 Universal VST3、AU 与独立程序。

构建使用固定提交的 JUCE 7.0.12，脚本自动应用已记录的编译器兼容修补，以及 macOS SDK 原生窗口截图 API 兼容处理。不包含 Avid 的专有 SDK。AAX 操作详见 [docs/AAX.md](docs/AAX.md)。

## 稳定性状态

你提供的 Live 12.4.16 报告显示：异常为 Windows 访问违规，故障线程的栈中包含旋钮绘制、JUCE 描边及堆分配相关地址。当前是根据报告采取修复和缓解措施，仍需要在 Live 上复测，不能保证已彻底解决所有崩溃。

仓库不包含原始崩溃报告、个人工程或 Live 配置。脱敏分析和复测步骤见 [docs/STABILITY.md](docs/STABILITY.md)。

本插件只支持单声道输入/输出和立体声输入/输出。重采样不是带限算法，高音高/失真下可能产生混叠；冻结音频不随工程保存。粒子背景包含艺术化光点，真实颗粒核心单独跟随音频生命周期。

## 许可证

原创代码 MIT；JUCE 7.0.12 使用 GPLv3 或适用的商业许可。组合后的二进制分发须遵守 JUCE 对应许可，不能因为原创代码为 MIT 就忽略它。字体 Questrial 使用 SIL OFL。AAX SDK 和 PACE 的授权条款单独适用。第三方说明见 THIRD-PARTY.md。
