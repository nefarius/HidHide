// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// HidHideCLI.cpp
#include "stdafx.h"
#include "CommandInterpreter.h"
#include "Utils.h"
#include "Volume.h"
#include "Logging.h"

namespace
{
    // Main application handling exceptions
    _Success_(return == ERROR_SUCCESS)
    DWORD MainApplication() noexcept
    {
        try
        {
            TRACE_ALWAYS(L"");

            // Ensure this application is on the whitelist before loading the filter driver proxy
            auto const deviceName{ HidHide::StringTable(IDS_CONTROL_DEVICE_NAME) };
            auto const fullImageName{ HidHide::FileNameToFullImageName(HidHide::ModuleFileName()) };
            if (!fullImageName.empty()) HidHide::FilterDriverProxy::WhitelistAddEntry(deviceName, fullImageName);

            HidHide::CommandInterpreter(deviceName).Start(HidHide::CommandLineArguments());
            return (ERROR_SUCCESS);
        }
        catch (std::exception const& exc)
        {
            std::wcerr << exc.what() << std::endl;
            LOGEXC_AND_RETURN_WIN32;
        }
        catch (...)
        {
            std::wcerr << L"Unhandled exception" << std::endl;
            LOGEXC_AND_RETURN_WIN32;
        }
    }
}

_Success_(return == ERROR_SUCCESS)
int wmain(int argc, wchar_t* argv[], wchar_t* envp[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    UNREFERENCED_PARAMETER(envp);

    LogRegisterProviders();
    auto const result{ MainApplication() };
    LogUnregisterProviders();
    return (static_cast<int>(result));
}
