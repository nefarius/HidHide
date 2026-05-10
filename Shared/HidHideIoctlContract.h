// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// HidHideIoctlContract.h — shared user-mode / kernel IOCTL definitions (single ABI contract).
#pragma once

#ifndef CTL_CODE
#ifdef _KERNEL_MODE
#include <wdm.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif
#endif

// The HidHide I/O control custom device type (range 32768 .. 65535)
#define IoControlDeviceType 32769

// The HidHide I/O control codes
#define IOCTL_GET_WHITELIST         CTL_CODE(IoControlDeviceType, 2048, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_SET_WHITELIST         CTL_CODE(IoControlDeviceType, 2049, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_GET_BLACKLIST         CTL_CODE(IoControlDeviceType, 2050, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_SET_BLACKLIST         CTL_CODE(IoControlDeviceType, 2051, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_GET_ACTIVE            CTL_CODE(IoControlDeviceType, 2052, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_SET_ACTIVE            CTL_CODE(IoControlDeviceType, 2053, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_GET_WLINVERSE         CTL_CODE(IoControlDeviceType, 2054, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_SET_WLINVERSE         CTL_CODE(IoControlDeviceType, 2055, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_ADD_SESSION_BLACKLIST   CTL_CODE(IoControlDeviceType, 2056, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_CLR_SESSION_BLACKLIST   CTL_CODE(IoControlDeviceType, 2057, METHOD_BUFFERED, FILE_READ_DATA)
