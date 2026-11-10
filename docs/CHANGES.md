# 本次改动清单（相对 1.0.2）

- **基线提交**：`bea80f3` — *Correct release version to 1.0.2*
- **比较范围**：`app/`、`driver/`、`tests/`
- **机器可读补丁**：[../patches/v1.0.2-hotfix.patch](../patches/v1.0.2-hotfix.patch)，17 个文本文件、约 207 KB，由真实 `git diff --cached --binary` 导出（含新增文件），无二进制块
- **版本说明**：[../RELEASE_NOTES.md](../RELEASE_NOTES.md)

本目录中的 `app/`、`driver/`、`tests/` **已经是改动后的源码**，无需再应用补丁。补丁用于代码审查，或把改动移植到一份干净的 1.0.2 checkout：

```bash
git apply --check patches/v1.0.2-hotfix.patch
git apply patches/v1.0.2-hotfix.patch
```

## 一、修改的既有文件（13 个）

| 文件 | 改动摘要 |
| --- | --- |
| `driver/private-gatt.cpp` | 设备接口 IID 由单值改为 26200 / 22631 双值表并依次 `QueryInterface`（只有 `E_NOINTERFACE` 才继续）；`DiscoverDeviceIid()` 对未知 build 只枚举、计数并失败关闭，不自动调用未验证 vtable；`Await()` 细分异步子阶段；`GetDevice`/`Open` 全链路写入 Stage；新增 `combaseapi.h` 以释放 `CoTaskMemFree` |
| `driver/private-gatt.h` | 新增 `STADIA_GATT_STAGE_*`（1 起始 / 2 激活工厂 / 3 CreateAsync / 4 等待异步 / 5 设备 QI / 6–10 服务与特征发现 / 11 无已验证 IID / 0x7F ready）、`STADIA_GATT_ASYNC_*` 子阶段枚举与 `Stage`、`IidIndex`、`CandidateCount`、`AsyncStage` 字段 |
| `driver/winstadia.c` | feature report 第 60–63 字节输出 Stage、IidIndex、CandidateCount 与 AsyncStage；报告长度、既有字段字节偏移均未变 |
| `driver/status.ps1` | 读取并打印阶段名称、IID 来源、未验证候选数与异步失败点；Stage=5 提示接口不匹配，Stage=11 提示失败关闭并指向兼容性矩阵 |
| `app/assets/locales.tsv` | 由 4 列（es/en/fr/de）扩为 5 列（新增 zh），287 条全部翻译，补 2 条动态拼接文案 → 289 行 |
| `app/I18n.cs` | 语言选项新增 `zh / 简体中文` 且置于首位；`Saved` 默认与回退值由 `es` 改为 `zh`；目录解析 4→5 列；列映射显式处理 `de` 并把 `zh` 映射到第 5 列；`VerifyCatalog` 校验 5 列 |
| `app/MainWindow.cs` | 删除配置文件确认框的尾部 `"»?"` 改为 `I18n.T("»?")`，使西语书名号/问号可被正确翻译；标题栏 **×** 改绑 `MinimizeToTray()`，新增 `Closing` 拦截系统关闭、`ForceClose()` 与 `closeButton` 字段，`VerifyDraftBehavior` 增加关闭按钮 / 系统关闭 / 托盘恢复断言 |
| `app/Setup.cs` | `/languages-test` 结果文案由 "four languages" 改为 "five languages" |
| `app/App.cs` | 13 处自动化测试的自退出由 `window.Close()` 改为 `window.ForceClose()`，避免测试窗口被 `Closing` 拦截后无法退出 |
| `driver/build.ps1` | `vcvars64.bat` 导入包在 `if ($env:STADIALINK_VC_ENV_READY -ne '1')` 内，允许调用方自备 `INCLUDE`/`LIB`/`PATH`；新增测试构建开关 `STADIALINK_ALLOW_UNVERIFIED_IID`（默认关闭） |
| `app/build-native.ps1` | 同上 |
| `tests/run.ps1` | 同上 |
| `app/package.ps1` | 两处暂存目录清理受 `STADIALINK_SKIP_STAGE_CLEANUP=1` 控制；默认行为不变 |

## 二、新增文件（4 个，已包含在补丁中）

| 文件 | 用途 |
| --- | --- |
| `tests/build-zh-locale.py` | 幂等生成 5 列翻译表（先移除已存在的 zh 列再重建），是 `locales.tsv` 的可复现来源 |
| `tests/probe-private-gatt.py` | 只读探测本机私有 WinRT 激活工厂与接口 IID，输出标准 8-4-4-4-12 GUID 并区分已验证 IID / 共享 statics / 未验证候选；不会连接手柄、不会写 GATT 特征 |
| `driver/iid-policy.h` | 纯策略头：候选 IID 筛选、已知 IID 排除、黑名单去重滚动与诊断位编码；驱动与单元测试共用 |
| `tests/iid-policy-tests.cpp` | 26 项策略单测（筛选、变体位、已知排除、黑名单、诊断编码），已接入 `tests/run.ps1` |

此外 `app/StadiaLink.ico` 是 `app/create-icon.ps1` 生成、被 `app/build.ps1` 引用的构建资源，随本仓库提供（新 `.gitignore` 不再忽略 `*.ico`）。它是二进制资源，**不放入文本补丁**。

## 三、仓库元数据与文档（不属于代码补丁）

- 新增 `.gitignore`（含"dist/ 有意纳入版本控制"说明）、`.gitattributes`
- 新增 `README.zh-CN.md`：完整的简体中文文档，章节与其他 4 语 README 一致，并增加本分支特有的「蓝牙震动兼容性」「本分支交付内容」「构建开关」等说明；「使用」一节补充了 × / Alt+F4 驻留托盘与托盘**退出**的行为
- `README.md` 顶部新增简体中文分支入口（指向中文 README 与版本说明）；`README.md` / `.en` / `.fr` / `.de` 的语言导航行均加入「🇨🇳 简体中文」链接；`LICENSE`、`UPSTREAM-README.md` 原样保留
- 默认 `README.md` 改为简体中文（与 `README.zh-CN.md` 一致）；原西班牙语版另存为 `README.es.md`；6 份 README 的语言导航同步更新；仓库 About 简介改为简体中文
- `CHANGELOG.md` 新增 `Unreleased — 1.0.2 hotfix (2026-11-09)` 条目（含关闭按钮修复），历史条目不变；`RELEASE_NOTES.md` 增加「关闭按钮行为」小节并更新产物哈希
- 新增 `RELEASE_NOTES.md`（中文版本说明）、本文件、`patches/`、`dist/`
- 新增 `docs/COMPATIBILITY.md`：兼容性矩阵（含 Windows 10 未验证说明）、分层自动适配与熔断机制、诊断字节表、缓存/黑名单重置与新增 IID 的实机 ABI 验证流程；6 语 README 的兼容性段落与 `cd StadiaLink-Win11-Fix` 构建命令同步更新
- `CHANGELOG.md` 追加「代码审查后加固」条目；`RELEASE_NOTES.md` 追加第 9 节并修订「已知限制」第 1 条

## 四、未改动的部分

USB 震动路径、蓝牙输入与 XInput 翻译、配置模型与持久化、电池读取、托盘图标与电量显示、开机自启、更新检查、驱动签名与安装流程、INF 的设备匹配 ID、报告长度与既有字段布局（新增诊断仅使用原本空闲的第 62、63 字节）。仅「关闭 / 恢复」的窗口行为按第一节调整。所有新增构建开关默认关闭，原有 `.\app\package.ps1 -Driver` 用法不变。

## 五、明确排除的内容

以下都不属于可发布内容，**未**放入本目录：

| 排除项 | 原因 |
| --- | --- |
| `.build/` | 构建暂存：WDK NuGet 包与解压目录（约 310 MB）、编译中间文件、语言截图 `zh-check/`、上一轮驱动包备份 `prebuild-out-*`、`vc-env.ps1`、本次生成脚本 |
| `.workbuddy/` | 智能体工作记忆与日志，与项目无关 |
| `.git/` | 父仓库的对象库与索引；本目录是可直接 `git init` / 建立分支的干净工作树 |
| `tests/vc-env.ps1` | 本机命令安全策略（禁止 `reg.exe`）的专用环境绕过脚本，只对这台机器有效 |
| `tests/patch-driver-iid.py` | 一次性二进制热补丁生成器；正式编译版已不需要，误用会覆盖 IID 并破坏其他 Windows 版本 |
| `tests/__pycache__/` | Python 字节码缓存 |
| `release/StadiaLink-1.0.2-source.zip` | 安装包内部已内嵌 `Source.zip`，且文件名沿用旧版本号，重复且易混淆 |
| `release/SHA256.txt`、`release/SHA256SUMS.txt` | 由 `dist/` 下重新生成的两份清单替代（只覆盖实际分发的文件） |
| `driver/out/` 中间产物、`*.obj`、`*.pdb`、`*.log`、截图、DriverStore 备份 | 构建中间物与本机回滚材料 |

`dist/driver/` 中的 `winstadia.dll` / `winstadia.inf` 是**唯一**保留的 `driver/out/pkg` 产物（2026-11-10 第二次重新编译，含审查加固与分层自动适配，双 IID 未变；`DriverVer=11/10/2026,19.30.26.699`）。

## 六、代码审查后的加固（2026-11-10）

| 文件 | 追加的改动 |
| --- | --- |
| `driver/private-gatt.cpp` | `STADIALINK_ALLOW_UNVERIFIED_IID`（默认 0）；`DiscoverDeviceIid()` 改为枚举全部候选 + 计数 + 失败关闭；已知 IID 循环区分 `E_NOINTERFACE`；`Await()` 拆出 5 个异步子阶段并修正 `get_Status` 失败被误判为超时；注释改为 `Windows 26200 (verified)` |
| `driver/private-gatt.h` | Stage 11、`STADIA_GATT_ASYNC_*`、`CandidateCount`、`AsyncStage` |
| `driver/winstadia.c` | 第 62、63 字节输出候选数与异步失败点 |
| `driver/status.ps1` | 阶段 11 / 异步子阶段 / 候选数的可读输出与排障指引 |
| `driver/build.ps1` | 测试构建专用编译开关，默认关闭 |
| `tests/probe-private-gatt.py` | 标准 GUID 输出、IID 分类标注、增加 22631 已验证 IID 探测项、末尾说明不误导 |
| 6 语 README + 模板 | 重写中文「蓝牙震动兼容性」章节以匹配失败关闭行为；统一 6 语兼容性边界；修正构建目录名 |
| `docs/COMPATIBILITY.md`（新增，文档类，不进补丁） | 兼容性矩阵、失败关闭说明、诊断字节表、新增 IID 实机验证流程、提交拆分约定 |

**未验证版本**：Windows 10 22H2 / 11 22H2 / 11 24H2 均无实机数据，不进入 `DeviceIids[]`。本次没有 Windows 10 的 IID 可补，下一步应先做只读探测，而不是猜一个 IID 写进白名单。

**行为影响**：22631 / 26200 上仍按原来两个已验证 IID 打开，震动路径完全不变；只有未知 build 从「可能猜错并调用」变为「明确停用并报告」。

## 七、同日第二次更新：分层自动适配（2026-11-10）

| 文件 | 改动 |
| --- | --- |
| `driver/private-gatt.cpp` | 拆出 `CreateDeviceObject` / `TryOpenWithDevice` / `GuardedTryOpen` / `AutoAdapt`；已验证 IID 路径保持原样，未知 build 才进入候选筛选 → vtable 静态校验 → 完整链路验证 → SEH 隔离 → 注册表缓存 / 黑名单 / 崩溃熔断 |
| `driver/iid-policy.h`（新增） | 纯策略头：候选筛选、已知 IID 排除、黑名单去重滚动、诊断位编码，供驱动与单元测试共用 |
| `driver/private-gatt.h` | 新增阶段 `12`；`IidIndex` 增加 `0x40` 缓存 / `0x80+i` 本次验证编码；`CandidateCount` 增加标志位说明 |
| `driver/build.ps1` | 默认开启自动适配，`STADIALINK_AUTO_IID=0` 构建严格版；链接新增 `advapi32.lib` |
| `driver/status.ps1` | 阶段 `11`/`12` 文案、`IidIndex` 解码、第 62 字节标志位与熔断提示 |
| `tests/iid-policy-tests.cpp`（新增） | 26 项策略单测；`tests/run.ps1` 接入编译与运行 |
| `tests/probe-private-gatt.py` | 说明改为「候选会被自动验证并缓存」，与驱动行为一致 |
| 6 语 README + 模板 | 中文「蓝牙震动兼容性」章节改为自动适配与容错；各语言兼容性段落补充自动适配边界 |
| `docs/COMPATIBILITY.md` | 重写：矩阵、五层验证、降级与诚实边界、诊断字节、手动补 IID、缓存/黑名单重置 |
| `RELEASE_NOTES.md` / `CHANGELOG.md` / 本节 | 追加第 10 节 / 新条目 / 本节 |

补丁现为 **17 个文本文件、约 207 KB**（13 个修改 + 4 个新增）。`dist/` 已全部重编译，哈希清单同步更新。
