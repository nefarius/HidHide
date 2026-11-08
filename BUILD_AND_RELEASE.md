# Build and release (maintainers)

The root `README.md` is reserved for end-user documentation. Maintainer build and release notes live here.

## Versioning

Release tags use three-part SemVer, for example `v1.8.0`. GitHub Actions stamps binaries as `MAJOR.MINOR.PATCH.<run number>`, so `v1.8.0` on run 42 produces file version `1.8.0.42`. The fourth component is the monotonic build counter. Windows Installer uses the first three components, which keeps MSI upgrades aligned with the SemVer tag.

Builds that are not release tags receive `0.0.0.<run number>`.

## CI

GitHub Actions builds x64 and ARM64. Each platform job restores the portable Windows SDK/WDK, builds the solution, and packages:

- `artifacts/msi/<platform>/HidHide_<platform>.msi`
- `disk1/HidHide_<platform>.cab`
- PDBs and `HidHide*.exe`

The x64 job also runs `HidHide.Tests.exe`. ARM64 tests are compiled but not executed because the runner is x64. CI test-signs the driver staged into the MSI with an ephemeral certificate so the package can be validated. Those MSI files are CI artifacts, not the production release.

## Local commands

`./build.ps1` bootstraps .NET from `global.json` when required and runs the Fallout build. `./build.sh` is the same entry point for the Linux version job. `build.cmd` selects the appropriate script.

```powershell
./build.ps1 --help
./build.ps1 TestVersion
./build.ps1 Compile --configuration Debug --platform x64
./build.ps1 Test --configuration Release --platform x64
./build.ps1 Ci --configuration Release --platform x64
./build.ps1 Ci --configuration Release --platform ARM64
```

`Ci` restores, rebuilds, tests when the platform can execute the tests, and creates both the MSI and attestation CAB. CAB packaging requires `--configuration Release` because the DDF files read `bin\Release`.

WiX 5.0.2 and its UI extension are required before `PackageMsi` or `Ci`:

```powershell
dotnet tool install --global wix --version 5.0.2
wix extension add -g WixToolset.UI.wixext/5.0.2
```

## Local signing and publication

Production signing stays on a maintainer machine. Download the successful tagged Actions artifacts, then sign the MSI and CAB files with the certificate in the local certificate store. The default subject is `Nefarius Software Solutions`.

```powershell
./build.ps1 Release --release-tag v1.8.0
./build.ps1 Release --release-tag v1.8.0 --no-signing
./build.ps1 Release --release-tag v1.8.0 --release-output artifacts/release --certificate-subject "Nefarius Software Solutions"
```

The command requires `GH_TOKEN` or `GITHUB_TOKEN`, or `--github-token`. It accepts only the successful Build run whose commit matches the tag and refuses to clean a directory outside the repository. Attach the signed MSI and CAB files to the GitHub release manually after smoke testing. The Winget workflow publishes MSI assets after that release is published.
