<div align="center">
  <img src="app/assets/readme-logo.svg" width="112" height="112" alt="StadiaLink 标志">
  <h1>StadiaLink</h1>
  <p><strong>你的 Stadia 手柄，由你定义。</strong></p>
  <p>Windows 11 x64 上的 Stadia 手柄配置工具 · 版本 1.0.2</p>
  <p><a href="https://github.com/szninjacat/StadiaLink-Win11-Fix/releases/tag/v1.0.2-hotfix"><strong>⬇ 下载</strong></a> · <a href="https://github.com/szninjacat/StadiaLink-Win11-Fix/issues">反馈问题</a> · <a href="LICENSE">GPL-3.0</a></p>
  <p>🪟 Windows 11 x64 &nbsp; · &nbsp; 🎮 XInput &nbsp; · &nbsp; 🔗 USB + 蓝牙</p>
  <p><a href="README.es.md"><img src="app/assets/flag-es.svg" width="24" height="16" alt="ES"> Español</a> &nbsp; | &nbsp; <a href="README.en.md"><img src="app/assets/flag-en.svg" width="24" height="16" alt="EN"> English</a> &nbsp; | &nbsp; <a href="README.fr.md"><img src="app/assets/flag-fr.svg" width="24" height="16" alt="FR"> Français</a> &nbsp; | &nbsp; <a href="README.de.md"><img src="app/assets/flag-de.svg" width="24" height="16" alt="DE"> Deutsch</a> &nbsp; | &nbsp; <strong>🇨🇳 简体中文</strong></p>
</div>

> **简体中文修复分支（2026-11-09）**：本分支修复 Windows 11 23H2 上蓝牙震动失效，将界面默认语言改为简体中文，并修复窗口「×」直接退出程序的问题。完整说明见 [RELEASE_NOTES.md](RELEASE_NOTES.md)、[CHANGELOG.md](CHANGELOG.md) 与 [docs/CHANGES.md](docs/CHANGES.md)；编译产物在 [dist/](dist/)。

---

## 🎮 功能

- USB 与蓝牙连接，输出兼容 XInput。
- 按键映射、键盘快捷键与按住说话（push to talk）。
- 摇杆灵敏度、死区与扳机调整。
- 按马达分别调节震动，支持连续测试与手动停止。
- 配置文件的导入与导出。
- 电量显示、通知、系统托盘与开机自启。
- 界面支持西班牙语、英语、法语、德语和**简体中文**（默认）。

## 🚀 安装

从 [Releases](https://github.com/szninjacat/StadiaLink-Win11-Fix/releases/tag/v1.0.2-hotfix) 下载 `StadiaLink.Setup.exe` 运行，并在提示时为驱动安装授予管理员权限。通过 USB 连接手柄，或在 Windows 蓝牙设置中配对后开机，StadiaLink 会自动识别。安装包内含全部组件与对应源码，可离线安装。

蓝牙连接要求手柄本身已具备蓝牙固件，StadiaLink 不会修改固件。程序仅依赖 .NET Framework 4.8 与 Windows 自带组件，不需要第三方手柄模拟器或额外运行时。

本分支的编译产物也已放在 [dist/](dist/) 中，可直接双击 `StadiaLink.Setup.exe` 安装。**升级前请先退出托盘中正在运行的旧版 StadiaLink**（单实例锁会阻止安装）；若安装后手柄未出现，关开一次蓝牙或重新插拔即可。

用同版本的 `SHA256SUMS.txt` 核对下载文件：

```powershell
Get-FileHash .\StadiaLink.Setup.exe -Algorithm SHA256
```

## 🛠 从零构建

### 环境要求

- Windows 11 x64 与系统自带的 Windows PowerShell 5.1。
- [Git for Windows](https://git-scm.com/downloads/win)，或 GitHub 提供的源码 ZIP。
- [Visual Studio 2022 或 Build Tools 2022](https://visualstudio.microsoft.com/downloads/)，需包含**使用 C++ 的桌面开发**、MSVC x64 与 Windows SDK **10.0.26100.0**。
- .NET Framework **4.8**。脚本使用 `%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe` 与 Windows 的 WPF 程序集，不需要 Node.js 或 `dotnet build`。
- 首次编译驱动需要联网：脚本会从 NuGet 下载 `Microsoft.Windows.WDK.x64` **10.0.26100.6584**（UMDF **2.31**），并缓存到 `.build/driver/.wdk`。

### 完整构建

在工作目录打开 Windows PowerShell。编译不需要管理员权限，安装驱动才需要。

```powershell
git clone https://github.com/szninjacat/StadiaLink-Win11-Fix.git
cd StadiaLink

# 若脚本被阻止执行，仅对本次会话生效。
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned

# 驱动、应用、原生库与安装程序。
.\app\package.ps1 -Driver

# 输入翻译与应用模型测试。
.\tests\run.ps1
```

安装程序生成在 `release/`，对应源码以 `Source.zip` 内嵌其中；`.build/` 只存放构建临时文件。若使用源码 ZIP，请解压后在包含 `app`、`driver`、`tests` 的目录中打开 PowerShell；Windows 若标记为来自 Internet，请先检查脚本再解除锁定。上面的策略设置不会改变机器的永久配置。

| 路径 | 产物 |
| --- | --- |
| `release/StadiaLink.exe` | x64 WPF 应用。 |
| `release/StadiaLink.exe.config` | .NET Framework 配置。 |
| `release/StadiaDevice.dll` | 原生电量读取库。 |
| `release/StadiaLink.Setup.exe` | 含组件与内嵌 `Source.zip` 的安装程序。 |
| `release/StadiaLink.Setup.exe.config` | 安装程序配置。 |
| `release/SHA256.txt` | 本机所生成安装程序的哈希。 |
| `driver/out/pkg/` | 编译出的 UMDF 驱动包。 |
| `.build/` | 依赖、目标文件与测试结果。 |

运行 `release/StadiaLink.Setup.exe` 才会安装你的构建结果；只打开应用不会安装驱动。分步构建：

```powershell
.\driver\build.ps1       # 仅驱动。
.\app\build.ps1          # 应用与原生库。
.\app\package.ps1        # 用已有驱动构建安装程序。
.\tests\run.ps1          # 需要已编译的应用。
```

缺少 MSVC 时检查 Visual Studio 的 C++ 工作负载；缺少头文件/库时安装指定 SDK；WDK 下载失败时检查 NuGet 连通性。驱动会写入构建日期与版本，因此不同批次构建的二进制不一定完全相同。

### 本分支新增的构建开关

默认行为完全不变，仅在特殊环境（CI、受限沙箱）中使用：

| 环境变量 | 作用 |
| --- | --- |
| `STADIALINK_VC_ENV_READY=1` | 跳过 `vcvars64.bat` 导入，由调用方自行准备 `INCLUDE` / `LIB` / `PATH`。适用于无法运行 `reg.exe` 的主机。 |
| `STADIALINK_SKIP_STAGE_CLEANUP=1` | 跳过 `app/package.ps1` 的暂存目录清理，适用于禁止递归删除的主机。 |

## ⚙️ 使用

震动增益在拖动滑块时即时应用并保存。按键、语言等配置改动需点击**保存并应用**；切换标签页会丢弃未保存的配置改动。开机自启复选框即时保存，并在该用户登录时以托盘方式启动。连续震动可通过**停止**、切换标签页、断开连接或退出应用来终止。

快捷键要求应用保持运行（可最小化为托盘）。请在游戏或语音软件中设置相同的按住说话按键。界面关闭后，驱动仍保留已应用的 XInput 配置。设置中显示的时间数据衡量的是 HID 查询与界面更新，不包含游戏与显示器的完整物理延迟。

配置文件保存在 `%LOCALAPPDATA%\StadiaStudio\profiles.json`，程序安装在 `%LOCALAPPDATA%\Programs\StadiaStudio`。这些内部名称沿用旧版本以保持兼容。界面语言可在「设置」中切换，首次启动（注册表无语言设置时）默认为**简体中文**。

点击窗口右上角 **×** 或按 `Alt+F4` 现在都会把程序最小化到系统托盘（状态栏），而不是退出；双击托盘图标或右键选择**打开 StadiaLink** 可恢复窗口，只有托盘菜单里的**退出**才会真正结束进程。最小化按钮的行为不变，驱动已应用的 XInput 配置在界面隐藏后继续有效。

## 🔋 电量检测

无需激活固件功能或安装额外工具。识别到手柄后，`StadiaDevice.dll` 会在后台每 **30 秒**读取一次电量，并显示在界面与托盘图标上。

- **蓝牙**：Windows 需枚举已配对手柄的 BLE Battery Service（`0x180F`）。库直接读取该设备上的 Battery Level（`0x2A19`），校验单字节值在 0–100 之间；不写入电池服务，也不修改 HID。
- **USB**：定位 Stadia WinUSB 接口 `VID_18D1 / PID_9400 / MI_00`，用控制请求 `0x83` 请求数值、`0x84` 取回并校验格式与百分比；不写入固件。
- **充电**：USB 连接显示**充电中**，读数为 100 % 时显示**已充满**。这是基于连接状态的推断，不测量电流，也不确认物理上正在充电。
- **通知**：开启后，连接、断开、电量 **≤ 15 %** 与 **100 %** 均会提示；满电提示要求读数有效。

显示**电量不可用**表示读取失败，不会臆造百分比。蓝牙下可关闭再打开手柄等待下一次读取，必要时删除配对后重新配对以让 Windows 重新枚举服务；USB 下检查数据线与 WinUSB 接口。按键可用并不保证 Windows 一定暴露电量接口。

## 🐛 蓝牙震动兼容性（本分支重点）

蓝牙震动通过未公开的私有 WinRT 接口 `GattClientDevice` 写入 HID 输出报告。原版驱动只硬编码了从 Windows **26200** 逆向得到的设备接口 IID `6BBEDE1F-797F-5C17-8AC7-3F342037CB59`；在 Windows 11 23H2（**Build 22631**）上该类实际使用 `0799A6A6-E942-5218-8A0E-7C9B996E4158`，因此 `QueryInterface` 返回 `E_NOINTERFACE`（`0x80004002`），表现为「蓝牙连接正常但没有震动」。

本分支的修复：

- 按已知版本依次尝试两个 IID；遇到未知 build 时回退到 `IInspectable::GetIids`，让对象自己声明实现的私有接口。
- 打开链路的每一步记录到 status feature report（第 60、61 字节）与 `driver/status.ps1`，可直接看出停在哪一步、用了哪个 IID。
- USB 震动、输入与 XInput 翻译链路未改动。

排障命令：

```powershell
.\driver\status.ps1                      # 查看阶段、IID、写入计数与 writeHRESULT
python .\tests\probe-private-gatt.py     # 只读枚举本机私有接口 IID
```

成功标志：`probeStatus=0x00000000`、`report=5 type=2 value=0x0044`、`writes` 持续增长。阶段 `5` 表示 IID 不匹配，`6` 及以后表示后续链路或布局差异。

**跨任意 Windows 版本仍不保证**：私有 ABI 没有兼容契约，运行时兜底只负责发现设备接口 IID，不适配 vtable 方法顺序；私有类本身也可能被移除。换系统后请先用上面两个工具确认。

## 💬 问题反馈、贡献与卸载

提交 [issue](https://github.com/szninjacat/StadiaLink-Win11-Fix/issues) 时请附上 Windows 版本、应用版本、USB 或蓝牙连接方式及复现步骤。安装日志位于 `%LOCALAPPDATA%\StadiaStudio\logs\driver-install.log`，分享前请删除个人信息。

欢迎 pull request：请说明验证步骤，改动配置或输入翻译时运行 `tests/run.ps1`。不要提交下载的依赖、生成的二进制、个人配置文件或固件研究材料。

通过 Windows 的**已安装的应用**卸载。若只想从自行构建的副本中移除驱动：

```powershell
.\driver\install.ps1 -Uninstall
```

该命令需要管理员权限，会移除本驱动的驱动包并把手柄交还给 Windows 自带的 HID 驱动，可能需要重新连接。

## 🤝 兼容性、致谢与许可

蓝牙震动依赖私有 Windows 接口，已在 Windows 11 build **26200** 与 **22631** 上验证；系统更新可能改变这些接口。耳机孔音频仅通过 USB 工作；不提供蓝牙音频、内置麦克风激活与 Wi-Fi 激活。仅支持 **x64**、UMDF 2.31（Win10 1809+ / Win11）与 .NET Framework 4.8。驱动为自签名，未做微软签名，新机器需重建证书信任。

**已并入的第三方代码：[aisk/WinStadia](https://github.com/aisk/WinStadia)**。它是本仓库原生驱动的基础，包含 StadiaLink 的修改：UMDF 结构、Stadia 到 XInput 的输入翻译、`driver/` 中的 USB/蓝牙传输，以及派生脚本与 INF。

上游仓库：**https://github.com/aisk/WinStadia**；参考提交：[e9e84a0a24c3cc13bf7cc7ac2b8baa2b653ce80a](https://github.com/aisk/WinStadia/commit/e9e84a0a24c3cc13bf7cc7ac2b8baa2b653ce80a)。保留原有版权声明。StadiaLink 及其修改以 **GPL-3.0** 发布，见 [LICENSE](LICENSE) 与 [UPSTREAM-README.md](UPSTREAM-README.md)。本项目与 Google、Microsoft 无隶属关系。

## ✨ 开发方式

项目由负责人借助 **ChatGPT / Codex** 以 vibe coding 方式管理与开发：负责人定义功能并审阅结果，Codex 协助实现、排查、编写文档与执行检查。真实手柄的实测与改动审阅同样是工作的一部分。此类协助不代表 OpenAI、Google 或 Microsoft 的认证或背书。

震动增益在拖动滑块时应用并保存，测试使用与游戏相同的增益。应用启动时及之后每六小时检查一次新版本，下载需在「设置」中手动完成。`StadiaLink.Setup.exe` 同时包含应用与驱动；`StadiaLink.Update.exe` 只更新应用，会保留配置文件并要求已安装兼容的驱动，否则请使用完整安装包。

## 📦 本分支交付内容

| 路径 | 说明 |
| --- | --- |
| `app/`、`driver/`、`tests/` | 改动后的完整源码与构建脚本。 |
| `dist/` | 本次编译产物（简体中文安装包、主程序、更新包、原生库、驱动 DLL/INF，均含关闭按钮修复）与 SHA-256 清单。 |
| `RELEASE_NOTES.md` | 本分支版本说明：根因、改动、产物、验证与使用。 |
| `CHANGELOG.md` | 更新后的版本历史。 |
| `docs/CHANGES.md` | 逐文件改动说明与排除清单。 |
| `patches/v1.0.2-hotfix.patch` | 相对 1.0.2 的可应用代码补丁。 |
