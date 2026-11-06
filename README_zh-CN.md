# IPLocationPlugin（TrafficMonitor 插件）

[English](README.md)

在 TrafficMonitor 的任务栏窗口或悬浮窗中显示公网 IPv4。鼠标提示显示地区、查询时间和数据源，适合检查 VPN、代理或网络切换后的出口地址。

旧版效果图（新版任务栏只显示 IP，地区移至鼠标提示）：

![旧版效果预览](https://picgo-lenblog.oss-cn-beijing.aliyuncs.com/lenblog/202603161907585.png)

## 下载与兼容性

从 [GitHub Releases](https://github.com/byLennert/IPLocation_for_TrafficMonitor/releases) 下载与 **TrafficMonitor.exe 进程架构**相同的 ZIP：

| TrafficMonitor 版本 | 插件包 | 适用系统 |
| --- | --- | --- |
| x86 / Win32 | `*_x86.zip` | Windows 10/11，包含运行 x86 宿主的 64 位系统 |
| x64 | `*_x64.zip` | Windows 10/11 x64 |
| ARM64EC | `*_arm64ec.zip` | Windows 11 on ARM，使用 ARM64EC 宿主时 |

Windows 版本与 CPU 架构是两个不同条件；插件 DLL 必须匹配 TrafficMonitor 进程。源码曾在 TrafficMonitor v1.85.1 x64 上验证。新增架构会在 GitHub Actions 中编译并校验 DLL 类型，但仍需对应设备上的实际运行测试。Windows 7/8.1 尚未验证，不列为支持平台。

TrafficMonitor 使用微软 VC++ 运行库。若插件管理器提示缺少 `MSVCP140.dll` 或 `VCRUNTIME140.dll`，请安装与宿主架构相同的 [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist)。

## 安装

1. 解压 ZIP，将 `IPLocationPlugin.dll` 放到 `TrafficMonitor.exe` 同级的 `plugins` 目录。
2. 完全退出并重启 TrafficMonitor。
3. 在“其他功能 → 插件管理”确认插件已加载，再从任务栏“显示设置”启用 `IP Location`。

升级 DLL 前先完全退出 TrafficMonitor。已有同名 DLL 时，替换该文件即可。

## 查询行为

- 每轮并发查询 3 个服务，并逐轮轮换：`ipv4.icanhazip.com`、`api4.ipify.org`、`v4.ident.me`、`ipinfo.io/ip`、`api.ip.sb/ip`。只接受合法 IPv4，选取出现次数最多的结果；平票时选本轮靠前的结果。三者都失败时尝试 `ipwho.is`。
- 成功一轮结束后等待 30 秒再查；IP 查询失败后等待 10 秒重试。请求耗时会额外计入两次更新之间的时间。
- 任务栏只显示 IP；获取失败时显示 `Failed`，避免把上一次的地址误认为当前地址。上一次成功的 IP 保留在鼠标提示中供排查。
- 地址获取成功后再查询地区：优先 `ipwho.is`，失败时尝试免费版 `ip-api.com`。后者只提供 HTTP，因此仅作为地区查询的最后兜底。地区数据按 IP 缓存。
- 使用 WinHTTP 和当前用户的手动代理、PAC 或自动检测设置。请求带禁用缓存头和变化的查询参数，降低代理或 CDN 返回旧结果的概率。

代理分流规则可能让不同域名走不同出口，因此数据源的结果仍可能不一致。鼠标提示显示选中数据源和一致票数；本插件无法代表所有应用的网络路径。地区数据库对机房和代理 IP 的标注也可能不同。

## 从源码构建

需要 Windows、CMake 3.21+、Visual Studio 2022 C++ 工具链和对应架构的构建组件。在本仓库根目录执行，例如 x64：

```powershell
cmake -S . -B build-x64 -G "Visual Studio 17 2022" -A x64
cmake --build build-x64 --config Release
```

将 `-A x64` 换成 `-A Win32` 或 `-A ARM64EC`，并使用不同的构建目录 `build-x86` 或 `build-arm64ec`。生成的 DLL 位于对应目录的 `Release` 子目录。`main` 分支与 PR 的 Windows CI 会构建三个架构，并将 DLL 作为测试产物供下载；CI 成功且推送 `v*` 标签时创建三个 ZIP 和 `SHA256SUMS.txt`。

仓库中的 `PluginInterface.h` 保留 API 7 接口，以兼容原已验证的 TrafficMonitor 版本；当前官方加载器不会拒绝 API 7，但与每个新版宿主的实际运行兼容性仍需测试。本插件没有使用 API 8 新增的绘制功能。

## 问题排查

- 插件未加载：核对 DLL 与 TrafficMonitor.exe 的架构、VC++ 运行库以及 `plugins` 目录位置。
- IP 长时间不变：检查代理是否为应用分流、PAC 是否生效，并查看 tooltip 的“更新时间”和“数据源”。
- 地区不准确：IP 定位服务使用不同数据库；地区不影响 IP 的获取或显示。

## 许可与致谢

MIT License，见 [LICENSE](LICENSE)。插件接口来自 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor)。
