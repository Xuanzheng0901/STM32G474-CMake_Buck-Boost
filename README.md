# STM32G474 数字电源平台

本仓库以 **STM32G474VET6** 为控制核心，集中维护 Buck-Boost、DAB、MPPT、PFC 和 DC-AC 逆变器等数字电源项目。各项目以 Git 分支隔离：`master` 保存 Buck-Boost CV/CC 基础平台，其余分支对应不同拓扑或测评固件。

> 各分支的硬件配置、控制参数和外设用途并不完全相同。开始开发或下载固件前，请先确认当前分支和实际功率板，不要跨分支直接套用 PWM、ADC 标定或保护参数。

## 分支导航

| 分支 | 功能与控制方式 | 界面 | 当前定位 |
|---|---|---|---|
| [`master`](../../tree/master) | Buck-Boost，电压外环与电流内环组成 CV/CC 双环，HRTIM Timer E 输出 PWM | LVGL + OLED | 通用数字电源基础平台；电流设定与显示统一使用 mA |
| [`charge`](../../tree/charge) | Buck-Boost CV/CC 双环充电控制 | LVGL + OLED | `master` 的早期充电功能快照，保留用于历史对照 |
| [`DAB`](../../tree/DAB) | 串联谐振双有源桥，三移相（TPS）控制；支持双向功率传输、调频和恒流闭环 | LVGL + SH1107 | TPS 主实现，面向全范围软开关和环流优化 |
| [`DAB-SPS`](../../tree/DAB-SPS) | 串联谐振双有源桥，单移相（SPS）控制；固定 50% 占空比，以桥间移相角控制功率 | LVGL + SH1107 | SPS 试验分支，含 1→2、2→1、AUTO 和预充电流程；最新提交含临时试验内容 |
| [`MPPT`](../../tree/MPPT) | Buck-Boost 光伏充电，扰动观察法（P&O）跟踪最大功率点 | LVGL + SSD1306 | MPPT 算法与四路电压/电流采样验证 |
| [`PFC`](../../tree/PFC) | 单相 Boost PFC，平均电流模式；电压外环产生电流幅值，PWM 周期内执行电流内环 | LVGL + OLED | 传统整流桥 Boost PFC 实现 |
| [`dcac`](../../tree/dcac) | 单相全桥 DC-AC，400 点 SPWM 与交流 RMS 采样 | LVGL + OLED | 当前为开环测试：PID 计算已注释，固定调制度为 `0.95` |
| [`dcac-trip`](../../tree/dcac-trip) | 三相三线 DC-AC，600 点三相 SPWM，公共线电压闭环，30/60 Hz 可切换 | LVGL + OLED | 三相逆变器主分支，目标线电压 32 V RMS、相电流 2 A RMS |
| [`APFC`](../../tree/APFC) | 低压图腾柱无桥 Boost PFC，SOGI-PLL + 电流内环 + 电压外环，含软启动与故障状态机 | LVGL + SH1107 | 带本地 UI 的调试版本；当前额定输出配置为 65 V DC |
| [`APFC-Release`](../../tree/APFC-Release) | 与 `APFC` 相同的图腾柱控制主线，保留 UART3 调参与 PD9 PowerGood | 无界面 | 无 LVGL/OLED 的轻量测评固件，侧重控制性能与资源占用 |

## 主要控制架构

### Buck-Boost 与 MPPT

```text
ADC DMA 采样 → 滤波/功率计算 → CV/CC 双环或 P&O 算法 → HRTIM PWM
```

- `master` / `charge`：电压环和电流环输出取较小值，实现恒压、恒流自动切换。
- `MPPT`：根据光伏输入功率变化扰动占空比，并设置功率变化死区抑制噪声误判。

### DAB

```text
电压、电流采样 → 工作模式与预充电状态机 → 电流 PID
            → 开关频率/移相角计算 → HRTIM 多路同步更新
```

- `DAB-SPS` 仅调节两桥之间的移相角。
- `DAB` 进一步调节原边内移相、次边内移相和桥间移相三个自由度。

### PFC

```text
交流电压/电流高速采样 → 电流内环 → HRTIM 占空比
直流输出低速采样     → 电压外环 → 电流幅值指令
```

- `PFC` 用于传统整流桥后的 Boost PFC。
- `APFC` / `APFC-Release` 用于低压图腾柱无桥 PFC，并增加 SOGI-PLL、过零极性切换、软启动和故障保护。

### DC-AC

```text
正弦表/相位累加 → HRTIM SPWM → 逆变输出 → ADC DMA → RMS → 电压控制
```

- `dcac` 为单相全桥，目前处于固定调制度的开环验证阶段。
- `dcac-trip` 为三相三线逆变器，三相共用一个线电压控制量，以保持相差 120° 的对称 SPWM。

## 公共软硬件

- MCU：STM32G474VET6，Cortex-M4F，最高 170 MHz
- PWM：HRTIM 高分辨率定时器；不同分支使用的 Timer、频率和输出通道不同
- 采样：ADC1～ADC4、DMA 双缓冲，以及按项目配置的差分/单端输入和硬件触发
- 实时系统：FreeRTOS
- 算法：增量式/位置式 PID、卡尔曼或 RC 滤波；部分分支使用 CMSIS-DSP
- 人机交互：LVGL、SH1107/SSD1306 OLED、旋转编码器
- 调试：UART3 日志/控制台和 GPIO 时序观测

典型目录如下；实际模块以当前分支为准：

```text
Core/                       # STM32CubeMX 生成的外设初始化与中断代码
Drivers/                    # STM32 HAL、CMSIS
Middlewares/                # FreeRTOS、CMSIS-DSP 等中间件
User/
├── main/                   # 应用入口、stdio 重定向、字体资源
└── components/
    ├── ADC/                # ADC、DMA 与采样数据分发
    ├── filter/             # 卡尔曼/RC 滤波
    ├── pid_ctrl/           # PID 与项目控制逻辑
    ├── LOG/                # 日志
    ├── display/ 和 UI/     # OLED、LVGL 与交互界面
    └── 项目专用模块        # MPPT、PFC、PLL、ctrl_loop 等
```

## 获取与构建

需要 Arm GNU Toolchain、CMake 和 Ninja。除 `APFC-Release` 外，各分支都记录了 LVGL 子模块。

```bash
git clone --recurse-submodules <repository-url>
cd G474_1
git switch <branch>
git submodule update --init --recursive

cmake --preset Debug
cmake --build --preset Debug --target G474_1
```

Release 构建：

```bash
cmake --preset Release
cmake --build --preset Release --target G474_1
```

切换分支后应重新执行对应 preset 的配置命令，避免构建目录继续引用上一分支已删除或新增的源文件。`APFC-Release` 不需要初始化 LVGL 子模块。

## 使用注意事项

- 本仓库包含真实功率级控制代码。首次上电应使用限流电源、隔离测量和低占空比开环测试，确认互补驱动、死区、极性、采样标定和保护逻辑后再进入闭环。
- 各分支的 ADC 标定、母线目标、电流限制、PWM 频率和拓扑定义不可直接互换。
- 编译成功只说明代码和链接配置可用，不代表功率级波形、环路稳定性或保护动作已经通过硬件验证。
- LVGL 是 Git 子模块。删除其工作目录会影响整个工作树，切换到需要 UI 的分支后须重新初始化子模块。
