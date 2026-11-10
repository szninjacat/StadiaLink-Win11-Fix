# 兼容性矩阵与新增 IID 验证流程

本文件是判断「某个 Windows 版本能不能用蓝牙震动」的依据。**驱动能安装**（UMDF 2.31、x64、INF 设备匹配）与**私有 GATT 震动接口能工作**是两件独立的事，前者不蕴含后者。

## 1. 兼容性矩阵

| Windows 版本 | Build | 蓝牙输入 | 蓝牙震动 | 设备接口 IID | 状态 |
| --- | --- | --- | --- | --- | --- |
| Windows 10 22H2 | 19045 | 待验证 | 待验证 | 未知 | 未验证 |
| Windows 11 22H2 | 22621 | 待验证 | 待验证 | 未知 | 未验证 |
| Windows 11 23H2 | 22631 | 已验证 | 已验证 | `0799A6A6-E942-5218-8A0E-7C9B996E4158` | 通过 |
| Windows 11 24H2 | 26100 | 待验证 | 待验证 | 未知 | 未验证 |
| Windows 11 | 26200 | 已验证 | 已验证 | `6BBEDE1F-797F-5C17-8AC7-3F342037CB59` | 通过 |

- 「已验证」= 实机完成安装、输入与震动写入，且 `driver/status.ps1` 观察到 `probeStatus=0`、阶段 `127`、`writes` 持续增长。
- **Windows 10 没有实测 IID 数据**。UMDF 2.31（Win10 1809+）只说明框架可安装，不代表私有 `GattClientDevice` 设备接口存在或布局相同；因此**不承诺 Windows 10 的蓝牙震动可用**。下一步是先在 Windows 10 实机上做只读探测，而不是补一个猜测的 IID。
- 共享 statics IID `CBAA1A5F-DCEF-508B-ACC6-158F1E160C0F` 仅在 22631 / 26200 上验证一致。

## 2. 失败关闭（fail-closed）

`driver/private-gatt.cpp` 的 `DeviceIids[]` 只保存经实机 ABI 验证的 IID。未知 build 上的流程：

1. 依次对两个已知 IID 调用 `QueryInterface`；只有 `E_NOINTERFACE` 才继续，其他 HRESULT 立即返回，不被下一个 IID 掩盖。
2. 都失败时调用 `IInspectable::GetIids` 枚举全部候选，把 `Data3 & 0xF000 == 0x5000` 且非已知的候选数量写入状态报告第 62 字节。
3. 阶段设为 `11`（`STADIA_GATT_STAGE_IID_UNVERIFIED`），返回 `HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)`，**不调用**候选接口的 vtable 槽位 11/14。

原因：WinRT 参数化接口的 `0x5xxx` 特征不能证明该接口就是 `GattClientDevice` 设备 facet。若对象实现多个此类接口，选中错误的一个会在 UMDF 宿主进程内按错误的 vtable 布局调用函数，可能导致宿主崩溃。宁可停用蓝牙震动，也不冒险调用。

代价：新 Windows 版本上震动不会自动恢复，需按第 4 节补一个已验证 IID。`GetIids` 也**不能**自动解决 vtable 方法顺序变化。

## 3. 状态报告诊断字节

| 字节 | 含义 |
| --- | --- |
| 60 | Stage：主阶段；`5` = 设备 QI，`11` = 无已验证 IID，`127` = ready |
| 61 | IidIndex：`1` = 26200，`2` = 22631，`>=0x80` = 测试构建运行时发现 |
| 62 | CandidateCount：未验证候选数量 |
| 63 | AsyncStage：`Await()` 失败点，`1` QI(IAsyncInfo)、`2` get_Status、`3` GetResults、`4` 异步错误、`5` 超时 |

字节 60–63 原本空闲，报告长度与既有字段布局均未改变。`driver/status.ps1` 会把这些值翻译成可读文字，并在阶段 `11` 时给出提示。

## 4. 把新 IID 加入白名单（必须实机验证）

1. 在目标系统运行 `python tests/probe-private-gatt.py`（只读，枚举 activation factory 的 IID；设备 facet 属于 `CreateAsync` 返回的对象，不在这里，因此此处对设备 IID 返回 `E_NOINTERFACE` 是预期的）。
2. 从系统 inbox 二进制（如 `HidOverGatt.dll`）或公共符号中定位与共享 statics IID 相邻的设备接口 IID；**不要**仅凭 `0x5xxx` 特征猜。
3. 确认该接口 vtable 槽位 11（HID 服务枚举）与 14（打开服务对象）语义未变。布局变化必须重新分析，不能只换 IID。
4. 临时测试构建：`$env:STADIALINK_ALLOW_UNVERIFIED_IID=1; .\driver\build.ps1`。该开关仅在恰好 1 个候选时允许自动选择，禁止用于分发。也可用 16 字节 IID 热补丁先验证根因。
5. 安装后实机验证：输入正常、`driver/status.ps1` 显示阶段 `127`、`writes` 增长、`writeHRESULT=0x00000000`，并实际感受到震动。
6. 全部通过后才把 IID 加入 `DeviceIids[]`，更新本矩阵与 `RELEASE_NOTES.md`，并用**不带开关**的默认构建重新发布。

## 5. 维护约定

后续维护请把「驱动 ABI 兼容性」「界面 / 本地化」「其他功能修复」拆成独立提交，便于审查与回滚。本次加固单独成一次提交，不改写已有历史。
