# HidHide Watchdog

Small Windows service checking and rectifying missing device class filter entries.

## How to install

Elevated terminal:

```PowerShell
.\HidHideWatchdog.exe /registerService /displayName "HidHide Watchdog"
```

## 3rd party credits

- [A modern C++ scope guard that is easy to use but hard to misuse](https://github.com/ricab/scope_guard)
- [Fast C++ logging library](https://github.com/gabime/spdlog)
- [POCO C++ Libraries](https://pocoproject.org/)
- [WinReg](https://github.com/GiovanniDicanio/WinReg)
- [nefcon](https://github.com/nefarius/nefcon)
