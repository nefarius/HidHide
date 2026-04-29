#pragma once

#include "HidHideDiagnosticsTypes.hpp"

#include <Poco/JSON/Object.h>

#include <string>

namespace hidhide::diag
{
    TraceStartRequest ParseStartBody(const std::string& jsonUtf8);

    void WriteStatusJson(Poco::JSON::Object& out, const TraceStatusResponse& r);

    void WriteErrorJson(Poco::JSON::Object& out, const ApiError& e);
}
