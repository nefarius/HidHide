# Build and release (maintainers)

The root `README.md` is reserved for end-user documentation. Maintainer notes for the BthPS3-style pipeline live here.

## Flow

- **CI (AppVeyor)** produces **unsigned** artifacts (x64 + ARM64): `HidHide_x64.msi`, `HidHide_ARM64.msi`, and driver attestation `.cab` files.
- **Local** maintainer machine performs **code signing** with `signtool` (certificate in local cert store) and publishes the release.

## CI build entrypoint

```powershell
.\build.ps1 Ci --configuration Release --platform x64
.\build.ps1 Ci --configuration Release --platform ARM64
```

## Installer payload

The MSI builder consumes a flat per-arch staging directory; see [INSTALL_LAYOUT.md](INSTALL_LAYOUT.md).

## Local signing

Download CI artifacts for a tag/version and sign MSI/CABs (AppVeyor API token required):

```powershell
.\release.ps1 -BuildVersion v1.2.3 -Token <appveyor_token>
```

Default certificate subject is `Nefarius Software Solutions` (override with `-CertName` if needed).
