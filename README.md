# 图腾柱无桥 PFC 数字控制器

基于 STM32G474RET6 的**图腾柱无桥 Boost PFC (功率因数校正)** 控制器，采用 SOGI-PLL 锁相 + 双环 PI 控制。

## 控制架构

```
                            ┌─────────────────────────────────────┐
                            │             控制回路                  │
                            │                                     │
    AC 输入 ─→ EMI ─→ 电感 ─→ 图腾柱快管 (TA1/TA2) ─→ DC 输出     │
                  │          │      ↑   ↑                         │
                  │          │   HRTIM  慢管 (TB1/TB2)             │
                  │          │                                     │
                  ├──────────┤── ADC1 (Vac) ─┐                    │
                  │          │── ADC2 (Iac) ─┤ 30kHz ISR          │
                  │          │                ├→ SOGI-PLL → θ     │
                  │          │                ├→ 电流内环 PI       │
                  │          │                └→ PWM 占空比更新     │
                  │          │                                     │
                  └──────────┤── ADC3 (Vdc) ─┐                    │
                             └── ADC4 (Idc) ─┤ ~100Hz 任务         │
                                             ├→ 电压外环 PI        │
                                             └→ I_amplitude 更新   │
                            └─────────────────────────────────────┘
```

### 两条控制通路

| 通路 | 触发源 | 频率 | 上下文 | 职责 |
|------|--------|------|--------|------|
| **电流内环 (快)** | ADC12 半/全完成中断 | ~30kHz | ISR | SOGI-PLL 锁相 → 极性检测 → 电流 PI → PWM 占空比写入 HRTIM |
| **电压外环 (慢)** | ADC34 → Queue → FreeRTOS 任务 | ~100Hz | 任务 | Vout/Iout 平均 → 状态机 (软启动/故障) → 电压 PI → 电流参考幅值 |

### 数据流

```
ADC1 (Vac) ─┐
            ├→ 32bit DMA buf[2] → 半/全完成 ISR → ctrl_loop_ac_isr()
ADC2 (Iac) ─┘                                        │
                                          ┌──────────┤
                                          │          ├→ SOGI-PLL (锁相, 输出 cosθ/sinθ)
                                          │          ├→ EMA 低通滤波 (α=0.7)
                                          │          ├→ 极性检测: cos(θ+offset) ≥ 0 ?
                                          │          ├→ Iref = I_amplitude × |cosθ|
                                          │          ├→ PI(Iref - Ifb) → 占空比修正
                                          │          └→ pfc_write_duty() → HRTIM CMP1
                                          │
ADC3 (Vdc) ─┐                              │    (I_amplitude 由电压环更新)
            ├→ 32bit DMA buf[40]           │
ADC4 (Idc) ─┘   │                          │
                ├→ 半/全完成 ISR            │
                │   → xQueueSendFromISR()   │
                │                          │
                └→ Queue → ctrl_loop_routine() (任务)
                              │
                              ├→ 窗口平均 Vout, Iout
                              ├→ 状态机: IDLE → SOFT_START → RUNNING
                              │          └→ FAULT (过压/过流/欠压/超时)
                              └→ PI(Vref - Vout) → I_amplitude ─┘
```

## 模块职责

| 模块 | 路径 | 职责 |
|------|------|------|
| **ctrl_loop** | `components/ctrl_loop/` | 控制编排: 电流内环 ISR + 电压外环任务入口 + PI 调谐 API |
| **ctrl_loop_state** | `components/ctrl_loop/` | 状态机: 软启动斜坡 / 运行 / 故障保护 |
| **console** | `components/ctrl_loop/` | 串口 PID 参数在线调谐 (`V/I Kp Ki [Kd]`) |
| **PFC utils** | `components/PFC/` | 理想占空比前馈 / HRTIM 寄存器写入 / 慢管极性控制 |
| **PFC config** | `components/PFC/pfc_config.h` | 硬件参数 / ADC 标定 / PI 默认值 / 保护阈值 |
| **SOGI-PLL** | `components/PLL/` | 单相锁相环: SOGI 正交信号 → Park 变换 → PI → VCO |
| **PID 引擎** | `components/pid_ctrl/` | 增量式/位置式 PID, 支持运行时参数更新 |
| **ADC** | `components/ADC/` | DMA 双缓冲采样 + ISR 回调注册 |
| **LOG** | `components/LOG/` | 异步 DMA 日志 (内存池 + FreeRTOS 队列) |
| **UI** | `components/UI/` | LVGL 界面: 电压/电流/功率显示 + 设定值调节 |
| **display** | `components/display/` | OLED 驱动抽象 (SH1107/SSD1306) + LVGL 移植 |

## 图腾柱工作原理

### 关键变量

| 变量 | 含义 | 更新者 | 消费者 |
|------|------|--------|--------|
| `spll.cosine` | 电网电压归一化波形 (≈sin(ωt)) | SOGI-PLL (ISR) | 电流参考生成, 极性检测 |
| `i_amplitude` | 电流参考峰值 (A) | 电压环任务 | 电流内环 ISR |
| `duty_current` | 当前占空比 (0~1) | 电流内环 ISR | HRTIM 寄存器写入 |
| `now_vout_V` | DC 输出电压 (V) | 电压环任务 | 电流内环前馈, UI 显示 |
| `hw_polarity` | 慢管当前极性 (0/1) | 电流内环 ISR | PWM 极性控制 |

### 控制公式

**电流内环 (30kHz ISR):**

```
Iref    = I_amplitude × |cosθ|              (电流参考: 与电网同相的正弦波)
Ifb     = IL × sign(cosθ)                    (电流反馈: 极性归一化)
Ierr    = Iref - Ifb
D_ideal = 1 - |Vin| / Vout                   (Boost 理想占空比前馈)
Duty    = D_ideal + PI(Ierr)                 (PI 修正 + 前馈)
```

**电压外环 (~100Hz 任务):**

```
Verr    = Vref - Vout                        (Vref 由软启动斜坡产生)
I_amplitude = PI(Verr)                       (电压环输出 = 电流参考峰值)
```

### 极性切换

图腾柱 PFC 在电网正负半周交替使用不同的 MOSFET 作为 Boost 开关:

| 半周 | cosθ | 极性 | Boost 管 | 续流管 | 电流反馈 |
|------|------|------|----------|--------|----------|
| 正 | ≥0 | 0 | TA2 (下管) | TA1 (上管, 体二极管) | IL (直接) |
| 负 | <0 | 1 | TA1 (上管) | TA2 (下管, 体二极管) | -IL (取反) |

慢管 (Timer B) 在过零点附近切换，相位偏移可调 (`ctrl_loop_set_polarity_offset`)。

## 状态机

```
  ┌──────┐  init   ┌─────────────┐  ramp完成   ┌───────────┐
  │ IDLE │ ──────→ │ SOFT_START  │ ─────────→ │  RUNNING  │
  └──────┘         └──────┬──────┘            └─────┬─────┘
                          │                         │
                          │  过压/过流/欠压          │  故障
                          │  超时                    │
                          └────────────┬─────────────┘
                                       ↓
                                 ┌───────────┐
                                 │   FAULT   │ ─→ clear_fault → SOFT_START
                                 └───────────┘
```

- **IDLE**: 初始化前状态
- **SOFT_START**: Vref 从 0 线性斜坡至 Vout_target (默认 1s)
- **RUNNING**: 正常闭环运行
- **FAULT**: 故障锁定，i_amplitude = 0，需手动清除或等待自动重试

### 故障保护

| 保护 | 阈值 | 检测位置 |
|------|------|----------|
| 过压 (OVP) | Vout > Vtarget × 1.2 | `ctrl_loop_state_update()` |
| 过流 (OCP) | Iout > 3.5A | `ctrl_loop_state_update()` |
| 电网丢失 (UVP) | PLL 电压幅值 < 5Vrms × 1.414 | `ctrl_loop_state_update()` |
| 软启动超时 | ticks > 5 × 电压环频率 | `ctrl_loop_state_update()` |
| 预充电保护 | Vout < 3V → duty = 0 (体二极管整流) | `ctrl_loop_current_isr()` |

## 关键配置参数

```c
// pfc_config.h
PFC_PWM_FREQ            20000      // PWM 开关频率 (Hz)
PFC_NOMINAL_VOUT        40.0       // 额定输出电压 (V)
PFC_NOMINAL_VIN_RMS     20.0       // 额定输入电压 RMS (V)
PFC_VOLTAGE_LOOP_FREQ   100.0      // 电压外环频率 (Hz)
PFC_SOFTSTART_SEC       1.0        // 软启动持续时间 (s)

// 电流内环 PI (30kHz)
PFC_I_KP_DEFAULT        0.2
PFC_I_KI_DEFAULT        0.2

// 电压外环 PI (100Hz)
PFC_V_KP_DEFAULT        0.2
PFC_V_KI_DEFAULT        0.8
PFC_V_KD_DEFAULT        0.05
```

## 构建

```bash
cmake --preset Debug
cmake --build build/Debug --target G474_1.elf
```

## 项目结构

```
User/
├── main/                  # 入口 (app_main)
│   ├── main.c             # 初始化编排
│   ├── IO.c               # printf 重定向 (UART3)
│   ├── chillbit.c         # 中文字体 16px
│   └── fusion_pixel_12.c  # 中文字体 12px
│
└── components/
    ├── ctrl_loop/         # 控制回路 (核心)
    │   ├── ctrl_loop.c/h          # 双环编排
    │   ├── ctrl_loop_state.c/h    # 状态机
    │   └── console.c/h            # 串口 PID 调谐
    ├── ADC/               # ADC 硬件驱动 + ISR
    ├── PFC/               # PFC 工具函数 + 硬件配置
    ├── PLL/               # SOGI-PLL 锁相环
    ├── pid_ctrl/          # PID 引擎
    ├── filter/            # 卡尔曼滤波器
    ├── LOG/               # 异步日志库
    ├── UI/                # LVGL 界面
    └── display/           # OLED 驱动 + LVGL 移植
        ├── lvgl_port/     # LVGL 显示/输入移植
        ├── stm_sh1107/    # SH1107 驱动
        └── stm_ssd1306/   # SSD1306 驱动
```

## 硬件平台

- **MCU**: STM32G474RET6 (Cortex-M4F, 170MHz)
- **HRTIM**: Timer A (快管 SPWM) + Timer B (慢管极性) + Master (时基)
- **ADC**: ADC1/2 交流 30kHz + ADC3/4 直流 100Hz, DMA 双缓冲
- **运放**: OPAMP1/3 信号调理
- **DAC**: DAC3 CH1 (cosθ 调试) + CH2 (占空比调试), 条件编译控制
- **通信**: UART3 DMA (日志 + 串口控制台)
- **显示**: SPI1 DMA → SH1107 OLED 128×128 + LVGL
- **输入**: EC11 旋转编码器 (Timer 2 编码器模式)
