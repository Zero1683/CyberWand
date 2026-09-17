# USB、BLE、MQTT 与 Home Assistant

USB 始终保留配置入口，无线只选择 BLE 或 Wi-Fi/MQTT 中的一种。修改无线配置后板子重启，网页尝试重连；若端口改变，请手动重新选择。

## BLE

网页选择 BLE 模式并保存。手机使用 nRF Connect 等 BLE GATT 工具，扫描 `MOZHANG-xxxxxx`，连接后找到以下服务，并订阅事件特征的 Notify。系统蓝牙设置中的“配对”不等于订阅事件。

| 用途 | UUID |
|---|---|
| 服务 | `aa490001-31c7-4e70-b655-12c923ec1a01` |
| 16 字节事件通知 | `aa490002-31c7-4e70-b655-12c923ec1a01` |
| 最近事件 JSON 读取 | `aa490003-31c7-4e70-b655-12c923ec1a01` |

通知是二进制，手机显示一串十六进制是正常的。格式为小端 `<BBIIHHH`：版本、类别编号、序号、启动后毫秒、距离×1000、候选差值×1000、保留值。类别 0–5 对应 left/right/up/down/circle/zigzag，6–11 对应 custom1–custom6。名称不包含在这 16 字节通知中，可读取 JSON 特征或按 ID 自行映射。

电脑接收：先运行 `python setup.py --ble`，然后用本目录虚拟环境的 Python 执行 `integrations/ble_receiver.py`。可在最后加设备广播名称；不指定时选择扫描到的符合服务 UUID 的设备。电脑需要蓝牙适配器和系统权限。

当前 BLE 不是键盘/HID，也不提供通过手机写入配置的特征，没有实现 BLE 安全配对流程。识别成功才发送事件，未知动作不发送。

## Wi-Fi / MQTT

在网页选 Wi-Fi/MQTT，填写 2.4 GHz Wi-Fi SSID/密码、MQTT 主机名或 IP、端口、可选用户名密码、主题，然后保存。主机只填地址，不填 `mqtt://` 或路径。默认主题 `mozhang/gesture`。

密码不会从设备回读；保存 MQTT 配置时密码留空表示提交空密码，不代表自动保留旧密码。服务日志会隐藏提交的配置内容。

成功事件格式：

```json
{"v":1,"device":"MOZHANG-ABCDEF","boot":123456,"seq":1,"uptime_ms":12345,"gesture":"custom1","name":"荧光闪烁","distance":0.42}
```

当前实现为 MQTT/TCP、QoS 0、retain=false，不提供 TLS 配置。用于可信局域网，不应直接把未加密服务暴露到公网。无线离线/队列溢出时允许丢弃；不会在重连后补发陈旧动作。设备不是 MQTT Broker，不包含 NAS 或 Home Assistant 服务本体。

## Home Assistant

在 Home Assistant 中配置 MQTT 集成，确认与板子连接同一个 Broker。先在 MQTT 监听工具订阅配置的主题，确认有识别事件；再把 `home-assistant.example.yaml` 放进单条自动化的 YAML 编辑器，并将 `MOZHANG-REPLACE_ME` 改成真实设备 ID。

示例只创建 Home Assistant 通知，方便先确认链路。之后可把 action 换为所需的灯、场景或脚本。不同自定义动作通过稳定 ID 区分；改显示名称不需要改自动化 ID。

目前无 MQTT Discovery，不会自动生成 Home Assistant 设备；也没有直接把 BLE 通知变成 HA 实体的组件。若选择 BLE 路线，需自行把接收程序输出接入自动化。

**测试状态：手机连接并接收到 BLE 通知已有实测；MQTT 与 Home Assistant 示例尚未经过真实 Broker / NAS 环境端到端验证。**
