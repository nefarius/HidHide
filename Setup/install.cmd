@echo off
@setlocal

set MYDIR=%~dp0
pushd "%MYDIR%"

RUNDLL32.EXE SETUPAPI.DLL,InstallHinfSection DefaultInstall 128 "%MYDIR%HidHide\HidHide_HID.inf"
RUNDLL32.EXE SETUPAPI.DLL,InstallHinfSection DefaultInstall 128 "%MYDIR%HidHide\HidHide_XNA.inf"
RUNDLL32.EXE SETUPAPI.DLL,InstallHinfSection DefaultInstall 128 "%MYDIR%HidHide\HidHide_XBOX.inf"

.\devcon.exe classfilter HIDClass upper -HidHide
.\devcon.exe classfilter XnaComposite upper -HidHide
.\devcon.exe classfilter XboxComposite upper -HidHide

popd
endlocal
