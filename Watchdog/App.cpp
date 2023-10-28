#include "App.hpp"
#include <fstream>

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
    std::ofstream file(R"(D:\StartUp_Skaner\Projekty\poco-windows-service\log.txt)");
    file << "The service is running";
    file.flush();
    file.close();

    while (true)
    {
    }
}
