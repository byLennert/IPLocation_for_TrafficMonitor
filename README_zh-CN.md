# IPLocationPlugin（TrafficMonitor 插件）

[English](README.md)

## 效果预览

![效果预览](https://picgo-lenblog.oss-cn-beijing.aliyuncs.com/lenblog/202603161907585.png)

TrafficMonitor 是一个极其轻量的小工具，非常适合在工作中快速核对当前网络状态。
本插件只做一件事：

- 在任务栏窗口/悬浮窗中显示**当前公网出口 IP**（常用于核对代理/VPN 节点是否切换成功）
- 鼠标悬停时显示 tooltip，便于快速确认与复制

目标很明确：切换节点后，你应当能在几十秒内确认当前出口 IP。

本插件基于作者提供的插件接口文档开发。虽然也尝试过其它 IP 插件，但无法满足“代理出口实时核对”的使用场景，因此实现了这个版本。

- 数据源优先级：
  - `ipwho.is`（返回 IP + 地区；支持 `lang=zh-CN`）
  - 兜底：`api.ipify.org`（仅 IP） + `ip-api.com`（国家/地区）

## 文件结构

- `PluginInterface.h`：TrafficMonitor 官方插件接口头文件（来自 TrafficMonitorPlugins 仓库）
- `IPLocationPlugin.*`：插件主体实现（实现 `ITMPlugin`）
- `IPLocationItem.*`：显示项实现（实现 `IPluginItem`）
- `NetworkHelper.*`：基于 WinHTTP 的 HTTP GET 封装
- `HtmlParser.*`：遗留 HTML 解析（保留兼容；目前不是主链路）
- `CMakeLists.txt`：CMake 构建脚本

## 兼容性

- TrafficMonitor：已在 `v1.85.1`（x64）验证
- Windows：Windows 10/11
- 位数：插件 DLL 必须与 TrafficMonitor 位数一致（x64/x86）

## 构建（Windows）

### 环境要求

- CMake
- MSVC 编译工具链（Visual Studio / Build Tools）

说明：

- 使用 MSVC 时建议在开发者终端中构建（或先运行 `VsDevCmd.bat`），确保 `cl.exe` / `nmake.exe` 可用。
- NMake/Ninja 这类单配置生成器需要通过 `-DCMAKE_BUILD_TYPE=Release` 指定 Release。

### 使用 MSVC + NMake 构建（推荐，适配你目前的方式）

需要在 **Visual Studio 开发者环境**中执行（确保 `cl.exe`、`nmake.exe` 可用），然后运行：

```powershell
cd D:\Project\Test\IP\IPLocationPlugin

# 清理旧的 build 缓存（切换生成器/改动配置时建议执行）
rmdir build -Recurse -Force

cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

通常生成的 DLL 路径：

- `build\IPLocationPlugin.dll`

### 使用 Visual Studio 生成器构建

如果你的 CMake 能检测到 Visual Studio：

```powershell
cd D:\Project\Test\IP\IPLocationPlugin
rmdir build -Recurse -Force

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## 安装

1. 将 `IPLocationPlugin.dll` 复制到 TrafficMonitor 的 `plugins` 目录。
   例如：`D:\Utils\TrafficMonitor\plugins\`
2. 重启 TrafficMonitor。
3. 在以下位置启用/查看插件：
   - 任务栏窗口右键菜单 -> 显示设置
   - 或：右键菜单 -> 其他功能 -> 插件管理

提示：更新 DLL 时建议先彻底退出 TrafficMonitor（确保进程结束）再覆盖文件。

## 当前行为说明

- 自动刷新：**每 30 秒**全量更新一次（IP + 地区）
- 手动刷新：不需要（已禁用点击刷新）
- 鼠标提示：显示 `Public IP` + 合并后的“地区”一行（避免 Country/Region/City 重复）

## 数据准确性说明

代理/机房 IP 的地理库数据常会出现差异（不同供应商、前缀/ASN 迁移、标注策略不同）。
本插件优先使用 `ipwho.is`，主要原因是接口稳定、可直接返回 JSON。

## 常见问题

- 如果复制 DLL 后 TrafficMonitor 启动就崩溃，通常是插件接口 ABI 不匹配。
  请确认使用的 `PluginInterface.h` 与 TrafficMonitor 版本匹配，且 `plugins` 目录里没有旧版本同名 DLL。
- 如果地区信息看起来“和别的软件不一致”，这是常见现象：代理/机房 IP 在不同库中的定位可能差异较大。

## 许可证

MIT License，详见 `LICENSE`。

## 致谢

- TrafficMonitor 项目提供插件系统与 `PluginInterface.h`。

## 联系方式

- 邮箱：1416679017@qq.com
