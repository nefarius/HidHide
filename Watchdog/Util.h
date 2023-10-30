#pragma once

#include <guiddef.h>
#include <string>

namespace util
{
    struct DeviceClassFilterPosition
    {
        enum Value
        {
            Upper,
            Lower
        };
    };

    bool AddDeviceClassFilter(const GUID* classGuid, const std::wstring& filterName,
                              DeviceClassFilterPosition::Value position);

    bool RemoveDeviceClassFilter(const GUID* classGuid, const std::wstring& filterName,
                                 DeviceClassFilterPosition::Value position);

    bool HasDeviceClassFilter(const GUID* classGuid, const std::wstring& filterName,
                              DeviceClassFilterPosition::Value position, bool& found);

    unsigned long IsAdminMode(bool& is_admin);

    bool IsAdmin();
};
