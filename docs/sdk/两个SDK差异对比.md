# 两个 SDK 差异对比（5706_RX vs 5726_TX）

> 本文件对比本仓库中的两套 JieLi 无线麦 SDK：
> - `5706_RX/`：CPU 平台 **AB5700**（目录名「RX」只是本项目的接收端用法，SDK 本身收发都支持）
> - `5726_TX/`：CPU 平台 **BT8910**（目录名「TX」只是本项目的发送端用法，SDK 本身收发都支持）
>
> 所有结论均来自实际代码，标注了「文件 + 行号」。配套阅读：
> - [`5706_RX_SDK多芯片支持原理.md`](5706_RX_SDK多芯片支持原理.md)
> - [`5726_TX_SDK多芯片支持原理.md`](5726_TX_SDK多芯片支持原理.md)

---

## 一句话结论

两套 SDK 同属 JieLi 无线麦 SDK 家族，代码风格一致，但**目标 CPU 平台不同**：`5706_RX` 基于 **AB5700** 平台，`5726_TX` 基于 **BT8910** 平台。平台不同，导致寄存器映射（`sfr.h`）、预编译库（`.a`）、目录组织都不一样；但「一个 SDK 支持多种芯片」的机制、以及「收发角色机制（`WIRELESS_MIC_ROLE`）」是**相同**的，差异只在出厂 config 怎么选（见第 4 节与另外两份文档）。

平台标识来自各自的 `cpu.h`：

| SDK | cpu.h | STR_CPU | LIB_CPU |
|-----|-------|---------|---------|
| 5706_RX | `5706_RX/libs/cpu.h` L4-5 | `"AB5700"` | `AB5700` |
| 5726_TX | `5726_TX/libs/bt8910/cpu.h` L4-5 | `"BT8910"` | `BT8910` |

`LIB_CPU` 注释为 `//for copy libs`，即它决定构建时拷贝/链接哪一套平台库。

---

## 差异总表

| 维度 | 5706_RX（AB5700） | 5726_TX（BT8910） | 代码证据 |
|------|------------------|------------------|----------|
| CPU 平台标识 | `AB5700` | `BT8910` | `libs/cpu.h` L4-5 / `libs/bt8910/cpu.h` L4-5 |
| 预编译库数量 | 5 个 | 6 个（多 `libeffects.a`） | `build.ps1` L157-158 / L160-165 |
| 预编译库路径 | `libs/*.a` | `libs/bt8910/liba/*.a` | 同上 |
| 平台头文件位置 | `include/`（sfr/io_def/config_define/config_extra） | `projects/microphone/header/` | `build.ps1` L149 / L148 |
| 外设驱动位置 | 根目录 `bsp/`（`bsp.h` 引入） | `projects/microphone/cpu/`（`periph.h` 引入） | `include/include.h` L15 / L14 |
| 角色选择机制 | 相同（`FUNC_*_EN` → `WIRELESS_MIC_ROLE` 0/1/2） | 相同 | `include/config_extra.h` L581-589 / `header/config_extra.h` L729-737 |
| 出厂 config 的角色选择 | 每个 config 只开一个角色（编译期固定 TX 或 RX） | 所有 config 都 both=1（运行期 xcfg 选） | `config_ab5706a_rx.h` L15-16、`config_ab5706a_tx.h` L15-16 / `config_ab5706a_le_mic.h` L17-18 |
| Flash 下限 | 支持 256K（`FSIZE_256K`） | 最低 512K（无 `FSIZE_256K`） | `config_ab5706a_rx.h` L38 / `config_ab5706a_le_mic.h` L55 |
| 蓝牙功能 | 关（`FUNC_BT_EN 0`），代码被裁剪 | 开（`FUNC_BT_EN 1`），含完整 BT 栈 | `config_ab5706a_rx.h` L17 / `config_ab5706a_le_mic.h` L16 |
| 编解码枚举 index 5 | `WS_CODEC_USER0` | `WS_CODEC_LC3F`（LC3 5ms） | `include/config_define.h` L436 vs `header/config_define.h` L403 |
| SDK 版本 | `0x0150` | `0x013` | `include/config_extra.h` L8 / `header/config_extra.h` L8 |
| 内存堆大小 | `9*1024` | `13*1024` | `main.c` L3 / `main.c` L3 |
| 寄存器映射差异 | 有 GPIO Port A（`GPIOADE` 等） | Port A/F 被注释，用 B/E/G | `include/sfr.h` L443 / `header/sfr.h` L248-302 |

---

## 详细说明

### 1. CPU 平台不同 → 寄存器映射与库不同

`sfr.h` 是寄存器映射文件，用内存映射方式定义寄存器（见多芯片原理文档）。两份 `sfr.h` 是**两份不同平台**的寄存器表：

- RX `include/sfr.h` L443 定义了 `GPIOADE`（SFR6 组的 Port A 数据使能寄存器）。
- TX `header/sfr.h` L248-260 把 Port A（`GPIOASET`~`GPIOAPD300`）**整段注释掉**，L262-288 只定义 Port B/E，L305+ 定义 Port G。

这说明 BT8910 与 AB5700 的外设/IO 布局不同，两套 SDK 各自只带一套平台寄存器表。

预编译库也按平台分：
- RX：`libs/libplatform.a`、`libbtstack.a`、`libcodecs.a`、`libdrivers.a`、`libvoices.a`（5 个，**无** libeffects）。
- TX：`libs/bt8910/liba/` 下 6 个，**多** `libeffects.a`（音效库）。

链接清单见 `build.ps1`：RX L157-158、TX L160-165。

### 2. 平台头文件的位置不同

- RX 把平台头 `sfr.h / io_def.h / config_define.h / config_extra.h` 放在**全局** `include/`（`build.ps1` L149 的 `-I..\..\include` 即可找到）。
- TX 把同样的四份头放在**项目内** `projects/microphone/header/`（`build.ps1` L148 的 `-Iheader` 找到），`include/` 只留 `global.h / include.h / macro.h / clib.h / typedef.h / s_common.h` 这类通用头。

### 3. 外设驱动目录不同

- RX：根目录 `bsp/`，通过 `include/include.h` L15 `#include "bsp.h"` 引入。
- TX：`projects/microphone/cpu/`（`periph.h`、`gpio.c`、`uart0.c`、`i2c.c`、`saradc.c`…），通过 `include/include.h` L14 `#include "periph.h"` 引入。

### 4. 收发角色机制相同，差异只在出厂配置（关键）

两套 SDK 的收发角色机制**完全相同**：用 `FUNC_DEVICE_EN` + `FUNC_ADAPTER_EN` 在 `config_extra.h` 里派生 `WIRELESS_MIC_ROLE`：

```c
// RX include/config_extra.h L581-589 / TX header/config_extra.h L729-737（逻辑逐字一致）
#if FUNC_ADAPTER_EN && FUNC_DEVICE_EN
    #define WIRELESS_MIC_ROLE  2    // 0=TX, 1=RX, 2=收发一体（运行期配置选择）
#elif FUNC_ADAPTER_EN
    #define WIRELESS_MIC_ROLE  1    // 只做接收端 RX
#elif FUNC_DEVICE_EN
    #define WIRELESS_MIC_ROLE  0    // 只做发送端 TX
#else
    #define WIRELESS_MIC_ROLE  0xff // 都不支持
#endif
```

`wireless_mic_role_init()` 再按 ROLE 决定 `cfg_wireless_role`（RX `wireless_proc.c` L312-316、TX L323-339）：

```c
#if WIRELESS_MIC_ROLE == 0      cfg_wireless_role = false;  // 固定 TX
#elif WIRELESS_MIC_ROLE == 1    cfg_wireless_role = true;   // 固定 RX
#elif WIRELESS_MIC_ROLE == 2
    if (xcfg_cb.wireless_adapter_en)      cfg_wireless_role = true;   // RX
    else if (xcfg_cb.wireless_device_en)  cfg_wireless_role = false;  // TX
    else                                  cfg_wireless_role = false;  // 默认 TX
#endif
```

**真正区别在出厂 config 怎么设：**

- **AB5700 SDK（5706_RX）**：每个 config 只开一个角色 → ROLE=0 或 1（编译期固定）。
  - RX 配置：`config_ab5706a_rx.h` L15-16 `FUNC_DEVICE_EN 0`、`FUNC_ADAPTER_EN 1`
  - TX 配置（存在！）：`config_ab5706a_tx.h` L15-16 `FUNC_DEVICE_EN 1`、`FUNC_ADAPTER_EN 0`；`config_ab570x_tx.h` L15-16、`config_low_latency_tx.h` L16-17 同
  - 所以 `5706_RX/` 目录既能做 RX 也能做 TX，只是把 `config.h` 的 `USER_CONFIG` 选到对应 `_tx.h` 配置即可。

- **BT8910 SDK（5726_TX）**：所有 config 都 both=1 → ROLE=2（运行期 xcfg 选）。
  - `config_ab5706a_le_mic.h` L17-18 两个都开。
  - 运行期依据 `xcfg.h` L71 `u32 wireless_adapter_en : 1;`、L72 `u32 wireless_device_en : 1;`（烧录时配置工具写入）。

因此：同一份 TX 固件，烧 `wireless_mic_emit` 档 = 发送端、`wireless_adapter` 档 = 接收端，无需重编译；而 RX SDK 要换角色则改 config 重编译。

### 5. 其它功能面差异

- 蓝牙：TX 打开完整蓝牙（`FUNC_BT_EN 1`），带 `modules/bluetooth/` 全套 + `ab_mate` 应用；RX 关蓝牙（`FUNC_BT_EN 0`），仅保留 `libs/strong_*.c` 等空桩裁剪。
- Flash：RX 有 `FSIZE_256K`（`include/config_define.h` L31，适配 5706B 的 256K 封装）；TX 的 `header/config_define.h` 只有 512K/1M/2M/4M。
- 编解码：两份 `config_define.h` 的 `WS_CODEC_*` 枚举数量相同（各 11 个，ADPCM/OPUS/MSBC/LC3/LC3S/SBC/LC3B 都有），唯一差异是 index 5：RX 为 `WS_CODEC_USER0`（`include/config_define.h` L436），TX 为 `WS_CODEC_LC3F`（LC3 5ms，`header/config_define.h` L403）。

---

## 目录结构对比（mermaid）

```mermaid
flowchart LR
    subgraph RX["5706_RX（AB5700 平台）"]
        direction TB
        R_INC["include/<br/>sfr.h io_def.h<br/>config_define.h config_extra.h"]
        R_BSP["bsp/<br/>bsp.h + bsp_*.c"]
        R_LIB["libs/<br/>5 个 .a"]
    end
    subgraph TX["5726_TX（BT8910 平台）"]
        direction TB
        T_INC["include/<br/>仅通用头"]
        T_HDR["projects/microphone/header/<br/>sfr.h io_def.h<br/>config_define.h config_extra.h"]
        T_CPU["projects/microphone/cpu/<br/>periph.h + gpio.c 等"]
        T_LIB["libs/bt8910/liba/<br/>6 个 .a"]
    end
    RX --- TX
```

---

## 知识点

1. **`LIB_CPU` / `STR_CPU`**：`cpu.h` 里的平台标识。`STR_CPU` 用于打印版本串（`main.c` L36/L55 `printf("Hello %s", STR_CPU)`），`LIB_CPU` 用于「拷贝/链接哪一套平台库」（`//for copy libs`）。

2. **平台库 `.a`**：芯片底层的硬件驱动、协议栈、编解码器以预编译静态库形式提供，一套库对应一个平台；同一平台的所有芯片共用这套库（这就是「一个 SDK 支持多芯片」的硬件底座，详见多芯片原理文档）。

3. **收发角色机制**：`FUNC_DEVICE_EN / FUNC_ADAPTER_EN` 控制是否编译发射/接收代码，`config_extra.h` 把它们派生为 `WIRELESS_MIC_ROLE`（0=TX、1=RX、2=收发一体）。两者都开 = 收发一体，角色交给 xcfg 运行时决定；只开一个 = 编译期固定角色（省 Flash，注释原文「节省空间」）。两套 SDK 此机制一致，只是出厂 config 选值不同。

4. **`FSIZE_256K`**：Flash 容量枚举，RX 有（适配 5706B 小容量封装），TX 没有（BT8910 系列最低 512K）。Flash 大小直接决定 `FLASH_CODE_SIZE`（程序区上限）与 FOTA 分区规划。
