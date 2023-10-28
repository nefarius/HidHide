#include "App.hpp"
#include <fstream>
#include "Util.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>

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
    auto debugSink = std::make_shared<spdlog::sinks::msvc_sink_mt>(false);
#if _DEBUG
    debugSink->set_level(spdlog::level::debug);
#else
    debugSink->set_level(spdlog::level::info);
#endif

    auto logger = std::make_shared<spdlog::logger>("HidHideWatchdog", debugSink);

#if _DEBUG
    spdlog::set_level(spdlog::level::debug);
    logger->flush_on(spdlog::level::debug);
#else
			logger->flush_on(spdlog::level::info);
#endif

    spdlog::set_default_logger(logger);

    while (true)
    {
    }
}
