#include "App.hpp"
#include <initguid.h>
#include <devguid.h>
#include <winevt.h>
#include <evntrace.h>
#include <tdh.h>
#include <strsafe.h>

#include <variant>
#include <expected>
#include <format>
#include <iostream>

#include <nefarius/neflib/MiscWinApi.hpp>
#include <nefarius/neflib/ClassFilter.hpp>

#pragma warning(disable: 26800)
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/win_eventlog_sink.h>
#pragma warning(default: 26800)

#include <Poco/Task.h>
#include <Poco/TaskManager.h>

#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/Timestamp.h>

using namespace Poco;
using namespace Poco::Net;
using namespace Poco::JSON;
using namespace Poco::Util;


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
    bool _isInteractive;

public:
    explicit WatchdogTask(const std::string& name, const bool isInteractive)
        : Task(name)
    {
        _isInteractive = isInteractive;
    }

    void runTask() override
    {
        const auto logger = _isInteractive
                                ? spdlog::get("console")
                                : spdlog::get("eventlog");

        logger->info("Started watchdog background thread");

        do
        {
            const std::wstring serviceName = L"HidHide";

            const auto serviceStatus = nefarius::winapi::services::GetServiceStatus(serviceName);

            // check if driver service is healthy
            if (!serviceStatus)
            {
                logger->error("Failed to query service status, error {}", serviceStatus.error().getErrorMessageA());
                continue;
            }

            // expecting service to be running as an indicator that driver is loaded
            if (serviceStatus.value().dwCurrentState != SERVICE_RUNNING)
            {
                logger->error("Driver service not detected running, removing filter entries");

                //
                // Prevents bricked HID devices
                // 

                auto removeResult = RemoveDeviceClassFilter(&GUID_DEVCLASS_HIDCLASS,
                                                            serviceName,
                                                            nefarius::devcon::DeviceClassFilterPosition::Upper);

                if (!removeResult)
                {
                    logger->error("Removal from GUID_DEVCLASS_HIDCLASS failed with {}",
                                  removeResult.error().getErrorMessageA());
                }

                removeResult = RemoveDeviceClassFilter(&GUID_DEVCLASS_XNACOMPOSITE,
                                                       serviceName,
                                                       nefarius::devcon::DeviceClassFilterPosition::Upper);

                if (!removeResult)
                {
                    logger->error("Removal from GUID_DEVCLASS_XNACOMPOSITE failed with {}",
                                  removeResult.error().getErrorMessageA());
                }

                removeResult = RemoveDeviceClassFilter(&GUID_DEVCLASS_XBOXCOMPOSITE,
                                                       serviceName,
                                                       nefarius::devcon::DeviceClassFilterPosition::Upper);

                if (!removeResult)
                {
                    logger->error("Removal from GUID_DEVCLASS_XBOXCOMPOSITE failed with {}",
                                  removeResult.error().getErrorMessageA());
                }

                continue;
            }

            // filter value or entry not present
            if (!HasDeviceClassFilter(&GUID_DEVCLASS_HIDCLASS, serviceName,
                                      nefarius::devcon::DeviceClassFilterPosition::Upper).value_or(false))
            {
                logger->warn("Filter missing for HIDClass, adding");

                if (!AddDeviceClassFilter(&GUID_DEVCLASS_HIDCLASS, serviceName,
                                          nefarius::devcon::DeviceClassFilterPosition::Upper))
                {
                    logger->error("Failed to add upper filters entry for HIDClass");
                }
            }

            // filter value or entry not present
            if (!HasDeviceClassFilter(&GUID_DEVCLASS_XNACOMPOSITE, serviceName,
                                      nefarius::devcon::DeviceClassFilterPosition::Upper).value_or(false))
            {
                logger->warn("Filter missing for XnaComposite, adding");

                if (!AddDeviceClassFilter(&GUID_DEVCLASS_XNACOMPOSITE, serviceName,
                                          nefarius::devcon::DeviceClassFilterPosition::Upper))
                {
                    logger->error("Failed to add upper filters entry for XnaComposite");
                }
            }

            // filter value or entry not present
            if (!HasDeviceClassFilter(&GUID_DEVCLASS_XBOXCOMPOSITE, serviceName,
                                      nefarius::devcon::DeviceClassFilterPosition::Upper).value_or(false))
            {
                logger->warn("Filter missing for XboxComposite, adding");

                if (!AddDeviceClassFilter(&GUID_DEVCLASS_XBOXCOMPOSITE, serviceName,
                                          nefarius::devcon::DeviceClassFilterPosition::Upper))
                {
                    logger->error("Failed to add upper filters entry for XboxComposite");
                }
            }
        }
        // sleep breaks on app termination
        while (!sleep(5000));

        logger->info("Stopping watchdog background thread");
    }
};

//
// REST API /api/etw/session handler
// 
class ETWRequestHandler : public HTTPRequestHandler
{
public:
    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override
    {
        if (request.getURI() != "/api/etw/session")
        {
            response.setStatus(HTTPResponse::HTTP_NOT_FOUND);
            response.send();
            return;
        }

        const auto& method = request.getMethod();

        // get session details
        if (method == HTTPRequest::HTTP_GET)
        {
        }
        // start/create session
        else if (method == HTTPRequest::HTTP_POST)
        {
        }
        // stop/remove session
        else if (method == HTTPRequest::HTTP_DELETE)
        {
        }
        // unsupported
        else
        {
            response.setStatus(HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
            response.send();
            return;
        }

        response.setContentType("application/json");
        response.setStatus(HTTPResponse::HTTP_OK);

        // Create a JSON response
        Object::Ptr jsonResponse = new Object();
        jsonResponse->set("message", "Hello from POCO HTTP Server");
        jsonResponse->set("timestamp", Timestamp().epochTime());

        // Send the JSON response
        std::ostream& out = response.send();
        Stringifier::stringify(jsonResponse, out);
    }
};

class ETWRequestHandlerFactory : public HTTPRequestHandlerFactory
{
public:
    HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& request) override
    {
        return new ETWRequestHandler;
    }
};

class WebServerTask : public Poco::Task
{
    bool _isInteractive;

public:
    explicit WebServerTask(const std::string& name, const bool isInteractive)
        : Task(name)
    {
        _isInteractive = isInteractive;
    }

    void runTask() override
    {
        const auto logger = _isInteractive
                                ? spdlog::get("console")
                                : spdlog::get("eventlog");

        logger->info("Starting web server background thread");

        SocketAddress sa("127.0.0.1", 34501);
        ServerSocket serverSocket(sa);
        HTTPServerParams::Ptr params = new HTTPServerParams;
        params->setMaxQueued(100);
        params->setMaxThreads(4);

        HTTPServer server(new ETWRequestHandlerFactory(), serverSocket, params);
        server.start();

        logger->info("Started web server");

        while (!sleep(1000))
        {
            if (isCancelled())
            {
                server.stop();
                break;
            }
        }

        logger->info("Stopping web server background thread");
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
    const auto errLogger = spdlog::stderr_color_mt("stderr");

    if (this->isInteractive())
    {
        set_default_logger(errLogger);
    }
    else
    {
        const auto eventLog = std::make_shared<spdlog::sinks::win_eventlog_sink_mt>("HidHideWatchdog");
        const auto eventLogger = std::make_shared<spdlog::logger>("eventlog", eventLog);
        set_default_logger(eventLogger);
    }

    console->info("Application started");

#if !defined(_DEBUG)
    const auto isAdmin = nefarius::winapi::security::IsAppRunningAsAdminMode();

    if (isAdmin.value_or(false))
#endif
    {
        Poco::TaskManager tm;
        // filter driver watchdog
        tm.start(new WatchdogTask("HidHideWatchdog", this->isInteractive()));
#if defined(EXPERIMENTAL)
        // ETW session web server
        tm.start(new WebServerTask("HidHideWebServer", this->isInteractive()));
#endif
        waitForTerminationRequest();
        tm.cancelAll();
        tm.joinAll();
    }
#if !defined(_DEBUG)
    else
    {
        errLogger->error("App need administrative permissions to run");
    }
#endif

    console->info("Exiting application");

#if !defined(_DEBUG)
    return isAdmin ? EXIT_OK : EXIT_TEMPFAIL;
#else
    return EXIT_OK;
#endif
}
