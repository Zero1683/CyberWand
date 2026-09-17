# 开发、烧录与恢复

以下命令都在仓库根目录执行。Windows 虚拟环境解释器为 `.venv\Scripts\python.exe`，macOS/Linux 替换为 `.venv/bin/python`。

## 安装与构建

```powershell
python setup.py --dev
.venv\Scripts\python.exe tools\build.py
```

构建环境：PlatformIO 6.1.19，espressif32 7.0.1，Arduino ESP32 2.0.17，ESP32-C3，4 MB Flash，DIO / 40 MHz，min_spiffs 分区表。PlatformIO 数据和工具链保存在本目录 `.cache/platformio`，首次构建需要联网下载。构建输出位于 `firmware/.pio/build/mozhang`。

## 烧录已有板

最方便的是网页“烧录固件”：释放串口、烧录应用、校验并重连。优先用本目录刚构建的应用，否则使用 `release/firmware.bin`。只写 0x10000，不清空模板与配置。

也可先断开网页串口，用命令行：

```powershell
.venv\Scripts\python.exe tools\flash.py --list
.venv\Scripts\python.exe tools\flash.py --port COM30
```

COM30 是示例，换成实际端口。必须继续给 J2 供电。若无法自动进下载模式：按住 BOOT，按一下 RESET，松 RESET，再松 BOOT，重新选择枚举后的原生 USB 端口。不要改成 FT232 的端口。

## 新焊板首次完整安装

```powershell
.venv\Scripts\python.exe tools\flash.py --port COM30 --full
```

写入同一发行组：bootloader 0x0，partitions 0x8000，boot_app0 0xe000，app 0x10000。这个选项用于本硬件与本分区布局的新板；已有当前系统的板子用默认应用更新即可。它不是整片擦除，也不导入出厂个人模板。

## 恢复

```powershell
.venv\Scripts\python.exe tools\flash.py --port COM30 --recovery
```

恢复镜像为 v0.6，仅写应用区；不支持 v0.7 的新增自定义流程与无线功能。新模板不会被主动清空，重新刷回 v0.7 后再核对。`release/manifest.json` 提供原始二进制大小和 SHA-256。

## 测试

```powershell
.venv\Scripts\python.exe firmware\test\gui_test.py
node firmware\test\presentation_test.js
```

四个 C++ 测试可用支持 C++17 的本机编译器分别编译运行，例如 `g++ -std=c++17 firmware/test/recognizer_test.cpp -o recognizer-test`。这些测试检查算法边界与软件状态，不代替真实动作和硬件测试。

发布时保留应用源码、构建配置、第三方许可及来源信息。`release/relink/main.cpp.o` 是本次应用对象文件，便于与匹配的框架重新链接；源代码也完整提供。依赖框架的许可证不被本项目 MIT 覆盖。
