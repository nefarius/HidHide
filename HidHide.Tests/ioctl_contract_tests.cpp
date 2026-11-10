// SPDX-License-Identifier: MIT
#include <windows.h>

#include <gtest/gtest.h>

#include "HidHideIoctlContract.h"

namespace
{
    constexpr ULONG GoldenCtlCode(ULONG function)
    {
        return static_cast<ULONG>(
            (static_cast<ULONG>(32769u) << 16) | (static_cast<ULONG>(FILE_READ_DATA) << 14)
            | (function << 2) | static_cast<ULONG>(METHOD_BUFFERED));
    }
}

TEST(IoctlContract, IoControlDeviceType)
{
    EXPECT_EQ(32769u, static_cast<unsigned>(IoControlDeviceType));
}

TEST(IoctlContract, IoctlCodesMatchHistoricAbi)
{
    EXPECT_EQ(GoldenCtlCode(2048u), static_cast<ULONG>(IOCTL_GET_WHITELIST));
    EXPECT_EQ(GoldenCtlCode(2049u), static_cast<ULONG>(IOCTL_SET_WHITELIST));
    EXPECT_EQ(GoldenCtlCode(2050u), static_cast<ULONG>(IOCTL_GET_BLACKLIST));
    EXPECT_EQ(GoldenCtlCode(2051u), static_cast<ULONG>(IOCTL_SET_BLACKLIST));
    EXPECT_EQ(GoldenCtlCode(2052u), static_cast<ULONG>(IOCTL_GET_ACTIVE));
    EXPECT_EQ(GoldenCtlCode(2053u), static_cast<ULONG>(IOCTL_SET_ACTIVE));
    EXPECT_EQ(GoldenCtlCode(2054u), static_cast<ULONG>(IOCTL_GET_WLINVERSE));
    EXPECT_EQ(GoldenCtlCode(2055u), static_cast<ULONG>(IOCTL_SET_WLINVERSE));
    EXPECT_EQ(GoldenCtlCode(2056u), static_cast<ULONG>(IOCTL_ADD_SESSION_BLACKLIST));
    EXPECT_EQ(GoldenCtlCode(2057u), static_cast<ULONG>(IOCTL_CLR_SESSION_BLACKLIST));
}
