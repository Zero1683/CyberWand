# Cyber Wand · 赛博魔杖

Cyber Wand 是基于 ESP32-C3 与 LSM6DS3TR-C 的手势识别设备，支持个人手势录入、本地识别、网页配置及无线事件输出。

不需要 NAS 就能使用：给板子供电，通过网页录入自己的动作，随后由板子独立识别，以 LED、USB 串口或蓝牙报告结果。有 Home Assistant 时，可以进一步接入 Wi-Fi / MQTT，把手势绑定到自己的自动化。

**当前版本：v0.7。** 自有应用代码与文档使用 MIT 许可；第三方组件见 [许可说明](THIRD_PARTY_NOTICES.md)。

## 作者说明

我是个蒟蒻作者，板子设计和程序编写全部都是 GPT6 完成的。有什么问题可以向 [SiriStudio_Zero@outlook.com](mailto:SiriStudio_Zero@outlook.com) 反馈，我看到会及时处理，谢谢！

## 从这里开始

1. 先读 [开箱与第一次使用](START_HERE.md)，确认电池接线和供电。
2. 电脑安装 Python 3.10 或更新版本，Windows 安装时勾选 PATH。第一次启动需要联网安装依赖，此后网页工具可离线使用。
3. Windows 双击 `start.cmd`；macOS / Linux 在目录中运行 `sh start.sh`。
4. 使用板子自己的 Type-C 数据口，网页选择 ESP32 原生 USB 串口并连接。原开发电脑叫 COM30，其他电脑端口名会不同，程序自动列出可选端口。
5. 网页选择动作、点击“录下一份”，按提示拿稳、挥动、停稳。每类录三份，至少录两类；然后点击“进入自动识别”。全程不需要按住 SW4。

板子若已装有当前固件，无需先重新烧录。界面平时显示 `--`，识别成功显示动作名称三秒，并保留记录。

## 能做什么

- 六个基础标签：左、右、上、下、画圆、折线；另有六个可自行命名的动作槽。
- 每类最多三份个人模板，断电保存；动作不会因为改名而改变，未训练的类别不会自动识别。
- 网页提供六轴波形、校准、模板计数、一次性自动录入、识别记录与导出、无线配置、应用固件烧录。
- 设备本地完成采样与识别，不需要电脑运行算法；不依赖云端。
- USB 始终可配置；无线选择 BLE 或 Wi-Fi/MQTT，二者不同时开启。

这不是通用的跨人手势模型，也不是位置追踪器。握姿、安装方向、重心或外壳改变后应重新录入。相似动作可能被拒绝，匹配距离不是置信概率。

## 目录

| 目录 | 内容 |
|---|---|
| `firmware/src` | LSM6DS3TR-C 采样、自动分段、DTW 识别、持久化、无线传输 |
| `studio` | 本机 Python 串口服务与网页，不需要 Node 构建 |
| `tools` | 启动、构建、烧录脚本 |
| `release` | 已构建 v0.7 固件、首次烧录组件、v0.6 恢复镜像、哈希清单 |
| `hardware` | 原理图与 PCB 编辑快照、网表、BOM、历史 Gerber |
| `integrations` | BLE 接收程序与 Home Assistant 示例 |
| `docs` | 算法、开发、故障排查、验证范围 |

## 进一步阅读

- [算法设计](docs/ALGORITHM.md)
- [开发、烧录与恢复](docs/DEVELOPMENT.md)
- [蓝牙、MQTT 与 Home Assistant](integrations/README.md)
- [裸板硬件与制造资料](hardware/README.md)
- [已完成的实测及限制](docs/VALIDATION.md)
- [常见问题](docs/TROUBLESHOOTING.md)

运行时的日志、导出数据、虚拟环境、缓存与通信令牌会在本文件夹下生成，已加入 `.gitignore`。本仓库不附带个人 Wi-Fi/MQTT 凭据，也不附带整片 Flash / NVS 转储。手势模板保留在实物板子中；新板需重新录入。
