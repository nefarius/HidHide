# Install layout (canonical)

HidHide installers must deploy user-visible binaries and ETW manifests under a **single architecture-neutral root** so ETW `resourceFileName` / `messageFileName` stay identical for x64 and ARM64 builds.

## Rules

- **Root directory:** `%ProgramFiles%\Nefarius Software Solutions\HidHide\`
- **Do not** add `x64`, `ARM64`, or similar architecture segments under that root.
- **Do not** use the legal suffix `e.U.` in **any filesystem path** (trailing-dot folder names break Windows and MSI tooling). Use **`Nefarius Software Solutions`** for on-disk folders; keep full legal names only in MSI metadata (Manufacturer, ARP), not directory names.

## Typical payload (same relative paths per platform)

| Relative path | Purpose |
|---------------|---------|
| `HidHide.sys`, `HidHide.inf`, `HidHide.cat` | Driver package |
| `nefconw.exe` | [nefcon](https://github.com/nefarius/nefcon) windowless build — driver install/remove and class filters ([releases](https://github.com/nefarius/nefcon/releases); use **v1.17.40+** for `install --remove-duplicates`) |
| `HidHide.man`, `HidHide.wprp` | Kernel ETW |
| `HidHideClient.exe`, `HidHideClient.man`, `HidHideClient.wprp` | Configuration client |
| `HidHideCLI.exe`, `HidHideCLI.man`, `HidHideCLI.wprp` | CLI |

The kernel driver continues to load from `%WinDir%\System32\drivers\` after installation (see driver setup actions); ETW manifests for the driver still reference `HidHide.sys` there.

## ETW message DLL paths

User-mode ETW providers in source manifests use:

- `%ProgramFiles%\Nefarius Software Solutions\HidHide\HidHideClient.exe`
- `%ProgramFiles%\Nefarius Software Solutions\HidHide\HidHideCLI.exe`

These must match the deployed EXE locations above.

## Building the MSI (WixSharp, OSS)

This repo uses the same stack as [WinDbgSymbolsCachingProxy](https://github.com/nefarius/WinDbgSymbolsCachingProxy): **`WixSharp_wix4`** (NuGet) driving the **WiX 5** toolchain.

### Prerequisites (Windows)

Install the **WiX 5.0.2** .NET global tool and the matching UI extension. **Do not** default to WiX 6 without validating WixSharp + `WixUI_*`: newer `wix` defaults are not compatible with the WixSharp + `WixUI_FeatureTree` combination without version tweaks (same pitfall as documented in WinDbgSymbolsCachingProxy).

```powershell
dotnet tool install --global wix --version 5.0.2
wix extension add -g WixToolset.UI.wixext/5.0.2
```

### Staging directory

Collect all payload files into one **flat folder** (same relative names as in the table above). Paths can come from a Release build, e.g. copy driver artifacts, **`nefconw.exe`** from a [nefcon release](https://github.com/nefarius/nefcon/releases), and built `.exe` / `.man` / `.wprp` files into `.\staging`.

### Build

```powershell
cd Installer
dotnet run -c Release -- --staging "C:\path\to\staging" --out "C:\path\to\msi-out" --platform x64
```

Optional environment variables: `HIDHIDE_INSTALLER_STAGING`, `HIDHIDE_INSTALLER_OUT`.

The generated `HidHide.msi` installs to `%ProgramFiles%\Nefarius Software Solutions\HidHide\` and runs **nefconw** + `wevtutil` during install/uninstall (see `Installer/Program.cs`).
