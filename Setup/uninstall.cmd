@echo off
@setlocal

set MYDIR=%~dp0
pushd "%MYDIR%"

.\devcon.exe classfilter HIDClass upper !HidHide
.\devcon.exe classfilter XnaComposite upper !HidHide
.\devcon.exe classfilter XboxComposite upper !HidHide

RUNDLL32.EXE SETUPAPI.DLL,InstallHinfSection DefaultUninstall 128 "%MYDIR%HidHide\HidHide_HID.inf"
RUNDLL32.EXE SETUPAPI.DLL,InstallHinfSection DefaultUninstall 128 "%MYDIR%HidHide\HidHide_XNA.inf"
RUNDLL32.EXE SETUPAPI.DLL,InstallHinfSection DefaultUninstall 128 "%MYDIR%HidHide\HidHide_XBOX.inf"

popd
endlocal
