#include "App.hpp"
#include "Util.h"
#include <initguid.h>
#include <devguid.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// XnaComposite
DEFINE_GUID(GUID_DEVCLASS_XNACOMPOSITE,
            0xd61ca365L, 0x5af4, 0x4486, 0x99, 0x8b, 0x9d, 0xb4, 0x73, 0x4c, 0x6c, 0xa3);

// XboxComposite
DEFINE_GUID(GUID_DEVCLASS_XBOXCOMPOSITE,
            0x05f5cfe2L, 0x4733, 0x4950, 0xa6, 0xbb, 0x07, 0xaa, 0xd0, 0x1a, 0x3a, 0x84);

void App::initialize(Application& self)
{
    Application::initialize(self);
}

void App::uninitialize()
{
    Application::uninitialize();
}

int App::main(const std::vector<std::string>& args)
{
    const auto console = spdlog::stdout_color_mt("console");
    const auto err_logger = spdlog::stderr_color_mt("stderr");

    set_default_logger(err_logger);

    console->info("Application started");

    const auto serviceName = L"HidHide";

    // filter value or entry not present
    if (bool found = false; !has_device_class_filter(&GUID_DEVCLASS_HIDCLASS, serviceName,
                                                     util::DeviceClassFilterPosition::Upper, found) || !found)
    {
        console->warn("Filter missing for HIDClass, adding");

        if (!add_device_class_filter(&GUID_DEVCLASS_HIDCLASS, serviceName, util::DeviceClassFilterPosition::Upper))
        {
            err_logger->error("Failed to add upper filters entry for HIDClass");
        }
    }
    else
    {
        console->info("HIDClass is configured properly");
    }

    // filter value or entry not present
    if (bool found = false; !has_device_class_filter(&GUID_DEVCLASS_XNACOMPOSITE, serviceName,
                                                     util::DeviceClassFilterPosition::Upper, found) || !found)
    {
        console->warn("Filter missing for XnaComposite, adding");

        if (!add_device_class_filter(&GUID_DEVCLASS_XNACOMPOSITE, serviceName, util::DeviceClassFilterPosition::Upper))
        {
            err_logger->error("Failed to add upper filters entry for XnaComposite");
        }
    }
    else
    {
        console->info("XnaComposite is configured properly");
    }

    // filter value or entry not present
    if (bool found = false; !has_device_class_filter(&GUID_DEVCLASS_XBOXCOMPOSITE, serviceName,
                                                     util::DeviceClassFilterPosition::Upper, found) || !found)
    {
        console->warn("Filter missing for XboxComposite, adding");

        if (!add_device_class_filter(&GUID_DEVCLASS_XBOXCOMPOSITE, serviceName, util::DeviceClassFilterPosition::Upper))
        {
            err_logger->error("Failed to add upper filters entry for XboxComposite");
        }
    }
    else
    {
        console->info("XboxComposite is configured properly");
    }

    waitForTerminationRequest();

    console->info("Exiting application");

    return EXIT_OK;
}
