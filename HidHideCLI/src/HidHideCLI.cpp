// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// HidHideCLI.cpp
#include "stdafx.h"
#include "CommandInterpreter.h"
#include "Utils.h"
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
            HidHide::CommandInterpreter(false).Start(HidHide::CommandLineArguments());
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

// Register the ETW logging and tracing providers
NTSTATUS WINAPI LogRegisterProviders() noexcept
{
    try
    {
        EventRegisterNefarius_HidHide_CLI();
        EventRegisterNefarius_Drivers_HidHideCLI();

        // The define for BldProductVersion is passed from the project file to the source code via a define
        ::LogEvent(ETW(Started), L"%s", _L(BldProductVersion));
        return (STATUS_SUCCESS);
    }
    catch (...)
    {
        DBG_AND_RETURN_NTSTATUS("LogRegisterProviders", STATUS_UNHANDLED_EXCEPTION);
    }
}

// Unregister the ETW logging and tracing providers
NTSTATUS WINAPI LogUnregisterProviders() noexcept
{
    try
    {
        ::LogEvent(ETW(Stopped), L"");
        EventUnregisterNefarius_Drivers_HidHideCLI();
        EventUnregisterNefarius_HidHide_CLI();
        return (STATUS_SUCCESS);
    }
    catch (...)
    {
        DBG_AND_RETURN_NTSTATUS("LogUnregisterProviders", STATUS_UNHANDLED_EXCEPTION);
    }
}

_Success_(return == ERROR_SUCCESS)
int wmain(int argc, wchar_t* argv[], wchar_t* envp[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    UNREFERENCED_PARAMETER(envp);

    ::LogRegisterProviders();
    auto const result{ MainApplication() };
    ::LogUnregisterProviders();
    return (static_cast<int>(result));
}
