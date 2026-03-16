# IPLocationPlugin (TrafficMonitor Plugin)

[中文说明](README_zh-CN.md)

## Preview

![Preview](https://picgo-lenblog.oss-cn-beijing.aliyuncs.com/lenblog/202603161907585.png)


TrafficMonitor is a lightweight Windows tool. This plugin adds one feature that is especially useful if you work with VPN/proxy nodes:

- Show the **current public (egress) IP** on the taskbar window
- Provide a tooltip for quick copy/verification

The goal is simple: when you switch nodes, you should be able to confirm the current egress IP within seconds.

- Data source priority:
  - `ipwho.is` (returns IP + location; supports `lang=zh-CN`)
  - Fallback: `api.ipify.org` (IP only) + `ip-api.com` (country)

This plugin also provides tooltip text via `ITMPlugin::GetTooltipInfo()`.

## Files

- `PluginInterface.h`: Official TrafficMonitor plugin interface header (from TrafficMonitorPlugins repo)
- `IPLocationPlugin.*`: Plugin implementation (`ITMPlugin`)
- `IPLocationItem.*`: Display item implementation (`IPluginItem`)
- `NetworkHelper.*`: WinHTTP GET helper
- `HtmlParser.*`: Legacy HTML parser (kept for compatibility; currently not the primary path)
- `CMakeLists.txt`: Build script

## Compatibility

- TrafficMonitor: tested with `v1.85.1` (x64)
- Windows: Windows 10/11
- Architecture: the plugin DLL must match TrafficMonitor (x64 vs x86)

## Build (Windows)

### Requirements

- CMake
- MSVC toolchain (Visual Studio / Build Tools)

Notes:

- When using MSVC, build in a Developer shell (or run `VsDevCmd.bat`) so `cl.exe` / `nmake.exe` are available.
- For single-config generators (NMake/Ninja), use `-DCMAKE_BUILD_TYPE=Release`.

### Build with MSVC + NMake (recommended for your current setup)

Open the Visual Studio Developer environment (so `cl.exe` and `nmake.exe` are available), then run:

```powershell
cd D:\Project\Test\IP\IPLocationPlugin

# Clean old build cache (recommended when changing generators/options)
rmdir build -Recurse -Force

cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The output DLL is typically:

- `build\IPLocationPlugin.dll`

### Build with Visual Studio generator

If CMake can detect your Visual Studio installation:

```powershell
cd D:\Project\Test\IP\IPLocationPlugin
rmdir build -Recurse -Force

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Install

1. Copy `IPLocationPlugin.dll` into TrafficMonitor's `plugins` folder.
   Example: `D:\Utils\TrafficMonitor\plugins\`
2. Restart TrafficMonitor.
3. Enable the item in:
   - Taskbar window context menu -> Display Settings
   - Or: More Functions -> Plugin Management

Tip: if you update the DLL, fully exit TrafficMonitor (ensure the process ends) before replacing the file.

## Behavior

- Auto refresh: every **30 seconds** (full refresh: IP + location)
- Manual click refresh: not used (disabled)
- Tooltip: shows `Public IP` and one merged `地区` line (avoids duplicated country/region/city)

## Data quality notes

Location databases may disagree for proxy/datacenter IPs. This plugin prioritizes `ipwho.is` because it provides stable JSON access without requiring a browser.

If you see different country/region in another software, it is usually due to:

- Different geolocation database vendors
- IP ownership/route changes (ASN prefix moved)
- Proxy/VPN/datacenter tagging differences

## Troubleshooting

- If TrafficMonitor crashes on startup after copying the DLL, it is almost always an ABI mismatch.
  Ensure you are using the official `PluginInterface.h` that matches your TrafficMonitor version.
- If location looks inconsistent, note that different databases may return different results for proxy/datacenter IPs.

## License

MIT License. See `LICENSE`.

## Credits

- TrafficMonitor plugin system and interface header are provided by the TrafficMonitor project.

## Contact

- Email: 1416679017@qq.com
