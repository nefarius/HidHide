#include "HidHideDiagnosticsTypes.hpp"

#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>

#include <algorithm>
#include <cctype>

namespace hidhide::diag
{
    namespace
    {
        bool IEquals(std::string_view a, std::string_view b)
        {
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); ++i)
            {
                const unsigned char ca = static_cast<unsigned char>(a[i]);
                const unsigned char cb = static_cast<unsigned char>(b[i]);
                if (std::tolower(ca) != std::tolower(cb))
                    return false;
            }
            return true;
        }
    }

    std::optional<TraceScope> ParseScope(const std::string_view value)
    {
        if (IEquals(value, "full"))
            return TraceScope::Full;
        if (IEquals(value, "driver"))
            return TraceScope::Driver;
        if (IEquals(value, "userspace"))
            return TraceScope::Userspace;
        return std::nullopt;
    }

    TraceStartRequest ParseStartBody(const std::string& jsonUtf8)
    {
        TraceStartRequest req{};
        if (jsonUtf8.empty())
            return req;

        Poco::JSON::Parser parser;
        const auto result = parser.parse(jsonUtf8);
        const auto obj = result.extract<Poco::JSON::Object::Ptr>();
        if (!obj)
            return req;

        if (obj->has("scope"))
            if (const auto s = ParseScope(obj->getValue<std::string>("scope")))
                req.scope = *s;

        if (obj->has("tuneChannels"))
            req.tuneChannels = obj->getValue<bool>("tuneChannels");

        return req;
    }

    void WriteStatusJson(Poco::JSON::Object& out, const TraceStatusResponse& r)
    {
        switch (r.state)
        {
        case TraceSessionState::Idle:
            out.set("state", "idle");
            break;
        case TraceSessionState::Recording:
            out.set("state", "recording");
            break;
        case TraceSessionState::Ready:
            out.set("state", "ready");
            break;
        }

        if (r.startedAtEpoch)
            out.set("startedAt", static_cast<Poco::Int64>(*r.startedAtEpoch));
        if (r.stoppedAtEpoch)
            out.set("stoppedAt", static_cast<Poco::Int64>(*r.stoppedAtEpoch));
        if (r.suggestedFileName)
            out.set("suggestedFileName", *r.suggestedFileName);
        if (r.message)
            out.set("message", *r.message);
    }

    void WriteErrorJson(Poco::JSON::Object& out, const ApiError& e)
    {
        Poco::JSON::Object err;
        err.set("code", e.code);
        err.set("message", e.message);
        if (e.win32)
            err.set("win32", static_cast<Poco::Int64>(*e.win32));
        out.set("error", err);
    }
}
