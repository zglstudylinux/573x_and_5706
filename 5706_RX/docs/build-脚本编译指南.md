# 用命令行脚本编译 AB5726 固件（脱离 Code::Blocks）

> 适用仓库：`D:\work\client\5726\SDK\app`（产品配置 `CONFIG_AB5706A_LE_MIC_RX`，编译器配置 `riscv32-v3`）。
> 目标：脱离 Code::Blocks GUI，用 PowerShell 脚本 + VSCode 任务在命令行完成与 GUI 完全等价的构建，并提供 IntelliSense（跳转/补全）。
> 做法：模仿 `D:\Code\AI\sdk_ab5766_le_mic2_v011_s8684_20260427\app_branch01` 工程的脚本编译方案，适配本工程差异。

## 1. 背景

此前固件只能在 Code::Blocks GUI 里编译。`projects/microphone/app.cbp` 是唯一的构建描述，仓库内没有 Makefile / CI 配置，脱离 GUI 就无法构建。本方案提供命令行构建脚本，并在 VSCode 里配置 `Ctrl+Shift+B` 一键编译与 IntelliSense，日常开发不必再开 Code::Blocks。

### 1.1 调研结论

- 工程引用编译器配置 `riscv32-v3`，其 `MASTER_PATH` 记录在 `%APPDATA%\codeblocks\default.conf`：本机实际工具链在 `D:\software_download\work\rv32-toolchain_v1.5.9\rv32-toolchain_v1.5.9\install\RV32-V3\bin`（`riscv32-elf-gcc` 14.2.0 / `ld` 2.43）。
- 该 bin 目录含 `riscv32-elf-gcc/ld/objcopy/xmaker`，**但无 objdump**。脚本用绝对路径定位工具链，并把 bin 前置进 `$env:Path`（`prebuild.bat`/`postbuild.bat` 用裸名调用工具）。
- 与 AB5766 参考工程相比，本工程构建流程多一步 **copy_tone**（按语言配置从 `res_tone\` 拷贝音效资源），且 `-Wno-address-of-packed-member` 不是全局选项、只对 `toolkit_effect.c` 单文件生效（来自 .cbp 的单文件 buildCommand）。
- 产品配置由 `config.h` 的 `USER_CONFIG` 宏选择（当前 `CONFIG_AB5706A_LE_MIC_RX` → `config_ab5706a_rx.h`），工程内共 8 个 `config_*.h` 变体，无需命令行 `-D`。
- `objdump` 缺失 → `app.lst` 不生成（postbuild 静默跳过，非致命）。

## 2. 交付物

| 文件 | 作用 |
| --- | --- |
| [projects/microphone/build.ps1](../projects/microphone/build.ps1) | 主构建脚本，解析 `app.cbp` 并复现 CB 的 Debug 构建流程 |
| [.vscode/tasks.json](../.vscode/tasks.json) | `build` / `clean` / `rebuild` 任务，`build` 绑定 `Ctrl+Shift+B` |
| [.vscode/c_cpp_properties.json](../.vscode/c_cpp_properties.json) | IntelliSense 配置（include 路径、compilerPath、intelliSenseMode） |
| `docs/build-脚本编译指南.md` | 本文档 |

复用、不修改的现有文件：`projects/microphone/app.cbp`（源清单与 flags 唯一来源）、`projects/microphone/Output/bin/prebuild.bat`、`projects/microphone/Output/bin/postbuild.bat`、`projects/microphone/Output/bin/copy_tone.xm`、`projects/microphone/ram.ld`、`projects/microphone/Output/bin/app.xm`。构建产物路径与 Code::Blocks 完全一致，脚本构建与 GUI 构建可随时混用。

## 3. 快速使用

### 3.1 命令行

在仓库根或任意目录执行（脚本内部会 `Set-Location` 到 `projects\microphone`）：

```powershell
# 全量构建
powershell -ExecutionPolicy Bypass -File projects/microphone/build.ps1

# 清理（仅清理，不构建）
powershell -ExecutionPolicy Bypass -File projects/microphone/build.ps1 -Clean

# 清理 + 构建
powershell -ExecutionPolicy Bypass -File projects/microphone/build.ps1 -Rebuild

# 显式指定工具链根目录（绕过自动探测）
powershell -ExecutionPolicy Bypass -File projects/microphone/build.ps1 -ToolchainRoot D:\...\RV32-V3
```

也可设置环境变量 `RV32_TOOLCHAIN_ROOT` 指向工具链根目录，脚本会优先采用。

### 3.2 VSCode 任务

- `Ctrl+Shift+B` → 执行 `build` 任务（默认构建任务）。
- 命令面板 → `Tasks: Run Task` → 选择 `clean` 或 `rebuild`。
- 输出面板的编译错误可点击跳转源码（`problemMatcher: $gcc`）。

### 3.3 期望产物

构建成功后 `projects/microphone/Output/bin/` 下生成：

- `app.rv32` —— 链接后的 ELF（约 245 KB，与 GUI 构建一致）
- `app.bin` —— objcopy 生成的裸二进制（约 149 KB）
- `app.dcf` —— xmaker 生成的下载镜像（约 247 KB）
- `map.txt` —— 链接 map
- `Output/obj/` 下各级 `.o` —— 中间产物（120 个源文件 + `ram.o`/`appxm.o`）

`app.lst`（反汇编）仅在工具链含 `objdump.exe` 时生成；本机 RV32-V3 缺失该工具，故不生成（非致命）。

## 4. 构建流程详解（与 Code::Blocks 对齐）

build.ps1 严格复现 `app.cbp` 描述的 Debug 构建顺序：

1. **工具链探测**：按优先级自动定位工具链根目录（详见 §5），把 `bin` 前置进 `$env:Path`。
2. **prebuild（3 步，对应 .cbp 的 ExtraCommands before）**：
   a. `riscv32-elf-gcc $CFLAGS $INCLUDES -E -P -x c -c Output\bin\copy_tone.xm -o Output\bin\copy_tone.bat` —— 把 C 预处理器与批处理混合的脚本预处理成真正的 .bat；
   b. 执行 `copy_tone.bat` —— 按 `config.h` 里的语言选择（`LANG_SELECT` 等）从 `Output\bin\res_tone\` 拷贝音效/EQ 资源到 `Output\bin\res\`；
   c. `cmd /c "Output\bin\prebuild.bat app"` —— 先在各 `.o` 位置写占位文件（CB 依赖跟踪用），再 `xmaker -b res.xm` 生成 `res.bin/res.h`、`xmaker -b xcfg.xm` 生成 `xcfg.bin/xcfg.h`，并按 `res.xm` 的 `xcopy()` 指令把 `res.h/kbox.c/kbox.h` 复制进工程目录。
3. **解析 app.cbp**：用 `[xml]` 加载，枚举所有 `Unit` 节点，仅取含 `<Option compilerVar="CC">` 的（共 120 个 `.c`）——这天然排除 `app.xm`/`download.xm`/`xcfg.xm`/`res.xm`/`ram.ld`（它们是 `compile="1"` 但无 `compilerVar="CC"`）。单文件 buildCommand 中夹在 `$options` 与 `$includes` 之间的额外选项被提取为该文件的附加 flags（目前仅 `toolkit_effect.c` 的 `-Wno-address-of-packed-member`）。每个源的 `.o` 输出路径 = `Output\obj\<相对仓库根的源路径去 .c 加 .o>`。
4. **预处理 ram.ld**：`riscv32-elf-gcc $CFLAGS $INCLUDES -E -P -x c -c ram.ld -o Output\obj\ram.o`（链接脚本是 GNU ld 语法混 C 预处理 `#include "config.h"`，需先展开）。
5. **预处理 app.xm**：`riscv32-elf-gcc $CFLAGS $INCLUDES -E -P -x c -c Output\bin\app.xm -o Output\bin\appxm.o`（postbuild 里 xmaker 的打包输入）。
6. **编译所有 .c**：循环 `riscv32-elf-gcc $CFLAGS [单文件附加flags] $INCLUDES -c <src> -o <obj>`，每条检查 `$LASTEXITCODE`，非 0 立即报错退出。
7. **链接**：`riscv32-elf-ld -o Output\bin\app.rv32 <所有 .o，按 cbp 顺序> -TOutput\obj\ram.o --gc-sections -Map=Output\bin\map.txt --no-warn-rwx-segments <libs>`。
8. **postbuild**：`cmd /c "Output\bin\postbuild.bat app"`。`objcopy -O binary` 生成 `app.bin`；`xmaker -b appxm.o` 把 `header.bin + app.bin + res.bin + xcfg.bin + updater.bin` 打包成 `app.dcf`；`C:\upload\upload.bat` 不存在则跳过上传；objdump 不存在则跳过 `app.lst`；最后 `xmaker -b download.xm` 生成下载镜像。

### 4.1 关键常量（取自 app.cbp，工作目录 = projects\microphone）

- **CFLAGS**：`-Os -Wall -march=rv32imc_zba_zbb_zbc_zbs_zca_zcb_zcmp_xbs1 -Wno-array-bounds -ffunction-sections -mjump-tables-in-text`（注意 `-Wno-address-of-packed-member` 不在其中，仅对 `toolkit_effect.c` 单文件附加）
- **INCLUDES**（24 个 `-I`）：`-I. -Idisplay -Imessage -Iport -Iplugin -I..\..\libs -I..\..\include -I..\..\3rd-party -I..\..\bsp -I..\..\system -I..\..\modules -I..\..\functions -I..\..\modules\gui -I..\..\modules\gui\ledseg -I..\..\modules\gui\lcd -I..\..\modules\gui\led -I..\..\modules\bluetooth\app\ab_mate -I..\..\modules\bluetooth\gfps -I..\..\modules\bluetooth\call -I..\..\modules\fm -I..\..\os -I..\kbox -I..\..\modules\wireless -I..\..\modules\wireless\wireless_con_interact`（其中 3 个目录磁盘上不存在，属于工程里的陈旧条目，`-I` 不存在目录不报错）
- **LIBS**（预置静态库，均在 `app\libs\`）：`..\..\libs\libplatform.a ..\..\libs\libbtstack.a ..\..\libs\libcodecs.a ..\..\libs\libdrivers.a ..\..\libs\libvoices.a`

### 4.2 .o 路径映射规则

| 源文件（相对 projects\microphone） | .o 输出 |
| --- | --- |
| `../../3rd-party/i2s/ta5711.c` | `Output\obj\3rd-party\i2s\ta5711.o` |
| `main.c` | `Output\obj\projects\microphone\main.o` |
| `port/port_key.c` | `Output\obj\projects\microphone\port\port_key.o` |

与 Code::Blocks 的 `.o` 布局完全一致。脚本为每个 `.o` 的父目录自动 `New-Item -ItemType Directory -Force`。

### 4.3 清理范围（-Clean / -Rebuild）

- 删除：`Output\obj\`（递归）+ `Output\bin\` 下 `app.rv32, app.bin, app.dcf, app.lst, appxm.o, map.txt`。
- 保留：`res.bin, xcfg.bin, res.h, xcfg.h, copy_tone.*, *.xm, header.bin, updater.bin, unpack.bin, res\, res_tone\, Settings\, *.bat, kbox.*`。

`-Clean` 后不自动构建；`-Rebuild` = 清理后再构建。

## 5. 工具链定位

脚本按以下优先级探测 RV32 工具链根目录（校验 `bin\riscv32-elf-gcc.exe` 存在）：

1. 参数 `-ToolchainRoot`；
2. 环境变量 `RV32_TOOLCHAIN_ROOT`；
3. 解析 `%APPDATA%\codeblocks\default.conf` 中 `riscv32_v3` 编译器集的 `MASTER_PATH`（CDATA）；
4. 硬编码本机历史路径 `D:\software_download\work\rv32-toolchain_v1.5.9\rv32-toolchain_v1.5.9\install\RV32-V3`；
5. 旧默认 `C:\Program Files (x86)\RV32-Toolchain\RV32-V3`。

探测到后把 `bin` 前置进 `$env:Path`，使 `prebuild.bat`/`postbuild.bat` 的裸名工具调用（`riscv32-elf-xmaker`、`riscv32-elf-objcopy`）可解析。

> 若工具链安装位置变化且未登记到 Code::Blocks 配置，请用 `-ToolchainRoot` 或环境变量显式指定。

## 6. IntelliSense 配置说明

`.vscode/c_cpp_properties.json`（配置名 `AB5726`）：

- `includePath`：将 build.ps1 的 24 个 `-I` 路径转为相对 `${workspaceFolder}` 的形式。
- `compilerPath`：指向 RV32-V3 的 `bin/riscv32-elf-gcc.exe`；安装位置不同请改此字段。
- `compilerArgs`：`-march=rv32imc_zba_zbb_zbc_zbs_zca_zcb_zcmp_xbs1 -ffunction-sections -mjump-tables-in-text`，确保 `__riscv_*` 等架构宏被正确展开。
- `intelliSenseMode`: `gcc-x86`（rv32 为 32 位 ilp32，C/C++ 扩展无 riscv 专用模式，x86 最接近）。
- 不加额外 `defines`：产品选择已在 `config.h` 内部用 `USER_CONFIG` 完成，IntelliSense 可自行展开。

## 7. 排错

| 现象 | 原因 / 处理 |
| --- | --- |
| `未找到 RV32 工具链` | 用 `-ToolchainRoot` 指定，或设 `RV32_TOOLCHAIN_ROOT` 环境变量。确认 `bin\riscv32-elf-gcc.exe` 存在。 |
| prebuild 报 `riscv32-elf-xmaker 不是内部或外部命令` | 工具链 bin 未进 PATH；脚本已自动前置，若手动调用 .bat 需先 `set PATH=<bin>;%PATH%`。 |
| 编译报找不到头文件 | 确认从 `projects\microphone` 目录调用，`-I` 路径基于此目录。脚本内部已 `Set-Location`。 |
| 链接报 `undefined reference` | 脚本以 `.cbp` 为唯一源清单来源；新源文件请先在 Code::Blocks 里加入 `app.cbp` 再重建。 |
| copy_tone 资源不对 | 资源选择由 `config.h`/`config_ab5706a_rx.h` 的语言与音效宏决定，改宏后重新构建即可。 |
| 无 `app.lst` | RV32-V3 缺 `objdump.exe`，postbuild 静默跳过。如需反汇编，安装含 objdump 的 RV32 工具链。 |
| `C:\upload\upload.bat` 报错 | 该上传脚本由硬件烧录流程提供（`upload.bat -D AB5700 app.dcf`），不存在则 postbuild 自动跳过。 |
| 切换产品配置 | 改 `projects/microphone/config.h` 的 `USER_CONFIG` 宏指向其它 `config_*.h`（工程内共 8 个变体），无需改脚本。 |

## 8. 验证清单

1. 命令行运行 `powershell -ExecutionPolicy Bypass -File projects/microphone/build.ps1 -Rebuild`，期望：prebuild（copy_tone → xmaker）→ 预处理 `ram.o`/`appxm.o` → **120 个 `.c` 编译 0 错** → 链接生成 `app.rv32`（约 245 KB）→ postbuild 生成 `app.bin`（约 149 KB）、`app.dcf`（约 247 KB）。
2. 产物路径/命名与 Code::Blocks GUI 构建完全一致（`Output\bin\`、`Output\obj\`），两种构建方式可互换混用。
3. `Ctrl+Shift+B` 触发 build 任务，输出面板编译错误可点击跳转源码。
4. `./build.ps1 -Clean` 后 `Output\obj\` 与 `app.*` 产物被清除，预置 bin 与 res/xcfg/copy_tone 生成物保留；再跑 `-Rebuild` 全量重建成功。
5. IntelliSense：打开 `system/system.c` 等文件，`#include` 不报红、可跳转到 SDK 头文件。
