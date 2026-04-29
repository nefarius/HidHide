rem DEPRECATED
@echo off
rem (c) Eric Korff de Gidts
rem SPDX-License-Identifier: MIT
rem ReinstallDriver.cmd
rem
rem Command script requiring administrator rights used for testing purposes at target environment
rem Mount the Visual Studio build output directory as network share and execute this script on the target environment
rem In order to install and/or update the Nefarius HidHide filter device driver
rem

rem we have to options and per default for the jenkens test job we will deploy the test environment using the installer
if "_%1" == "_" goto :L1

rem
rem This is the first installation option where we utilize the installer
rem It is typically used for deploying the package in a test environment during production builds
rem
echo Deploy installer
hidhide.exe /install /quiet /log ReinstallDriver.log INSTALLLEVEL=2
goto :L2

rem
rem This is the second installation option where we bypass the installer
rem It is typically used during development on the PC of the developer
rem
:L1
echo Close eventvwr
pause

echo Uninstalling the old driver
".\nefconw.exe" --remove-class-filter --position upper --service-name HidHide --class-guid 05f5cfe2-4733-4950-a6bb-07aad01a3a84
".\nefconw.exe" --remove-class-filter --position upper --service-name HidHide --class-guid d61ca365-5af4-4486-998b-9db4734c6ca3
".\nefconw.exe" --remove-class-filter --position upper --service-name HidHide --class-guid 745a17a0-74d3-11d0-b6fe-00a0c90f57da
".\nefconw.exe" remove "root\HidHide"
del "%windir%\INF\SetupApi.dev.log" > NUL

echo Updating Nefarius HidHide filter device driver
if not exist "C:\Program Files\Nefarius"         mkdir "C:\Program Files\Nefarius"
if not exist "C:\Program Files\Nefarius\HidHide" mkdir "C:\Program Files\Nefarius\HidHide"
copy "HidHide\HidHide.cat"   "C:\Program Files\Nefarius\HidHide" /Y > NUL
copy "HidHide\HidHide.sys"   "C:\Program Files\Nefarius\HidHide" /Y > NUL
copy "HidHide\HidHide.inf"   "C:\Program Files\Nefarius\HidHide" /Y > NUL
copy "HidHide.man"           "C:\Program Files\Nefarius\HidHide" /Y > NUL
copy "HidHide.wprp"          "C:\Program Files\Nefarius\HidHide" /Y > NUL

echo Updating Nefarius HidHide client
if not exist "C:\Program Files\Nefarius\HidHideClient" mkdir "C:\Program Files\Nefarius\HidHideClient"
copy "HidHideClient.exe"     "C:\Program Files\Nefarius\HidHideClient" /Y > NUL

echo Updating Symbols
if not exist "C:\Program Files\Nefarius\Symbols" mkdir "C:\Program Files\Nefarius\Symbols"
copy "HidHide.pdb"           "C:\Program Files\Nefarius\Symbols" /Y > NUL
copy "HidHideClient.pdb"     "C:\Program Files\Nefarius\Symbols" /Y > NUL

echo Clear logging
wevtutil.exe cl Application
wevtutil.exe cl Security
wevtutil.exe cl Setup
wevtutil.exe cl System

echo Restart tracing
wevtutil.exe um "C:\Program Files\Nefarius\HidHide\HidHide.man" > NUL
wevtutil.exe im "C:\Program Files\Nefarius\HidHide\HidHide.man" > NUL
wevtutil.exe sl "Nefarius-Drivers-HidHide/Diagnostic" /e:false /q:true > NUL
wevtutil.exe sl "Nefarius-Drivers-HidHide/Diagnostic" /e:true /q:true > NUL

echo Install device
".\nefconw.exe" install "C:\Program Files\Nefarius\HidHide\HidHide.inf" "root\HidHide" --no-duplicates --remove-duplicates
".\nefconw.exe" --add-class-filter --position upper --service-name HidHide --class-guid 745a17a0-74d3-11d0-b6fe-00a0c90f57da
".\nefconw.exe" --add-class-filter --position upper --service-name HidHide --class-guid d61ca365-5af4-4486-998b-9db4734c6ca3
".\nefconw.exe" --add-class-filter --position upper --service-name HidHide --class-guid 05f5cfe2-4733-4950-a6bb-07aad01a3a84

echo Done (inspect %windir%\INF\SetupApi.dev.log when needed)
rem goto :L1
:L2
