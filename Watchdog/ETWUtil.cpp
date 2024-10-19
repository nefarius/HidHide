#include "ETWUtil.h"

#include <scope_guard.hpp>

#include <winevt.h>
#include <evntrace.h>
#include <tdh.h>
#include <strsafe.h>

using namespace nefarius::utilities;


std::expected<void, Win32Error> etw::utils::SetLogEnabled(LPCWSTR channel, bool enabled)
{
    EVT_HANDLE config = EvtOpenChannelConfig(NULL, channel, 0);
    if (!config)
    {
        return std::unexpected(Win32Error("EvtOpenChannelConfig"));
    }

    auto guard = sg::make_scope_guard(
        [ config ]() noexcept
        {
            if (config)
                EvtClose(config);
        });

    EVT_VARIANT logEnabled = {};
    logEnabled.Type = EvtVarTypeBoolean;
    logEnabled.BooleanVal = enabled;

    if (!EvtSetChannelConfigProperty(config, EvtChannelConfigEnabled, 0, &logEnabled))
    {
        return std::unexpected(Win32Error("EvtSetChannelConfigProperty"));
    }

    if (!EvtSaveChannelConfig(config, 0))
    {
        return std::unexpected(Win32Error("EvtSaveChannelConfig"));
    }

    return {};
}

std::expected<void, Win32Error> etw::utils::StartSession()
{
    // TODO: example code, finish and test!

    TRACEHANDLE sessionHandle = 0;
    EVENT_TRACE_PROPERTIES* sessionProperties = NULL;
    DWORD bufferSize = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(TCHAR) * (MAX_PATH + 1);
    sessionProperties = (EVENT_TRACE_PROPERTIES*)malloc(bufferSize);
    ZeroMemory(sessionProperties, bufferSize);

    // Set the properties for the session
    sessionProperties->Wnode.BufferSize = bufferSize;
    sessionProperties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    sessionProperties->Wnode.ClientContext = 1; // QPC clock resolution
    sessionProperties->Wnode.Guid = SystemTraceControlGuid; // GUID for kernel tracing
    sessionProperties->LogFileMode = EVENT_TRACE_FILE_MODE_SEQUENTIAL; // Use file for output
    sessionProperties->MaximumFileSize = 10; // Maximum size of log file in MB
    sessionProperties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    sessionProperties->LogFileNameOffset = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(TCHAR) * (MAX_PATH + 1);


    TCHAR logFileName[MAX_PATH] = TEXT("C:\\ETWLog.etl");
    StringCchCopy((TCHAR*)(((char*)sessionProperties) + sessionProperties->LogFileNameOffset), MAX_PATH, logFileName);

    TCHAR sessionName[MAX_PATH] = TEXT("MyETWSession");
    ULONG status = StartTrace(&sessionHandle, sessionName, sessionProperties);

    if (status != ERROR_SUCCESS)
    {
        printf("Error starting trace session: %lu\n", status);
        //return 1;
    }

    printf("ETW session started successfully!\n");

    status = EnableTraceEx2(
        sessionHandle,
        &SystemTraceControlGuid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_VERBOSE,
        0,
        0,
        0,
        NULL
    );

    if (status != ERROR_SUCCESS)
    {
        printf("Error enabling provider: %lu\n", status);
        //return 1;
    }

    printf("Provider enabled successfully!\n");

    return {};
}
