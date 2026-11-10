# 兼容性矩阵与设备接口自适应策略

本文件是判断「某个 Windows 版本能不能用蓝牙震动」的依据。**驱动能安装**（UMDF 2.31、x64、INF 设备匹配）与**私有 GATT 震动接口能工作**是两件独立的事，前者不蕴含后者。

## 1. 兼容性矩阵

| Windows 版本      | Build | 蓝牙输入 | 蓝牙震动 | 设备接口 IID                               | 状态  |
| --------------- | ----- | ---- | ---- | -------------------------------------- | --- |
| Windows 10 22H2 | 19045 | 待验证  | 待验证  | 未知（自动适配）                              | 未验证 |
| Windows 11 22H2 | 22621 | 待验证  | 待验证  | 未知（自动适配）                              | 未验证 |
| Windows 11 23H2 | 22631 | 已验证  | 已验证  | `0799A6A6-E942-5218-8A0E-7C9B996E4158` | 通过  |
| Windows 11 24H2 | 26100 | 待验证  | 待验证  | 未知（自动适配）                              | 未验证 |
| Windows 11      | 26200 | 已验证  | 已验证  | `6BBEDE1F-797F-5C17-8AC7-3F342037CB59` | 通过  |

- 「已验证」= 实机完成安装、输入与震动写入，且 `driver/status.ps1` 观察到 `probeStatus=0`、阶段 `127`、`writes` 持续增长。
- 「未验证」= 未做实机测试。程序会按第 2 节**自动尝试适配**设备接口 IID，但不承诺成功；`driver/status.ps1` 的 `device IID` 会显示 `cached` 或 `auto-adapted`，说明是否命中。
- **Windows 10 没有实测 IID 数据**。UMDF 2.31（Win10 1809+）只说明框架可安装，不代表私有 `GattClientDevice` 设备接口存在或布局相同；因此**不承诺 Windows 10 的蓝牙震动可用**。自动适配会尝试，但首次结果必须实机确认。
- 共享 statics IID `CBAA1A5F-DCEF-508B-ACC6-158F1E160C0F` 仅在 22631 / 26200 上验证一致。

## 2. 未知版本的分层自动适配（默认开启）

目标：不因缺少硬编码 IID 就停用震动，也绝不把「某个 IID 看起来像参数化接口」当作可以安全调用的理由。

解析顺序：

1. 两个硬编码、实机验证过的 IID（26200、22631）。命中后走**原始代码路径**，不经过任何新逻辑，因此这两个系统上已验证的行为完全不变。
2. 上次成功验证并缓存的 IID（`PrivateGatt` 注册表项中的 `Verified`）。
3. `IInspectable::GetIids` 枚举出的运行时候选。

候选必须依次通过下面五关，才会被用来写震动数据：

| 关卡 | 做法 | 挡住什么 |
| --- | --- | --- |
| 筛选 | `driver/iid-policy.h`：版本 5 参数化接口（`Data3` 高 4 位为 `5`）、RFC 4122 变体位合法、排除 IUnknown / IInspectable / IAgileObject / IStringable / IClosable / 共享 statics 与两个已验证 IID；由 `tests/iid-policy-tests.cpp` 的 26 项断言覆盖 | 把 WinRT 管道接口误认成设备接口 |
| vtable 静态校验 | 只读取不执行：槽 6–14 必须都指向已提交的可执行内存，且至少 5 个不同实现；按「与激活工厂实现位于同一映像」的方法数排序 | 占位接口、垃圾指针、明显无关的接口 |
| 完整链路语义验证 | 复用原有 `Open()`：HID 服务 UUID `0x1812` 恰好 1 个、服务 UUID 匹配、Report Reference `5/2`。只有全部通过才认为该 IID 可用 | 方法槽语义不同（`GetIids` 本身无法解决 vtable 顺序变化） |
| 调用隔离 | 未知 / 缓存接口的调用包在 `GuardedTryOpen()` 的 SEH 中，访问违例变成 HRESULT 而不是击穿 WUDFHost.exe | 猜错后宿主进程崩溃 |
| 持久化与熔断 | 成功写 `Verified`；失败或崩溃的 IID 写 `Blacklist`（最多 4 个）；尝试前写 `Attempt` 标记，若宿主崩溃导致标记残留，下次启动自动把该 IID 拉黑。每次最多尝试 3 个候选 | 崩溃循环、每次开机重复试错 |

降级行为：

- 注册表不可写时所有持久化静默跳过，自动适配照常工作，只是失去跨重启熔断。
- 候选全失败：阶段 `12`，停用蓝牙震动，等待按第 4 节补 IID。
- 需要严格策略时，用 `$env:STADIALINK_AUTO_IID=0; .\driver\build.ps1` 构建「失败关闭」版本：只枚举计数、阶段 `11`、不调用任何未验证接口。

诚实边界：

- 自动适配解决的是**接口 IID 变化**，不解决 **vtable 方法顺序变化**；若新系统连方法布局都改了，链路校验会失败并安全停用。
- SEH 能拦截多数访问违例，但不能保证进程内状态完好，`Attempt` 标记与黑名单就是为此存在。
- 上述自适应路径在 22631 / 26200 上**不会触发**（命中硬编码 IID），因此没有实机覆盖；其正确性来自编译、26 项策略单测、已验证路径的回归测试与代码审查。首次在新系统使用时请按第 4 节做一次实机确认。

## 3. 状态报告诊断字节

| 字节 | 含义                                                                                        |
| -- | ----------------------------------------------------------------------------------------- |
| 60 | Stage：`5` = 设备 QI，`11` = 无已验证 IID 且自动适配不可用，`12` = 候选全部未通过，`127` = ready            |
| 61 | IidIndex：`1` = 26200，`2` = 22631，`0x40` = 缓存 IID，`0x80+i` = 本次自动验证通过（i 为 GetIids 序号）     |
| 62 | CandidateCount：低 6 位 = 候选数（上限 63）；`0x40` = 试过缓存 IID；`0x80` = 上次宿主崩溃、该 IID 已拉黑      |
| 63 | AsyncStage：`Await()` 失败点，`1` QI(IAsyncInfo)、`2` get_Status、`3` GetResults、`4` 异步错误、`5` 超时 |

字节 60–63 原本空闲，报告长度与既有字段布局均未改变。`driver/status.ps1` 会翻译这些值，并在阶段 `11` / `12` 给出下一步提示。

## 4. 自动适配失败后补入硬编码 IID

自动适配成功时无需本节；仅当阶段 `11` / `12`、或希望把 IID 固化进白名单时才需要：

1. 在目标系统运行 `python tests/probe-private-gatt.py`（只读枚举 activation factory 的 IID；设备 facet 属于 `CreateAsync` 返回的对象，此处对设备 IID 返回 `E_NOINTERFACE` 属预期）。
2. 用 `driver/status.ps1` 读取第 61/62 字节：若 `IidIndex` 为 `0x80+i` 说明自动适配已成功，可把对应 IID 从 `GetIids` 输出中抄出；若全是 `0`，从系统 inbox 二进制（如 `HidOverGatt.dll`）或公共符号中定位与共享 statics IID 相邻的设备接口 IID。**不要**仅凭 `0x5xxx` 特征猜。
3. 确认该接口 vtable 槽位 11（HID 服务枚举）与 14（打开服务对象）语义未变。布局变化必须重新分析，不能只换 IID。
4. 把 IID 加入 `driver/private-gatt.cpp` 的 `DeviceIids[]` 与 `driver/iid-policy.h` 的排除表，并更新本矩阵、`RELEASE_NOTES.md` 与 `docs/CHANGES.md`。
5. 安装后实机验证：输入正常、`driver/status.ps1` 显示阶段 `127`、`writes` 增长、`writeHRESULT=0x00000000`，并实际感受到震动。
6. 用默认构建（不带任何开关）重新发布。

## 5. 重置缓存与黑名单

缓存 IID 失效时驱动会自动清除 `Verified` 并重新发现，通常无需手工干预。若某个 IID 被误拉黑、需要强制重来，可用管理员 PowerShell 清除 UMDF 宿主账户（LocalService）下的状态：

```powershell
Remove-ItemProperty -Path 'Registry::HKEY_USERS\S-1-5-19\SOFTWARE\StadiaLink\PrivateGatt' -Name Verified,Blacklist,Attempt -ErrorAction SilentlyContinue
```

状态键位于 `HKCU` 对应的 `HKEY_USERS\S-1-5-19`，普通用户会话的 `HKCU:` 指向自己，删除那里不起作用。重置后驱动会在下一次连接时重新走第 2 节流程。

## 6. 维护约定

后续维护请把「驱动 ABI 兼容性」「界面 / 本地化」「其他功能修复」拆成独立提交，便于审查与回滚。按本次要求，两次更新合并在同一分支，未改写已有历史。
