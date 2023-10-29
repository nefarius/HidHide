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

    bool add_device_class_filter(const GUID* classGuid, const std::wstring& filterName,
                                 DeviceClassFilterPosition::Value position);

    bool remove_device_class_filter(const GUID* classGuid, const std::wstring& filterName,
                                    DeviceClassFilterPosition::Value position);

    bool has_device_class_filter(const GUID* classGuid, const std::wstring& filterName,
                                 DeviceClassFilterPosition::Value position, bool& found);

    unsigned long is_admin_mode(bool& is_admin);

    bool is_admin();
};
