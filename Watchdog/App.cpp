#include "App.hpp"
#include "Util.h"
#include <initguid.h>
#include <devguid.h>

#include <scope_guard.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <Poco/Task.h>
#include <Poco/TaskManager.h>

// XnaComposite
DEFINE_GUID(GUID_DEVCLASS_XNACOMPOSITE,
            0xd61ca365L, 0x5af4, 0x4486, 0x99, 0x8b, 0x9d, 0xb4, 0x73, 0x4c, 0x6c, 0xa3);

// XboxComposite
DEFINE_GUID(GUID_DEVCLASS_XBOXCOMPOSITE,
            0x05f5cfe2L, 0x4733, 0x4950, 0xa6, 0xbb, 0x07, 0xaa, 0xd0, 0x1a, 0x3a, 0x84);

//
// Background task running as long as the service is running
// 
class WatchdogTask : public Poco::Task
{
    static DWORD CheckServiceStatus(const std::wstring& serviceName, unsigned long& serviceState)
    {
        SC_HANDLE sch = nullptr;
        SC_HANDLE svc = nullptr;

        sg::make_scope_guard(
            [sch, svc]() noexcept
            {
                if (sch) CloseServiceHandle(svc);
                if (svc) CloseServiceHandle(svc);
            });

        sch = OpenSCManager(
            nullptr,
            nullptr,
            SC_MANAGER_ALL_ACCESS
        );
        if (sch == nullptr)
        {
            return GetLastError();
        }

        svc = OpenService(
            sch,
            serviceName.c_str(),
            SC_MANAGER_ALL_ACCESS
        );
        if (svc == nullptr)
        {
            return GetLastError();
        }

        SERVICE_STATUS_PROCESS stat;
        DWORD needed = 0;
        BOOL ret = QueryServiceStatusEx(
            svc,
            SC_STATUS_PROCESS_INFO,
            (BYTE*)&stat,
            sizeof stat,
            &needed
        );
        if (ret == 0)
        {
            return GetLastError();
        }

        serviceState = stat.dwCurrentState;

        return ERROR_SUCCESS;
    }

public:
    explicit WatchdogTask(const std::string& name)
        : Task(name)
    {
    }

    void runTask() override
    {
        spdlog::get("console")->info("Started watchdog background thread");

        // sleep breaks on app termination
        while (!sleep(3000))
        {
            const auto serviceName = L"HidHide";

            // filter value or entry not present
            if (bool found = false; !HasDeviceClassFilter(&GUID_DEVCLASS_HIDCLASS, serviceName,
                                                             util::DeviceClassFilterPosition::Upper, found) || !found)
            {
                spdlog::warn("Filter missing for HIDClass, adding");

                if (!AddDeviceClassFilter(&GUID_DEVCLASS_HIDCLASS, serviceName,
                                             util::DeviceClassFilterPosition::Upper))
                {
                    spdlog::error("Failed to add upper filters entry for HIDClass");
                }
            }

            // filter value or entry not present
            if (bool found = false; !HasDeviceClassFilter(&GUID_DEVCLASS_XNACOMPOSITE, serviceName,
                                                             util::DeviceClassFilterPosition::Upper, found) || !found)
            {
                spdlog::warn("Filter missing for XnaComposite, adding");

                if (!AddDeviceClassFilter(&GUID_DEVCLASS_XNACOMPOSITE, serviceName,
                                             util::DeviceClassFilterPosition::Upper))
                {
                    spdlog::error("Failed to add upper filters entry for XnaComposite");
                }
            }

            // filter value or entry not present
            if (bool found = false; !HasDeviceClassFilter(&GUID_DEVCLASS_XBOXCOMPOSITE, serviceName,
                                                             util::DeviceClassFilterPosition::Upper, found) || !found)
            {
                spdlog::warn("Filter missing for XboxComposite, adding");

                if (!AddDeviceClassFilter(&GUID_DEVCLASS_XBOXCOMPOSITE, serviceName,
                                             util::DeviceClassFilterPosition::Upper))
                {
                    spdlog::error("Failed to add upper filters entry for XboxComposite");
                }
            }
        }

        spdlog::get("console")->info("Stopping watchdog background thread");
    }
};

void App::initialize(Application& self)
{
    Application::initialize(self);
}

void App::uninitialize()
{
    Application::uninitialize();
}

//
// Main service routine
// 
int App::main(const std::vector<std::string>& args)
{
    const auto console = spdlog::stdout_color_mt("console");
    const auto err_logger = spdlog::stderr_color_mt("stderr");

    set_default_logger(err_logger);

    console->info("Application started");

    const bool is_admin = util::IsAdmin();

    if (is_admin)
    {
        Poco::TaskManager tm;
        tm.start(new WatchdogTask("HidHideWatchdog"));
        waitForTerminationRequest();
        tm.cancelAll();
        tm.joinAll();
    }
    else
    {
        err_logger->error("App need administrative permissions to run");
    }

    console->info("Exiting application");

    return is_admin ? EXIT_OK : EXIT_TEMPFAIL;
}
