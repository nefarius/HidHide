> [!WARNING]
> **No other official website exists** for this project besides this GitHub repository (github.com/nefarius/HidHide) and sites hosted on `*.nefarius.at` (e.g. docs.nefarius.at, discord.nefarius.at).
>
> **Anyone claiming otherwise is a scammer and a fraud.** Do not trust other websites, download links, or people claiming to represent this project. Please be wary and only use the sources listed above.

---

# <img src="assets/hidhide-128x128.png" align="left" />HidHide

[![Build status](https://ci.appveyor.com/api/projects/status/s3t4ffx5fnfw5g65/branch/master?svg=true)](https://ci.appveyor.com/project/nefarius/hidhide/branch/master)
[![GitHub All Releases](https://img.shields.io/github/downloads/nefarius/HidHide/total)](https://somsubhra.github.io/github-release-stats/?username=nefarius&repository=HidHide)
[![Chocolatey package](https://img.shields.io/chocolatey/dt/hidhide?color=blue&label=chocolatey)](https://community.chocolatey.org/packages/hidhide)
![GitHub issues by-label](https://img.shields.io/github/issues/nefarius/HidHide/bug)
![GitHub issues by-label](https://img.shields.io/github/issues/nefarius/HidHide/enhancement)

Gaming Input Peripherals Device Firewall for Windows.

## Introduction

*Microsoft Windows* offers support for a wide range of human interface devices, like joysticks and game pads.
Associating the buttons and axes of these devices with application specific behavior, such as *Fire*, *Roll*, or *Pitch*
is however left to the individual application developers to realize.

While there are good examples of applications allowing a user to customize the controls to their liking, other
applications are less sophisticated or lack just that feature a user is looking for. This is where utilities like *vJoy*
and *Joystick Gremlin* come to the rescue. These utilities aren't limited by a vendor lock-in and attempt to move
certain features back into the domain of the operating system. Once properly arranged, a feature becomes
universally available for a wide range of applications.

A technique used by these utilities is to use a feeder application that listens to the physical devices on a system,
and in turn controls one or more virtual devices where the game or application is listening to. Mapping physical
devices to a virtual device allows for e.g. dual joystick support in games that only support a single joystick, or
enable multiple devices to bind to the one and same function in a game that only supports single controller bindings.

While this approach offers a lot of advantages, it also comes with a side effect. Most applications record the user
interactions while binding a function with a control or button press. When a virtual device is used, the application
receives input from two devices simultaneously. It will be notified by both the physical device triggered, and the
virtual device that acts in turn! Some feeders have an option to spam the application repeatedly; however, that approach is
cumbersome and error prone.

With *HidHide* it is possible to deny a specific application access to one or more human interface devices, effectively
hiding a device from the application. When a HOTAS is preferred for a flight-simulator one can hide the game pads.
When a steering wheel is preferred for a racing game, one can hide the joysticks, and so on. When, as mentioned
above, a feeder utility is used, one can use *HidHide* to hide the physical device from the application, hence avoiding
multiple notifications while binding game functions and device controls.

## Package content

*HidHide* is a kernel-mode filter driver available for **Windows 10** or higher (KMDF 1.13+). It comes with a configuration
utility via which the driver is configured and controlled. The filter driver starts automatically and runs unattended
with system privileges. A system reboot may be triggered after driver installation or removal. The configuration utility
runs in the least privileged mode and doesn't require elevated rights.

## User guide

The configuration utility allows you to:

- Enable or disable the service
- Specify which applications may look through the cloak
- Specify the human interface devices that should be hidden from ordinary applications

The main dialog of the configuration utility offers two main tabs.
![Screen capture of applications tab](/README/DlgApplications.jpg)

The *Applications* tab shows all white-listed applications that are allowed access to the hidden devices. Typically listed
here are vendor-specific utilities for configuring the human interface devices, and feeder utilities. Entries can be added
to the list by pressing *+*. Select one or more entries with the *shift* and/or *control* key and press *-* to remove entries
from the list. Notice that the client replaces a logical drive letter by a full path. This is intentionally and offers some
resilience for changes in logical drive mapping.

![Screen capture of devices tab](/README/DlgDevices.jpg)

Per default, the *Devices* tab lists all *Gaming devices* currently connected to the system. The list refreshes automatically
when a new device is detected. The dialog offers two check boxes for filtering.

Via *Filter-out disconnected* one can extend the list with devices that were connected earlier to the system but are
currently not present. With *Gaming devices only* one can limit the list to game pads and joysticks only. This feature
relies on proper information from the device vendors. Some vendors however use vendor-specific codes. Be sure to
switch off this filter should you notice that your gaming device seems absent in the list. The filters are ignored for
devices that are selected for hiding, so that one has a complete overview on the hidden devices.

Last but not least, the *Enable device hiding* check box provides control over the *HidHide* service. When enabled it
blocks access to the black-listed devices unless the application is explicitly white-listed. When disabled, all applications
are granted access to all devices.

An entry in the list can be expanded to reveal the composite devices associated with a device and offers fine-grained
control over a device. *HidHide* uses the selection also for a secondary purpose. Some legacy applications ignore the
human interface device layer offered by the operating system and instead interact with the underlying device driver.
Access to the underlying driver will be blocked when a device only has composite HID devices, and all are selected.

The expanded list may mark entries as *absent* or *denied*. *absent* entries appear when the device characteristics are altered.
These are residual entries in the caches of the operating system, and can be cleaned-up using utilities like *Device Cleanup Tool*.
*denied* entries appear for hidden devices when the configuration utility itself is not whilelisted.

## Package integration

Installation packages and third-party applications can rely on the following two registry keys.
*"HKCR\Installer\Dependencies\NSS.Drivers.HidHide.x64\Version"* signals the availability of HidHide and its version.
*"HKCR\SOFTWARE\Nefarius Software Solutions e.U.\Nefarius Software Solutions e.U. HidHide\Path"* tells its location.

Third-party software deployment may benefit from the *HidHide Command Line Interface (CLI)* while deploying software.
Please be conservative while altering a clients' configuration and only extend the configuration with new features offered.
Don't assume exclusive ownership of the configuration settings as a recovery typically requires manual actions by the user.

## Programmatic integration

Applications that need to talk to the HidHide kernel driver at runtime do so through a WDM control device exposed as the
symbolic link **`\\.\HidHide`** (DOS device name). The same device can also be opened by interface GUID
**`{0C320FF7-BD9B-42B6-BDAF-49FEB9C91649}`** via `SetupDiGetClassDevs` / `CM_Get_Device_Interface_List`.

### IOCTL reference

All IOCTLs use `METHOD_BUFFERED` / `FILE_READ_DATA` and are constructed with:

```c
CTL_CODE(/*DeviceType=*/32769, /*Function=*/N, METHOD_BUFFERED, FILE_READ_DATA)
```

| Function | Name | Input | Output | Notes |
|---|---|---|---|---|
| 2048 | `GET_WHITELIST` | — | `MULTI_SZ` | Full image paths of whitelisted applications |
| 2049 | `SET_WHITELIST` | `MULTI_SZ` | — | Replaces the whitelist |
| 2050 | `GET_BLACKLIST` | — | `MULTI_SZ` | Device instance paths of hidden devices |
| 2051 | `SET_BLACKLIST` | `MULTI_SZ` | — | Replaces the blacklist |
| 2052 | `GET_ACTIVE` | — | `DWORD` | 1 = device hiding enabled |
| 2053 | `SET_ACTIVE` | `DWORD` | — | Enable / disable device hiding |
| 2054 | `GET_WLINVERSE` | — | `DWORD` | 1 = whitelist inverse mode enabled |
| 2055 | `SET_WLINVERSE` | `DWORD` | — | Enable / disable whitelist inverse mode |
| 2056 | `ADD_SESSION_BLACKLIST` | `MULTI_SZ` | — | Add devices to the session (process-lifetime) blacklist |
| 2057 | `CLR_SESSION_BLACKLIST` | — | — | Remove all session blacklist entries for the calling process |

### MULTI_SZ wire format

List IOCTLs use the standard Windows `MULTI_SZ` format: a sequence of null-terminated wide-character strings (`WCHAR`)
followed by an extra null terminator to mark the end of the list. The buffer size passed to `DeviceIoControl` must
include all null terminators and be an even number of bytes.

```
L"entry_one\0entry_two\0\0"
 ─────────── ─────────── ─
 string 1    string 2    list terminator
```

For device instance paths use the format returned by `SetupDiGetDeviceInstanceId`, e.g.
`HID\VID_054C&PID_09CC&MI_03\7&...`.

### Session blacklist

The **session blacklist** (functions 2056 / 2057) is designed for feeder applications — programs that exclusively own
one or more physical devices and re-expose them as virtual controllers. Unlike the persistent blacklist (functions
2050 / 2051), session blacklist entries are:

- **Process-scoped** — each entry is owned by the process that issued the IOCTL.
- **Automatically removed** — when the owning process exits for any reason (clean shutdown, crash, or kill), the
  kernel driver removes all of its entries via `PsSetCreateProcessNotifyRoutine`. No cleanup code is required in the
  application.
- **Non-persistent** — entries exist only in kernel memory and are never written to the registry, leaving the user's
  permanent HidHide configuration untouched.

#### Recommended usage pattern

1. On startup, call `ADD_SESSION_BLACKLIST` with the `MULTI_SZ` list of device instance paths to hide.
2. Use the device exclusively for the lifetime of the process.
3. On clean exit, optionally call `CLR_SESSION_BLACKLIST` to release immediately; otherwise the driver clears the
   entries automatically when the process handle closes.

This is preferable to temporarily mutating the persistent blacklist because it requires no rollback logic and is safe
against crashes or forced termination.

## Bugs & Features

~~Found a bug and want it fixed? Feel free to open a detailed issue on the [GitHub issue tracker](../../issues)!~~

There is currently no capacity for any major works on HidHide, if you wish to see this change, consider contributing.

Contact us [through Discord](https://discord.nefarius.at/)!

---

*HidHide* provides both logging and tracing. Logging can be found the *Event Viewer* under *Windows Logs* and *System*.
Tracing can be found under *Applications and Services Logs* and *Nefarius* after enabling *Show Analytic and Debug Logs*.
Extended tracing is available but switched off per default for performance reasons. Tracing is controlled using the *wevtutil* utility
which is an integral part of the operating system. To enable extended tracing, open a command shell, and enter the following;

```cmd
wevtutil set-log Nefarius-Drivers-HidHide/Diagnostic /e:false
wevtutil set-log Nefarius-Drivers-HidHide/Diagnostic /k:5
wevtutil set-log Nefarius-Drivers-HidHide/Diagnostic /e:true
```

Tracing adjustments remain in affect after a reboot. Restore tracing to its default level using the above sequence with /k:1 instead.
Tracing to the debug console is enabled with /k:3 and /k:7 respectively.

## Questions & Support

Please respect that the GitHub issue tracker isn't a help desk. [Look at the community support resources](https://docs.nefarius.at/Community-Support/).

## Donations

> From creator Eric

Creating a utility like this requires time and dedication. Should you like to express your gratitude, consider a pledge
for a game I'm rather fond of; the biggest crowd funded game currently in development *Star Citizen*. Be sure to apply a
referral code at account creation as it gives a bit more in-game currency and can't be applied later on. My referral code
is *STAR-K6S5-KPY7* should you seek one. Have fun and see you in the verse!

> From maintainer Nefarius

You can find all my donation options [over here](https://docs.nefarius.at/Donations/)!
