# IPLocationPlugin for TrafficMonitor

[简体中文](README_zh-CN.md)

Displays the full public IPv4 address and country in TrafficMonitor's taskbar or floating window, for example `110.185.36.126 (China)`. The tooltip also shows the location, lookup time, and source, making it useful when checking VPN, proxy, or network changes.

Earlier preview:

![Earlier preview](https://picgo-lenblog.oss-cn-beijing.aliyuncs.com/lenblog/202603161907585.png)

## Download and compatibility

Download the ZIP matching the **TrafficMonitor.exe process architecture** from [GitHub Releases](https://github.com/byLennert/IPLocation_for_TrafficMonitor/releases):

| TrafficMonitor build | Plugin package | Operating system |
| --- | --- | --- |
| x86 / Win32 | `*_x86.zip` | Windows 10/11, including a 64-bit OS running the x86 host |
| x64 | `*_x64.zip` | Windows 10/11 x64 |
| ARM64EC | `*_arm64ec.zip` | Windows 11 on ARM with the ARM64EC host |

Use TrafficMonitor **v1.86 or later** for automatic taskbar width updates. It remeasures plugin sample text each second using the taskbar font and DPI; v1.85.1 does not do this, so a longer country name may require a manual layout refresh.

The DLL must match the host process, not merely the operating system architecture. The earlier source was tested with TrafficMonitor v1.85.1 x64. CI builds and checks the DLL architecture for the new variants, but each still needs a runtime test on matching hardware. Windows 7/8.1 have not been verified and are not claimed as supported.

If the plugin manager reports a missing `MSVCP140.dll` or `VCRUNTIME140.dll`, install the [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist) for the host architecture.

## Install

1. Extract the ZIP and put `IPLocationPlugin.dll` in the `plugins` directory beside `TrafficMonitor.exe`.
2. Fully exit and restart TrafficMonitor.
3. Check “More Functions → Plugin Management”, then enable `IP Location` in the taskbar display settings.

Fully exit TrafficMonitor before replacing an existing DLL.

## Lookup behavior

- Each poll queries three services concurrently and rotates through `ipv4.icanhazip.com`, `api4.ipify.org`, `v4.ident.me`, `ipinfo.io/ip`, and `api.ip.sb/ip`. It accepts only valid IPv4 addresses and selects the most frequent response. Ties prefer the first result in that poll. If all three fail, it tries `ipwho.is`.
- It waits 30 seconds after a successful poll or 10 seconds after an IP lookup failure before the next poll. Request time adds to that interval.
- The taskbar displays the complete `IP (country)` value. The width sample follows that value with extra padding so TrafficMonitor v1.86+ can expand it for long country names and the selected font/DPI. A layout update may take up to the next taskbar timer tick (about one second).
- While country lookup is pending or unavailable, it displays `IP (Unknown)`. A failed IP poll displays `Failed` instead of a stale address; the last confirmed address remains in the tooltip for diagnosis.
- Location is looked up after the address is published, using `ipwho.is` first and the HTTP-only free `ip-api.com` endpoint as a location fallback. Successful location data is cached per IP.
- WinHTTP uses the current user's manual proxy, PAC, or auto-detect settings. No-cache headers and a changing query parameter reduce stale proxy or CDN responses.

Proxy routing rules can send different lookup domains through different exits. The tooltip shows the selected source and vote count; the result cannot represent every application's route. Location databases may also disagree on VPN or data-center IPs.

## Build from source

Requires Windows, CMake 3.21+, Visual Studio 2022 C++ tools, and the corresponding architecture components. From this repository root, for x64:

```powershell
cmake -S . -B build-x64 -G "Visual Studio 17 2022" -A x64
cmake --build build-x64 --config Release
ctest --test-dir build-x64 -C Release --output-on-failure
```

Use `-A Win32` with `build-x86` or `-A ARM64EC` with `build-arm64ec` for the other architectures. The DLL is in the selected build directory's `Release` subdirectory. Windows CI builds all three on main and pull requests and uploads the DLLs as test artifacts; a `v*` tag publishes three ZIPs and `SHA256SUMS.txt` after CI succeeds.

CI runs GDI display-width regression tests for x86/x64 with long IPv4 addresses, Chinese and long English country names, three fonts, and 100%/125%/150%/200% scaling. ARM64EC is built and checked but cannot run on an x64 runner; configure it with `-DBUILD_TESTING=OFF` when cross-compiling.

The vendored `PluginInterface.h` remains at API 7 for compatibility with the previously tested TrafficMonitor version. The current official loader does not reject API 7, but runtime compatibility with each newer host build still needs testing. This plugin does not use the API 8 drawing functions.

## Troubleshooting

- Plugin not loaded: check DLL/host architecture, VC++ runtime, and the `plugins` directory.
- Text clipped: use TrafficMonitor v1.86+, confirm the new DLL is loaded, and allow a second for the width to update. Also check secondary-monitor DPI and StartAllBack layouts.
- Address stays unchanged: check application routing and PAC settings, then inspect the tooltip's lookup time and source.
- Location differs from another app: geo databases differ; location failure does not prevent the IP from displaying.

## License and credits

MIT License; see [LICENSE](LICENSE). Plugin interface provided by [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor).
