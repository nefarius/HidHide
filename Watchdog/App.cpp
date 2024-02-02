#include "App.hpp"
#include "Util.h"
#include <initguid.h>
#include <devguid.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/win_eventlog_sink.h>

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
        DWORD error = ERROR_SUCCESS;

        __try
        {
            sch = OpenSCManager(
                nullptr,
                nullptr,
                SC_MANAGER_ALL_ACCESS
            );
            if (sch == nullptr)
            {
                error = GetLastError();
                __leave;
            }

            svc = OpenService(
                sch,
                serviceName.c_str(),
                SC_MANAGER_ALL_ACCESS
            );
            if (svc == nullptr)
            {
                error = GetLastError();
                __leave;
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
                error = GetLastError();
                __leave;
            }

            serviceState = stat.dwCurrentState;

            error = ERROR_SUCCESS;
            __leave;
        }
        __finally
        {
            if (svc)
                CloseServiceHandle(svc);
            if (sch)
                CloseServiceHandle(sch);
        }

        return error;
    }

public:
    explicit WatchdogTask(const std::string& name)
        : Task(name)
    {
    }

    void runTask() override
    {
        spdlog::get("console")->info("Started watchdog background thread");

        do
        {
            const auto serviceName = L"HidHide";
            unsigned long winError = 0, serviceStatus = 0;

            // check if driver service is healthy
            if ((winError = CheckServiceStatus(serviceName, serviceStatus)) != ERROR_SUCCESS)
            {
                spdlog::error("Failed to query service status, error {}", winError);
                continue;
            }

            // expecting service to be running as an indicator that driver is loaded
            if (serviceStatus != SERVICE_RUNNING)
            {
                spdlog::error("Driver service not detected running, removing filter entries");

                //
                // Prevents bricked HID devices
                // 

                RemoveDeviceClassFilter(&GUID_DEVCLASS_HIDCLASS,
                                        serviceName, util::DeviceClassFilterPosition::Upper);
                RemoveDeviceClassFilter(&GUID_DEVCLASS_XNACOMPOSITE,
                                        serviceName, util::DeviceClassFilterPosition::Upper);
                RemoveDeviceClassFilter(&GUID_DEVCLASS_XBOXCOMPOSITE,
                                        serviceName, util::DeviceClassFilterPosition::Upper);

                continue;
            }

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
        // sleep breaks on app termination
        while (!sleep(5000));

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

    if (this->isInteractive())
    {
        set_default_logger(err_logger);
    }
    else
    {
        const auto event_log = std::make_shared<spdlog::sinks::win_eventlog_sink_mt>("HidHideWatchdog");
        const auto event_logger = std::make_shared<spdlog::logger>("eventlog", event_log);
        set_default_logger(event_logger);
    }

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
