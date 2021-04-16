// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Device.h
#pragma once
#include "Logic.h"

EXTERN_C_START

_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS HidHideCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit);

EXTERN_C_END
