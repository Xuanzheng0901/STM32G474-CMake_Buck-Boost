# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

基于 **STM32G474VET6** (170MHz Cortex-M4F) 的**三相 DC-AC 逆变器**，采用 SPWM (正弦脉宽调制) 实现直流到交流变换，运行于 FreeRTOS。

> 注意：`README.md` 描述的是早期**单相 / 400 点正弦表**方案，当前代码已演进为**三相 / 600 点正弦表** (`TABLE_SIZE = 600`，B/C 相分别相移 200/400 点)。以代码为准。

## 构建 / 烧录

工具链为 **arm-none-eabi-gcc**（须在 PATH 中），生成器为 **Ninja**，通过 CMake Presets 管理：

```bash
cmake --preset Debug          # 配置 (输出到 build/Debug/)
cmake --build build/Debug     # 编译，产出 build/Debug/G474_1.elf

cmake --preset Release        # -Os 优化版本，输出到 build/Release/
cmake --build build/Release
```

- 工具链文件：`cmake/gcc-arm-none-eabi.cmake`（FPU=fpv4-sp-d16, hard float, `--specs=nano.specs`）。
- 链接脚本：`STM32G474XX_FLASH.ld`。链接时开启 `-u _printf_float -u _scanf_float`（`printf`/`sscanf` 需浮点支持）。
- 编译后自动打印内存占用（`-Wl,--print-memory-usage`）。
- 无测试框架；这是裸机/RTOS 固件工程，验证靠硬件在环 + 串口日志 + GPIO 时序观测。
- 烧录/调试在 CLion 中通过 `.idea/` 运行配置完成（无命令行 openocd 脚本）。

## CubeMX 代码生成的约束

`Core/`、`Drivers/`、`Middlewares/` 及根目录 `*.ioc`、`startup_*.s`、`.ld` 由 **STM32CubeMX** 生成。修改 `G474_1.ioc` 后重新生成会覆盖 `Core/Src/*.c`。

- **只在 `/* USER CODE BEGIN */ ... /* USER CODE END */` 之间写代码**，否则会被覆盖。
- 应用逻辑应放在 `User/` 目录下的自定义模块，而非 `Core/`。
- 新增的 CubeMX 源文件须手动加入 `cmake/stm32cubemx/CMakeLists.txt` 的 `MX_Application_Src`。

## 架构

### 启动流程
`Core/Src/app_freertos.c` 的 `StartDefaultTask()` → `app_main()` (`User/main/main.c`)。`app_main()` 依次初始化各模块并创建 FreeRTOS 任务，然后启动 HRTIM 波形输出，最后启动 Master 定时器的重复中断。

### 控制回路（核心）
```
DC输入 → 全桥逆变 (HRTIM Timer A/B/C, SPWM) → 三相AC输出
             ↑                                      ↓
    600点正弦表查表 (Master重复中断)      ADC1/2/3 DMA双缓冲采样
             ↑                                      ↓
    调制比 factor ←── 电压PID (增量/位置式) ←── 方差法RMS + 卡尔曼滤波
```

1. **SPWM 生成** (`pid_ctrl_main.c: HAL_HRTIM_RepetitionEventCallback`)：每个 PWM 周期在 Master 重复中断中，从预计算的 `a/b/c_sine_table` 查表，直接写 HRTIM 的 `CMP1xR`/`CMP3xR` 寄存器（互补输出）。中断内不做浮点运算——查表值由 `set_mod_ratio_by_factor()` 用 CMSIS-DSP (`arm_scale_f32`/`arm_offset_f32`) 预先算好。

2. **ADC 采样** (`ADC/ADC.c`)：ADC1+ADC2 双同步模式 DMA（32 位打包：低 16 位=ADC1，高 16 位=ADC2）+ ADC3 独立 DMA。半满/全满中断把当前半缓冲区指针 (`adc_dma_block_t`) 通过 `adc_queue` 送给 PID 任务，构成 ping-pong 双缓冲。

3. **PID 任务** (`pid_ctrl_main.c: PID_ctrl_routine`)：阻塞等待 `adc_queue` → 检查 `pid_ctrl_queue_mV` 目标电压是否更新 → `adc_data_process()` 用**方差法** (`Var = SumSq/N - Mean²`) 直接算三相 RMS（避免逐点浮点、无直流分量影响）→ 卡尔曼滤波平滑 → 三路 `pid_compute()` 输出调制比 → `set_mod_ratio_by_factor()` 刷新正弦表。

### 关键时序常量（`Core/Inc/main.h`）
- `PWM_Period = 44800`（HRTIM 周期计数）
- `ADC_BUFFER_LENGTH = 400`；PID 调控频率 = PWM频率 / ADC分频 / 缓冲区半长 ≈ 50Hz（即输出交流基波频率）

### 模块划分（`User/`）
- `main/` — 入口 (`main.c` 的 `app_main`)、GPIO/IO、字库
- `components/ADC/` — ADC DMA 采集 + 中断回调（数据的 RMS 处理逻辑实际在 `pid_ctrl_main.c`）
- `components/pid_ctrl/` — SPWM 控制主逻辑 (`pid_ctrl_main.c`)、PID 算法 (`pid_ctrl.c`)、串口调参 (`console.c`)。`PID.h` 定义 ADC 缓冲/DMA 块类型，`pid_ctrl_internal.h` 定义 PID 句柄 API
- `components/filter/` — 一维卡尔曼滤波 (`kalman.c`)、RC 滤波
- `components/display/` — SH1107/SSD1306 OLED 驱动 + LVGL 移植层 (`lvgl_port/`)，通过 SPI
- `components/UI/` — LVGL 界面与动画
- `components/LOG/` — ESP-IDF 风格分级彩色日志宏 (`LOGI/LOGW/LOGE/...`)

各 component 由 `User/components/CMakeLists.txt` 用 `GLOB_RECURSE` 收集为 INTERFACE 库 `user_components`；**新增子目录须同时更新该文件的 glob 列表与 include 路径**。

### 串口调参 (`console.c`)
USART3 通过 DMA 空闲中断接收，格式为空格分隔的三个浮点数 `"kp ki kd"`，`sscanf` 解析后调用 `pid_set_param()` 同时更新三相 PID 参数。

## 约定

- 模块内部头文件命名为 `*_internal.h`（如 `pid_ctrl_internal.h`），对外接口在 `PID.h` / `display.h` 等。
- 中断上下文中用 `...FromISR` 版 FreeRTOS API 并配合 `portYIELD_FROM_ISR`。
- 日志用 `LOG.h` 的宏（带 TAG 与级别），不要直接 `printf`。
- 浮点/DSP 运算优先用 CMSIS-DSP (`arm_math.h`)；中断内避免浮点，改用预计算查表。
- GPIO PC0=LED 心跳，PC1 用于翻转观测 ISR/任务时序。
