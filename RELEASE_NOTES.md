# StadiaLink 2.0.0 独立修复分支 · 版本说明

- **日期**：2026-11-09
- **基线**：`bea80f3` — *Correct release version to 1.0.2*（`origin`: `https://github.com/minysito/StadiaLink.git`）
- **程序内版本号**：`2.0.0`（2026-11-11 起启用独立版本线；`App.cs`、`Setup.cs`、`app.manifest`、`InstallerActions.cs`、`package.ps1` 与界面显示均已同步，Release tag 为 `v2.0.0`，与上游 1.x 不再共用版本号）
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
| `StadiaLink.Setup.exe` | 595,968 | `8349cbed7a9beebb` | 简体中文完整安装包（内嵌应用 + 驱动 + 原生库 + 源码；2.0.0 独立版本线，含关闭按钮修复与自动适配） |
| `StadiaLink.exe` | 253,440 | `582b8d8c39d92543` | WPF 主程序 x64（版本资源 `2.0.0.0`） |
| `StadiaLink.Update.exe` | 514,048 | `e2f133f03e1c410e` | 仅应用更新包 |
| `StadiaDevice.dll` | 165,376 | `b3e6816ddccfe7ae` | 原生电池读取库 |
| `driver/winstadia.dll` | 152,064 | `06b70008929b2cb7` | UMDF 驱动（含双 IID、阶段诊断与分层自动适配） |
| `driver/winstadia.inf` | 3,970 | `fc0a1d07216dbd48` | `DriverVer=11/10/2026,19.30.26.699` |

2026-11-11 为独立版本线重新编译了三个 exe 与 `StadiaDevice.dll`（驱动 DLL / INF 未改动，仍为 11-10 版）；上表与 `dist/SHA256SUMS.txt` 均为本次重编后的实际值。

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

- `driver/out/pkg/winstadia.dll` 同时含 26200 与 22631 两个设备 IID，并含分层自动适配、SEH 隔离与注册表熔断逻辑；Setup.exe 内嵌副本逐字节相同（2026-11-10 第二次重新编译）。
- `tests/run.ps1`：配置翻译 **4,864,527 次检查 PASS**、应用模型自测 PASS。
- 应用 `--self-test` PASS（5 列翻译表完整、动态翻译正常）；安装器 `/languages-test` PASS（5 种语言全部渲染）。
- **默认语言截图与显式设为 `zh` 的截图 SHA-256 完全相同**，界面文字全部简体中文，无西语残留（仅保留 StadiaLink / Windows / USB / x64 / GPLv3 等专有名词）。
- 三个 exe 均为有效 x64 .NET 程序集，`StadiaDevice.dll` 为原生 x64 PE；哈希清单逐项一致。
- 实机震动：`driver/status.ps1` 报告 `probeStatus=0x00000000`、`writeHRESULT=0x00000000`、`writes` 持续增长。
- 关闭行为自测 PASS（`--draft-test`）：最小化、**×** 按钮、系统关闭三种入口都会驻留托盘，托盘恢复均正常。
- 实时震动滑块测试在退出占用单实例锁的旧实例后重跑 PASS；全套应用与模型测试在最终 2026-11-10 18:46 重建版上全部通过（配置翻译 4,864,527 次检查、应用模型自测、实时震动滑块测试均 PASS）。
- 2026-11-11 独立版本线重编译：三个 exe 与 `StadiaDevice.dll` 的版本资源为 `2.0.0.0`，`UpdateChecker.Verify()` 覆盖上游地址、1.x / 旧 hotfix / 预览 tag 与 draft 的拒绝断言；上表哈希与 `dist/SHA256SUMS.txt` 逐项复核一致。

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
docs/COMPATIBILITY.md        兼容性矩阵、分层自动适配与熔断机制、新增 IID 实机验证流程
patches/v1.0.2-hotfix.patch  相对 1.0.2 的可 apply 代码补丁（21 个文本文件，约 219 KB）
```

`dist/` 被有意纳入版本控制（见 `.gitignore` 注释）；构建中间产物、智能体工作目录与本机专用脚本均未包含。

## 7. 安装 / 升级 / 回滚

1. 先退出托盘中正在运行的旧 StadiaLink 实例（单实例锁会阻止安装）。本修复版点 **×** 只会最小化到托盘，请从托盘菜单选择**退出**。
2. 双击 `dist/StadiaLink.Setup.exe`，驱动安装会请求管理员权限。
3. 若手柄未出现，关开一次蓝牙或重新插拔；此前 `pnputil` 返回 **3010** 即表示需要重连设备。
4. 运行 `driver/status.ps1`：成功标志为 `probeStatus=0x00000000`、`report=5 type=2 value=0x0044`、`writes` 递增；编译版还会显示 `Private GATT stage` 与所选 IID。
5. 回滚：`StadiaLink.Setup.exe /uninstall`，或管理员运行 `driver/install.ps1 -Uninstall`。
6. **从 1.0.2 修复版升级**：旧版 exe 内写死了上游更新地址，无法通过远程配置修正，请直接运行本版 `StadiaLink.Setup.exe` 覆盖安装。安装后界面显示 `2.0.0 · Win11-Fix`，此后只有本仓库发布 v2.x 正式 Release 才会提示更新。

## 8. 已知限制

1. **跨任意 Windows 版本不保证**：蓝牙震动依赖未公开私有 ABI。`statics IID` 仅在 22631 / 26200 验证一致；未知 build 会按第 10 节的五层验证**自动尝试适配**设备接口 IID 并缓存成功结果，但 `GetIids` **不适配 vtable 方法顺序**，验证失败即阶段 `12` 停用，私有类本身也可能被移除。**Windows 10 只有框架层面的可安装性**，私有 GATT 震动接口未实机验证、不承诺成功；矩阵见 [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md)。换系统后请先用 `tests/probe-private-gatt.py` 与 `driver/status.ps1` 的 Stage 判断：`5` 是 IID 问题，`6+` 是布局问题，`11`/`12` 是自动适配不可用或未通过。
2. 私有类 `GattClientDevice` 本身可能被移除或重构。
3. 平台依赖：仅 **x64**、UMDF 2.31（Win10 1809+ / Win11）、.NET Framework 4.8.1、INF 固定匹配 `VID_18D1&PID_9400`。
4. 驱动为**自签名**，未做微软签名；每台机器需重建证书信任，SmartScreen / Secure Boot 策略可能拦截。

## 9. 代码审查后的加固（2026-11-10）

按第三方代码审查结论修订。**22631 / 26200 两个已验证构建的行为完全不变**，改动只涉及未知 build 的回退路径、错误分类与诊断。

> 说明：本节记录当天第一次加固（失败关闭）。同日第二次更新见第 10 节，已改为默认开启的分层自动适配；两次更新中 22631 / 26200 的行为都未改变。

| 审查项 | 处理 |
| --- | --- |
| 未知 IID 仅凭 `Data3 & 0xF000 == 0x5000` 猜设备接口并直接调用 | 改为失败关闭：只枚举并计数，阶段 `11`、返回 `ERROR_NOT_SUPPORTED`；仅当编译开关 `STADIALINK_ALLOW_UNVERIFIED_IID=1` 且候选恰好 1 个时才自动选择（测试构建专用，默认关闭） |
| `QueryInterface` 失败原因被后续 IID 掩盖 | 只有 `E_NOINTERFACE` 继续尝试，其余 HRESULT 立即返回 |
| Stage 4 把整个异步等待混在一起 | `Await()` 细分 `IAsyncInfo` QI / `get_Status` / `GetResults` / 异步错误 / 超时，写入第 63 字节；同时修正原先 `get_Status` 失败被误报为超时 |
| 诊断数据可能误导用户 | 新增第 62 字节候选数、阶段 `11` 的可读提示；`status.ps1` 输出异步失败点；`probe-private-gatt.py` 的 GUID 改为标准 8-4-4-4-12，标准 IID 与共享 statics IID 不再被误标成「未验证候选」，并说明设备 IID 在 factory 上返回 `E_NOINTERFACE` 属预期 |
| Windows 10 兼容性边界 | 新增 `docs/COMPATIBILITY.md`：正式兼容性矩阵、失败关闭说明、新增 IID 的实机 ABI 验证流程；6 语 README 明确 Windows 10 仅框架可安装、蓝牙震动未验证 |
| 构建命令目录名 | 6 语 README 的 `cd StadiaLink` 修正为 `cd StadiaLink-Win11-Fix` |
| 源码注释过度承诺 | `Windows 26200+` 改为 `Windows 26200 (verified)` |

**仍未解决 / 不在本次范围**：没有 Windows 10 实机 IID 数据，也没有 22H2 / 24H2 的验证记录；这些版本需按 `docs/COMPATIBILITY.md` 第 4 节走完流程才能列入 `DeviceIids[]`。后续维护建议把驱动 ABI、界面本地化与其他功能修复拆成独立提交。本次加固单独成一次提交，不改写已有历史。

本次修订后重新编译了驱动与安装包，`dist/`、SHA-256 清单与 Release 资产同步更新；USB 震动、输入与 XInput 翻译链路未改动。

## 10. 同日第二次更新：未知系统自动适配（2026-11-10）

用户反馈「未知 build 直接停用震动」实用性不足，于是把第 9 节的失败关闭策略改为**默认开启的分层自动适配**。核心原则：不再凭一个 IID 特征就调用 vtable，而是让候选先通过可验证的关卡。

| 改动 | 内容 |
| --- | --- |
| 解析顺序 | 已验证 IID → 缓存 IID → 运行时候选；22631 / 26200 命中已验证 IID 时代码路径与官方 1.0.2 一致，未引入 SEH、注册表或策略判断 |
| 新增 `driver/iid-policy.h` | 纯策略头：参数化接口 / GUID 变体位筛选、排除标准与已验证 IID、黑名单去重与滚动、诊断位编码；由新增的 `tests/iid-policy-tests.cpp`（26 项断言）覆盖，并接入 `tests/run.ps1` |
| vtable 静态校验 | 槽 6–14 全部指向已提交的可执行内存、至少 5 个不同实现；按与激活工厂同映像的方法数排序，最多尝试 3 个候选 |
| 语义验证 | 用完整 `Open()` 链路（HID 服务 UUID、恰好 1 个服务、Report Reference 5/2）作为验证器，猜错不会进入写入阶段 |
| SEH 隔离 | `GuardedTryOpen()` 把访问违例转成 HRESULT，避免击穿 WUDFHost.exe |
| 熔断与缓存 | `HKCU\SOFTWARE\StadiaLink\PrivateGatt` 下 `Verified` / `Blacklist`（最多 4 个）/ `Attempt`；宿主崩溃导致 `Attempt` 残留时，下次启动自动拉黑该 IID，不会崩溃循环；注册表不可写时静默降级 |
| 开关 | `STADIALINK_AUTO_IID=0` 构建严格失败关闭版（阶段 `11`）；默认不设即为自动适配 |
| 诊断 | 阶段 `12` = 候选全部未通过；`IidIndex` `0x40` = 缓存、`0x80+i` = 本次验证通过；第 62 字节低 6 位为候选数、`0x40` 缓存标志、`0x80` 崩溃拉黑标志 |
| 文档 | `docs/COMPATIBILITY.md` 重写为矩阵 + 自适应策略 + 重置方法；6 语 README 与探针脚本同步；`tests/iid-policy-tests.cpp` 新增 |

**验证**：精简 WDK 下驱动与三个 exe 重编译成功（仅 3 类既有 WDK 头警告）；`STADIALINK_AUTO_IID=1/0` 两种策略均零警告编译通过；策略单测 26 项全过；配置翻译 4,864,527 次检查与应用模型自测 PASS。本机 22631 命中硬编码 IID，自动适配路径不会触发，**该路径暂无实机覆盖**；实时震动滑块测试因用户托盘实例占用单实例锁未重跑。

**限制**：自动适配解决 IID 变化，不解决 vtable 方法顺序变化；Windows 10 / 22H2 / 24H2 仍无实机验证数据，不承诺成功。完整机制与实机验证步骤见 [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md)。

## 11. 独立版本线与更新通道（2026-11-11）

**问题**：原版 `app/UpdateChecker.cs` 把更新检查地址写死为 `minysito/StadiaLink`，`Parse()` 还强制要求 `html_url` 属于上游仓库。上游 2026-11-10 发布 1.0.3 后，本分支安装的用户会被提示升级到上游版本。

| 位置 | 处理 |
| --- | --- |
| `app/UpdateChecker.cs` | `ReleasesUrl` 与 API 地址改为 `szninjacat/StadiaLink-Win11-Fix`；`Parse()` 同时校验页面 URL 与 API `url` 的仓库归属、HTTPS、非 draft/预发布，且 tag 必须是 `v` 前缀的纯版本号、**Major ≥ 2**。上游 1.x、旧 `v1.0.2-hotfix`、带后缀的预览 tag 一律拒绝 |
| 版本字面量 | `App.cs` / `Setup.cs` 的 `AssemblyVersion` / `AssemblyFileVersion`、`app.manifest`、`USO.md`、`package.ps1` 源码包文件名统一为 `2.0.0` |
| `app/InstallerActions.cs` | 卸载注册表 `DisplayVersion` 改为从程序集版本动态读取，`DisplayName` 改为 `StadiaLink Win11 Fix`，`Publisher` 指向本仓库 |
| `app/MainWindow.cs` | 侧栏与「关于」版本显示动态读取并标注 `Win11-Fix`；更新通知标记改用 `Win11Fix.UpdateNotified`，与上游/旧版残留隔离 |
| 文档与下载 | 6 语 README 版本徽标改为 `2.0.0 (Win11-Fix)`，下载按钮改指 `/releases/latest` |

**效果**：程序内版本 `2.0.0.0` 与 Release tag `v2.0.0` 一致；只有本仓库发布 `v2.x` 及以上正式 Release 才会触发更新提示。`UpdateChecker.Verify()` 增加了针对上游地址、1.x tag、旧 hotfix tag、预览 tag、伪造 API 地址与 draft/prerelease 的拒绝断言。

**注意**：已安装 1.0.2 修复版的用户必须重装本版 Setup.exe；旧 exe 内的地址无法通过远程配置改变。旧 Release `v1.0.2-hotfix` 保留为历史记录，不再作为默认下载入口。
