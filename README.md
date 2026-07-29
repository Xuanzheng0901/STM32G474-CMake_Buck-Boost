# dcac-trip — 三相三线 DC-AC 逆变器

基于 STM32G474VET6 和 HRTIM 的三相逆变器，使用中心对齐 SPWM 输出相差严格 120° 的三相电压。当前闭环目标为三相线电压有效值，题目工况为：

- 线电压：32 V RMS
- 相电流：2 A RMS
- 输出频率：30 Hz / 60 Hz 可切换

## 控制架构

```text
线电压目标
    │
    ▼
公共线电压 PID ──→ 公共调制度 m ──→ A/B/C 三相 SPWM
    ▲                                      │
    │                                      ▼
(Vab + Vbc + Vca) / 3              三相三线逆变输出
    ▲                                      │
    └──── Vab/Vbc/Vca RMS ← ADC DMA采样 ────┘
```

### SPWM

- A、B、C 三个桥臂分别由 HRTIM Timer A/B/C 驱动。
- 使用 600 点正弦表，B、C 相分别偏移 200、400 点，对应 120°、240°。
- 三相使用同一个调制度，避免三个独立线电压 PID 互相耦合。
- 调制度限制为 `0.0 ~ 0.98`。

### 线电压闭环

ADC 电压通道依次定义为：

| 通道 | 测量量 |
|---|---|
| ADC1 Rank 1 | Vab |
| ADC2 Rank 1 | Vbc |
| ADC3 Rank 1 | Vca |

控制器使用三路线电压 RMS 的平均值作为反馈：

```c
Vll_feedback = (Vab_rms + Vbc_rms + Vca_rms) / 3;
error = Vll_target - Vll_feedback;
```

PID 输出同时作用于三个桥臂，以保持三相幅值一致和固定的 120° 相位关系。

> 三相三线系统只有两个独立线电压自由度。当前公共电压环保证平均线电压，不会独立修正 Vab、Vbc、Vca 的不平衡。若需要在明显不平衡负载下分别稳压，应改用 αβ 坐标系下的双 PR 控制器。

## ADC 与有效值计算

- ADC1/ADC2 使用双同步模式，ADC3 独立采样。
- DMA 每个采样组包含三路线电压和三相电流。
- 使用方差法去除 ADC 偏置并计算 RMS：

```text
RMS = sqrt(SumSq / N - Mean²)
```

- RMS 结果经过一维卡尔曼滤波后进入控制环和 UI。
- 线电压校准系数按每个通道分别标定：

```text
新系数 = 28.5 × 示波器实测线电压 / MCU显示线电压
```

线电压采样必须经过差分放大、隔离或其他合适的信号调理，不能把开关节点直接连接到单端 ADC。

## 频率与时序

系统时钟为 162 MHz，`PWM_Period = 36000`。

| 输出频率 | HRTIM 倍频配置 | PWM 频率 | 每周期点数 |
|---|---:|---:|---:|
| 30 Hz | MUL4 | 18 kHz | 600 |
| 60 Hz | MUL8 | 36 kHz | 600 |

切换频率时，Master 与 Timer A/B/C 同时停止、修改预分频并重新启动，三相仍由 Master 周期同步复位。

## UI

LVGL 主界面显示：

- `AB / BC / CA` 三路线电压
- `Ia / Ib / Ic` 三相电流
- 线电压设定值，范围 `0.00 ~ 33.00 V`
- 30 Hz / 60 Hz 切换
- 平衡三相假设下的视在功率估算

视在功率显示采用：

```text
S ≈ √3 × Vline_avg × Iline_avg
```

默认 UI 字体为 Fusion Pixel 12 px。生成参数记录在
[`User/main/fusion_pixel_12.c`](User/main/fusion_pixel_12.c) 第 4 行；修改 UI 文本后，应将新增字符加入 `--symbols` 并重新运行该行的 `lv_font_conv` 命令。
页脚“三相逆变器”标题单独使用 ChillBitmap 字体，不属于 Fusion Pixel 字符集。

## 32 V / 2 A 星形电阻负载

平衡星形负载中：

```text
Vphase = 32 / √3 ≈ 18.48 V
Rphase = 18.48 / 2 ≈ 9.24 Ω
Pphase = 2² × 9.24 ≈ 36.95 W
Ptotal ≈ 110.9 W
```

建议三个电阻均选约 `9.2 Ω`，单只额定功率不低于 `50 W`，并尽量保证阻值和温漂一致。

## 硬件平台

| 项目 | 配置 |
|---|---|
| MCU | STM32G474VET6，Cortex-M4，162 MHz |
| PWM | HRTIM Master + Timer A/B/C |
| 调制 | 中心对齐三相 SPWM，600 点正弦表 |
| ADC | ADC1/ADC2 双同步 DMA + ADC3 DMA |
| 显示 | LVGL + OLED |
| 调试 | 日志输出及 GPIO 时序观测 |

## 项目结构

```text
User/
├── components/
│   ├── ADC/          # ADC DMA采样与队列
│   ├── display/      # OLED驱动与LVGL移植
│   ├── filter/       # 卡尔曼滤波
│   ├── LOG/          # 日志
│   ├── pid_ctrl/     # RMS处理、公共线压PID与SPWM
│   └── UI/           # 三相三线界面
└── main/
    ├── main.c
    ├── fusion_pixel_12.c
    └── chillbit.c
```

## 构建

```bash
cmake --preset Debug
cmake --build --preset Debug --target G474_1
```
