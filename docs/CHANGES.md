# 本次改动清单（相对 1.0.2）

- **基线提交**：`bea80f3` — *Correct release version to 1.0.2*
- **比较范围**：`app/`、`driver/`、`tests/`
- **机器可读补丁**：[../patches/v1.0.2-hotfix.patch](../patches/v1.0.2-hotfix.patch)，15 个文本文件、约 172 KB，由真实 `git diff --cached --binary` 导出（含新增文件），无二进制块
- **版本说明**：[../RELEASE_NOTES.md](../RELEASE_NOTES.md)

本目录中的 `app/`、`driver/`、`tests/` **已经是改动后的源码**，无需再应用补丁。补丁用于代码审查，或把改动移植到一份干净的 1.0.2 checkout：

```bash
git apply --check patches/v1.0.2-hotfix.patch
git apply patches/v1.0.2-hotfix.patch
```

## 一、修改的既有文件（13 个）

| 文件 | 改动摘要 |
| --- | --- |
| `driver/private-gatt.cpp` | 设备接口 IID 由单值改为 26200 / 22631 双值表并依次 `QueryInterface`；新增 `DiscoverDeviceIid()`，未知 build 用 `IInspectable::GetIids`（vtable 槽 3）按参数化接口特征挑选；`GetDevice`/`Open` 全链路写入 Stage；成功时记录 `IidIndex`；新增 `combaseapi.h` 以释放 `CoTaskMemFree` |
| `driver/private-gatt.h` | 新增 `STADIA_GATT_STAGE_*`（1 起始 / 2 激活工厂 / 3 CreateAsync / 4 等待异步 / 5 设备 QI / 6–10 服务与特征发现 / 0x7F ready）与 `Stage`、`IidIndex` 字段 |
| `driver/winstadia.c` | feature report 第 60、61 字节输出 Stage 与 IidIndex；报告长度、既有字段字节偏移均未变 |
| `driver/status.ps1` | 读取并打印阶段名称与 IID 来源；Stage=5 且失败时提示私有接口与当前 Windows build 不匹配 |
| `app/assets/locales.tsv` | 由 4 列（es/en/fr/de）扩为 5 列（新增 zh），287 条全部翻译，补 2 条动态拼接文案 → 289 行 |
| `app/I18n.cs` | 语言选项新增 `zh / 简体中文` 且置于首位；`Saved` 默认与回退值由 `es` 改为 `zh`；目录解析 4→5 列；列映射显式处理 `de` 并把 `zh` 映射到第 5 列；`VerifyCatalog` 校验 5 列 |
| `app/MainWindow.cs` | 删除配置文件确认框的尾部 `"»?"` 改为 `I18n.T("»?")`，使西语书名号/问号可被正确翻译；标题栏 **×** 改绑 `MinimizeToTray()`，新增 `Closing` 拦截系统关闭、`ForceClose()` 与 `closeButton` 字段，`VerifyDraftBehavior` 增加关闭按钮 / 系统关闭 / 托盘恢复断言 |
| `app/Setup.cs` | `/languages-test` 结果文案由 "four languages" 改为 "five languages" |
| `app/App.cs` | 13 处自动化测试的自退出由 `window.Close()` 改为 `window.ForceClose()`，避免测试窗口被 `Closing` 拦截后无法退出 |
| `driver/build.ps1` | `vcvars64.bat` 导入包在 `if ($env:STADIALINK_VC_ENV_READY -ne '1')` 内，允许调用方自备 `INCLUDE`/`LIB`/`PATH`；默认行为不变 |
| `app/build-native.ps1` | 同上 |
| `tests/run.ps1` | 同上 |
| `app/package.ps1` | 两处暂存目录清理受 `STADIALINK_SKIP_STAGE_CLEANUP=1` 控制；默认行为不变 |

## 二、新增文件（2 个，已包含在补丁中）

| 文件 | 用途 |
| --- | --- |
| `tests/build-zh-locale.py` | 幂等生成 5 列翻译表（先移除已存在的 zh 列再重建），是 `locales.tsv` 的可复现来源 |
| `tests/probe-private-gatt.py` | 只读探测本机私有 WinRT 激活工厂与接口 IID，供换 Windows 版本后诊断；不会连接手柄、不会写 GATT 特征 |

此外 `app/StadiaLink.ico` 是 `app/create-icon.ps1` 生成、被 `app/build.ps1` 引用的构建资源，随本仓库提供（新 `.gitignore` 不再忽略 `*.ico`）。它是二进制资源，**不放入文本补丁**。

## 三、仓库元数据与文档（不属于代码补丁）

- 新增 `.gitignore`（含"dist/ 有意纳入版本控制"说明）、`.gitattributes`
- 新增 `README.zh-CN.md`：完整的简体中文文档，章节与其他 4 语 README 一致，并增加本分支特有的「蓝牙震动兼容性」「本分支交付内容」「构建开关」等说明；「使用」一节补充了 × / Alt+F4 驻留托盘与托盘**退出**的行为
- `README.md` 顶部新增简体中文分支入口（指向中文 README 与版本说明）；`README.md` / `.en` / `.fr` / `.de` 的语言导航行均加入「🇨🇳 简体中文」链接；`LICENSE`、`UPSTREAM-README.md` 原样保留
- `CHANGELOG.md` 新增 `Unreleased — 1.0.2 hotfix (2026-11-09)` 条目（含关闭按钮修复），历史条目不变；`RELEASE_NOTES.md` 增加「关闭按钮行为」小节并更新产物哈希
- 新增 `RELEASE_NOTES.md`（中文版本说明）、本文件、`patches/`、`dist/`

## 四、未改动的部分

USB 震动路径、蓝牙输入与 XInput 翻译、配置模型与持久化、电池读取、托盘图标与电量显示、开机自启、更新检查、驱动签名与安装流程、INF 的设备匹配 ID、报告长度与既有字段布局。仅「关闭 / 恢复」的窗口行为按第一节调整。所有新增构建开关默认关闭，原有 `.\app\package.ps1 -Driver` 用法不变。

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

`dist/driver/` 中的 `winstadia.dll` / `winstadia.inf` 是**唯一**保留的 `driver/out/pkg` 产物（精简 WDK 后重新编译，代码与双 IID 未变；`DriverVer=11/09/2026,22.52.7.585`）。
