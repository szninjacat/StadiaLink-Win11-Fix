# StadiaLink 1.0.2 修复分支 · 版本说明

- **日期**：2026-11-09
- **基线**：`bea80f3` — *Correct release version to 1.0.2*（`origin`: `https://github.com/minysito/StadiaLink.git`）
- **程序内版本号**：仍为 `1.0.2`（本分支是修复版本；`App.cs`、`MainWindow.cs` 等文件有代码改动，但 `App.cs`、`Setup.cs`、`InstallerActions.cs`、`package.ps1` 中的版本字面量一律未变，因此现有 exe 与源码一致）
- **许可证**：GPL-3.0，见 [LICENSE](LICENSE)；第三方代码归属见 [UPSTREAM-README.md](UPSTREAM-README.md)
- **简体中文文档**：[README.zh-CN.md](README.zh-CN.md)（功能、安装、构建、使用、排障、致谢）

本分支解决三个实际问题：**Windows 11 23H2（Build 22631）上蓝牙手柄震动失效**、**界面默认西班牙语、缺少简体中文**，以及**窗口「×」直接退出、无法驻留托盘**。逐文件改动见 [docs/CHANGES.md](docs/CHANGES.md)，机器可读补丁见 [patches/v1.0.2-hotfix.patch](patches/v1.0.2-hotfix.patch)。

---

## 1. 根因与证据

蓝牙震动走一条未公开的私有 WinRT 链路：`GattClientDevice` → HID service → Report Reference → 输出报告写入。`driver/private-gatt.cpp` 原先只硬编码了从 **Windows 26200** 恢复的私有接口 IID。

| 项目 | 值 |
| --- | --- |
| 激活工厂 statics IID（22631 与 26200 相同） | `CBAA1A5F-DCEF-508B-ACC6-158F1E160C0F` |
| 设备接口 IID（26200，原驱动唯一硬编码值） | `6BBEDE1F-797F-5C17-8AC7-3F342037CB59` |
| 设备接口 IID（22631 / 23H2 实际值） | `0799A6A6-E942-5218-8A0E-7C9B996E4158` |
| 失败点 | `created->QueryInterface()` → `E_NOINTERFACE` = `0x80004002` |

实机核查结论：设备枚举与 INF 匹配正常，输入报告正常，`6BBEDE1F…` 在 22631 的全部蓝牙组件二进制中不存在，而 `HidOverGatt.dll` 里 statics IID 紧邻 `0799A6A6…`（偏移 +16 字节）。

**决定性验证**：先用一个仅替换 16 字节 IID 的二进制热补丁试装，`vtable` 槽位、结构体布局、异步约定一律未改，震动随即恢复正常（`probeStatus=0x00000000`、`writes=10`、`stops=5`、`writeHRESULT=0x00000000`）。这说明 22631 的私有接口布局与 26200 一致，**根因是单一的 IID 不匹配**。

> 注意：热补丁是临时手段，**不在本目录内**。

## 2. 修复内容

| 文件 | 改动 |
| --- | --- |
| `driver/private-gatt.cpp` | 单一 IID 改为多版本表（26200 / 22631），依次 `QueryInterface`；未知 build 回退到 `IInspectable::GetIids`，按 WinRT 参数化接口特征（`Data3 & 0xF000 == 0x5000`）挑选私有接口；全链路记录失败阶段与所用 IID；引入 `combaseapi.h` 使用 `CoTaskMemFree` |
| `driver/private-gatt.h` | 新增 `STADIA_GATT_STAGE_*` 阶段枚举（1–10 / ready=0x7F）及 `Stage`、`IidIndex` 诊断字段 |
| `driver/winstadia.c` | 用 status feature report 空闲的第 60、61 字节输出阶段与 IID 索引；报告长度与既有字段完全不变 |
| `driver/status.ps1` | 解析并打印阶段名称、所选 IID（已知 #1 / #2 / 运行时发现），Stage=5 时给出明确提示 |
| `app/assets/locales.tsv` | 新增第 5 列简体中文，287 条全部翻译，补齐 2 条动态拼接文案 → 289 行 × 5 列、无空值 |
| `app/I18n.cs` | 新增 `zh / 简体中文` 选项并设为**默认语言**（无注册表设置时回退 `zh`），适配 5 列与列映射，校验改为 5 列 |
| `app/MainWindow.cs` | 删除配置文件对话框的西语书名号/问号改为经翻译的「」吗？；标题栏 **×** 改绑 `MinimizeToTray()`，新增 `Closing` 拦截系统关闭（Alt+F4）与 `ForceClose()`，并扩充 `VerifyDraftBehavior` 断言 |
| `app/Setup.cs` | 语言测试文案同步为 5 种语言 |
| `app/App.cs` | 13 处自动化测试的自退出由 `window.Close()` 改为 `window.ForceClose()`，避免被新的关闭拦截逻辑挂住 |
| `driver/build.ps1`、`app/build-native.ps1`、`tests/run.ps1` | 增加 `STADIALINK_VC_ENV_READY=1` 跳过 `vcvars64.bat` 的开关，供无法运行 `reg.exe` 的 CI/沙箱使用 |
| `app/package.ps1` | 增加 `STADIALINK_SKIP_STAGE_CLEANUP=1` 跳过暂存目录清理的开关 |
| `tests/build-zh-locale.py` | 新增：幂等生成 5 列翻译表 |
| `tests/probe-private-gatt.py` | 新增：换 Windows 版本后枚举私有接口 IID 的只读诊断工具 |

**未改动**：USB 震动、蓝牙输入与 XInput 翻译、配置/模型、电池、签名与安装流程、INF 设备匹配 ID。所有构建开关默认关闭，**原构建行为不变**。托盘图标、电量显示与开机自启本身也未改动，只调整了「关闭 / 恢复」的窗口行为。

### 关闭按钮行为（第三项修复）

官方 1.0.2 把标题栏 `×` 直接绑定到 `Close()`，因此点关闭就退出，只有最小化按钮才会驻留托盘；本分支将 `×` 与系统关闭命令（Alt+F4）统一改为隐藏窗口并从任务栏移除，进程继续在托盘运行。托盘菜单的**退出**走 `ForceClose()` 才真正结束进程；自动化测试也改用 `ForceClose()`，否则测试窗口会被拦截逻辑挂住。`app/App.cs` 中全部 13 处测试自退出点已相应调整。

WPF 细节：`Closing` 事件里仅设置 `WindowState=Minimized` 会被关闭流程抑制，因此实现上直接 `Hide()` 并置 `ShowInTaskbar=false`；托盘不可见时不拦截，避免无法退出。

## 3. 编译产物（`dist/`）

| 文件 | 大小 (B) | SHA-256（前 16 位） | 说明 |
| --- | --- | --- | --- |
| `StadiaLink.Setup.exe` | 583,680 | `a0b148d37853a1ca` | 简体中文完整安装包（内嵌应用 + 驱动 + 原生库 + 源码，含关闭按钮修复） |
| `StadiaLink.exe` | 251,392 | `e29be9999f3e425c1` | WPF 主程序 x64 |
| `StadiaLink.Update.exe` | 505,344 | `1f3ee5d56e3f5e60` | 仅应用更新包 |
| `StadiaDevice.dll` | 165,376 | `4d03b4637f013cce` | 原生电池读取库 |
| `driver/winstadia.dll` | 145,920 | `09fde6d9d35c2f4b` | UMDF 驱动（含双 IID 与阶段诊断） |
| `driver/winstadia.inf` | 3,969 | `252a3981089d8577` | `DriverVer=11/09/2026,22.52.7.585` |

另含 2 个 `.exe.config` 及 `SHA256.txt` / `SHA256SUMS.txt`。完整哈希以 `dist/SHA256SUMS.txt` 为准，可用 `Get-FileHash -Algorithm SHA256` 复核。已从安装包内嵌的 `payload.zip` 中解出核对：`driver/pkg/winstadia.dll`、`driver/pkg/winstadia.inf` 与 `StadiaLink.exe` 均和 `dist/` 中的副本逐字节一致。

## 4. 构建环境

- Visual Studio 2022 Build Tools 17.14.44 → MSVC **14.44.35207**
- Windows SDK **10.0.26100.0**、WDK NuGet **10.0.26100.6584**（UMDF 2.31）
- .NET Framework **4.8.1**（系统自带 `csc.exe` + WPF 引用程序集）
- 驱动编译仅有 3 条 WDK 头文件警告，无错误

重新构建（默认路径，不需要任何环境变量）：

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned
.\app\package.ps1 -Driver
.\tests\run.ps1
```

## 5. 验证结果

- `driver/out/pkg/winstadia.dll` 同时含 26200 与 22631 两个设备 IID；Setup.exe 内嵌副本逐字节相同（清理 WDK 后重新编译，代码与 IID 未变，仅 PE 时间戳 / `DriverVer` 更新）。
- `tests/run.ps1`：配置翻译 **4,864,527 次检查 PASS**、应用模型自测 PASS。
- 应用 `--self-test` PASS（5 列翻译表完整、动态翻译正常）；安装器 `/languages-test` PASS（5 种语言全部渲染）。
- **默认语言截图与显式设为 `zh` 的截图 SHA-256 完全相同**，界面文字全部简体中文，无西语残留（仅保留 StadiaLink / Windows / USB / x64 / GPLv3 等专有名词）。
- 三个 exe 均为有效 x64 .NET 程序集，`StadiaDevice.dll` 为原生 x64 PE；哈希清单逐项一致。
- 实机震动：`driver/status.ps1` 报告 `probeStatus=0x00000000`、`writeHRESULT=0x00000000`、`writes` 持续增长。
- 关闭行为自测 PASS（`--draft-test`）：最小化、**×** 按钮、系统关闭三种入口都会驻留托盘，托盘恢复均正常。
- 实时震动滑块测试在退出占用单实例锁的旧实例后重跑 PASS；全套应用与模型测试在最终 23:04 重建版上全部通过。

## 6. 目录结构

```
.gitignore  .gitattributes  LICENSE
README.md（默认·简体中文）  README.zh-CN.md  README.es.md  README.en.md  README.fr.md  README.de.md
UPSTREAM-README.md  CHANGELOG.md  RELEASE_NOTES.md
app/        应用源码、资源、构建脚本
driver/     UMDF 驱动源码、INF、安装与状态脚本
tests/      测试与两个诊断/生成工具
dist/       本次编译产物 + SHA-256 清单
docs/CHANGES.md              逐文件改动说明与排除清单
patches/v1.0.2-hotfix.patch  相对 1.0.2 的可 apply 代码补丁（15 个文本文件）
```

`dist/` 被有意纳入版本控制（见 `.gitignore` 注释）；构建中间产物、智能体工作目录与本机专用脚本均未包含。

## 7. 安装 / 升级 / 回滚

1. 先退出托盘中正在运行的旧 StadiaLink 实例（单实例锁会阻止安装）。本修复版点 **×** 只会最小化到托盘，请从托盘菜单选择**退出**。
2. 双击 `dist/StadiaLink.Setup.exe`，驱动安装会请求管理员权限。
3. 若手柄未出现，关开一次蓝牙或重新插拔；此前 `pnputil` 返回 **3010** 即表示需要重连设备。
4. 运行 `driver/status.ps1`：成功标志为 `probeStatus=0x00000000`、`report=5 type=2 value=0x0044`、`writes` 递增；编译版还会显示 `Private GATT stage` 与所选 IID。
5. 回滚：`StadiaLink.Setup.exe /uninstall`，或管理员运行 `driver/install.ps1 -Uninstall`。

## 8. 已知限制

1. **跨任意 Windows 版本不保证**：蓝牙震动依赖未公开私有 ABI。`statics IID` 仅在 22631 / 26200 验证一致；运行时兜底只负责发现设备接口 IID，**不适配 vtable 方法顺序**。若未来 build 改了布局，代码仍按硬编码槽位调用，可能失败甚至影响 UMDF host。换系统后请先用 `tests/probe-private-gatt.py` 与 `driver/status.ps1` 的 Stage 判断：`5` 是 IID 问题，`6+` 是布局问题。
2. 私有类 `GattClientDevice` 本身可能被移除或重构。
3. 平台依赖：仅 **x64**、UMDF 2.31（Win10 1809+ / Win11）、.NET Framework 4.8.1、INF 固定匹配 `VID_18D1&PID_9400`。
4. 驱动为**自签名**，未做微软签名；每台机器需重建证书信任，SmartScreen / Secure Boot 策略可能拦截。
