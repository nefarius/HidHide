#include "DiagnosticsTraceHandler.hpp"

#include "DiagnosticsTraceService.hpp"
#include "HidHideDiagnosticsJson.hpp"

#include <Poco/Exception.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/URI.h>
#include <Poco/UnicodeConverter.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace
{
    constexpr const char* kApiTraceBase = "/api/v1/diagnostics/trace";

    std::string NormalizePath(std::string path)
    {
        while (!path.empty() && path.back() == '/')
            path.pop_back();
        return path;
    }

    void SendJson(Poco::Net::HTTPServerResponse& response,
                  const int status,
                  const Poco::JSON::Object& obj)
    {
        response.setStatus(static_cast<Poco::Net::HTTPResponse::HTTPStatus>(status));
        response.setContentType("application/json");
        std::ostream& out = response.send();
        Poco::JSON::Stringifier::stringify(obj, out);
    }

    void SendError(Poco::Net::HTTPServerResponse& response,
                   const int status,
                   const hidhide::diag::ApiError& err)
    {
        Poco::JSON::Object root;
        hidhide::diag::WriteErrorJson(root, err);
        SendJson(response, status, root);
    }

    std::string WideToUtf8(const std::wstring& w)
    {
        std::string out;
        Poco::UnicodeConverter::toUTF8(w, out);
        return out;
    }

    class NotFoundHandler : public Poco::Net::HTTPRequestHandler
    {
    public:
        void handleRequest(Poco::Net::HTTPServerRequest& request,
                           Poco::Net::HTTPServerResponse& response) override
        {
            static_cast<void>(request);
            hidhide::diag::ApiError err;
            err.code = "not_found";
            err.message = "The requested resource was not found.";
            SendError(response, 404, err);
        }
    };

    class DiagnosticsTraceHandler : public Poco::Net::HTTPRequestHandler
    {
    public:
        void handleRequest(Poco::Net::HTTPServerRequest& request,
                           Poco::Net::HTTPServerResponse& response) override
        {
            const std::string method = request.getMethod();
            Poco::URI uri(request.getURI());
            const std::string p = NormalizePath(uri.getPath());

            if (p == kApiTraceBase && method == Poco::Net::HTTPRequest::HTTP_GET)
                return HandleGetStatus(response);

            if (p == kApiTraceBase && method == Poco::Net::HTTPRequest::HTTP_DELETE)
                return HandleDelete(response);

            if (p == std::string(kApiTraceBase) + "/start" && method == Poco::Net::HTTPRequest::HTTP_POST)
                return HandlePostStart(request, response);

            if (p == std::string(kApiTraceBase) + "/stop" && method == Poco::Net::HTTPRequest::HTTP_POST)
                return HandlePostStop(response);

            if (p == std::string(kApiTraceBase) + "/etl" && method == Poco::Net::HTTPRequest::HTTP_GET)
                return HandleGetEtl(response);

            hidhide::diag::ApiError err;
            err.code = "not_found";
            err.message = "Unknown diagnostics route.";
            SendError(response, 404, err);
        }

    private:

        static void HandleGetStatus(Poco::Net::HTTPServerResponse& response)
        {
            const auto r = DiagnosticsTraceService::Instance().GetStatus();
            if (!r)
                return SendError(response, 500, r.error());

            Poco::JSON::Object root;
            hidhide::diag::WriteStatusJson(root, *r);
            SendJson(response, 200, root);
        }

        void HandlePostStart(Poco::Net::HTTPServerRequest& request,
                             Poco::Net::HTTPServerResponse& response)
        {
            std::string body;
            Poco::StreamCopier::copyToString(request.stream(), body);

            hidhide::diag::TraceStartRequest parsed;
            try
            {
                parsed = hidhide::diag::ParseStartBody(body);
            }
            catch (const Poco::Exception& ex)
            {
                hidhide::diag::ApiError err;
                err.code = "bad_request";
                err.message = std::string("Invalid JSON: ") + ex.what();
                return SendError(response, 400, err);
            }

            const auto r = DiagnosticsTraceService::Instance().Start(parsed);
            if (!r)
            {
                const std::string& code = r.error().code;
                int status = 400;
                if (code == "recording_in_progress")
                    status = 409;
                else if (code == "trace_start_failed")
                    status = 500;
                return SendError(response, status, r.error());
            }

            Poco::JSON::Object root;
            root.set("ok", true);
            root.set("message", "Recording started.");
            SendJson(response, 200, root);
        }

        static void HandlePostStop(Poco::Net::HTTPServerResponse& response)
        {
            const auto r = DiagnosticsTraceService::Instance().Stop();
            if (!r)
            {
                const int status = r.error().code == "not_recording" ? 400 : 500;
                return SendError(response, status, r.error());
            }

            Poco::JSON::Object root;
            root.set("ok", true);
            root.set("suggestedFileName", r->suggestedFileName);
            if (r->message)
                root.set("message", *r->message);
            SendJson(response, 200, root);
        }

        static void HandleGetEtl(Poco::Net::HTTPServerResponse& response)
        {
            const auto pathW = DiagnosticsTraceService::Instance().GetEtlPathForDownload();
            if (!pathW)
                return SendError(response, 400, pathW.error());

            std::ifstream in(fs::path(*pathW), std::ios::binary);
            if (!in)
            {
                hidhide::diag::ApiError err;
                err.code = "open_failed";
                err.message = "Could not open capture file for reading.";
                return SendError(response, 500, err);
            }

            const std::string attachName = WideToUtf8(fs::path(*pathW).filename().wstring());

            response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
            response.setContentType("application/octet-stream");
            response.set("Content-Disposition", std::string("attachment; filename=\"") + attachName + "\"");

            std::ostream& out = response.send();
            out << in.rdbuf();
        }

        static void HandleDelete(Poco::Net::HTTPServerResponse& response)
        {
            const auto r = DiagnosticsTraceService::Instance().DiscardCapture();
            if (!r)
                return SendError(response, 500, r.error());

            Poco::JSON::Object root;
            root.set("ok", true);
            root.set("message", "Capture discarded.");
            SendJson(response, 200, root);
        }
    };
}

Poco::Net::HTTPRequestHandler* DiagnosticsHttpHandlerFactory::createRequestHandler(
    const Poco::Net::HTTPServerRequest& request)
{
    Poco::URI uri(request.getURI());
    const std::string path = NormalizePath(uri.getPath());

    if (path.rfind("/api/v1/diagnostics/", 0) == 0)
        return new DiagnosticsTraceHandler();

    return new NotFoundHandler();
}
