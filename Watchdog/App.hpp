#ifndef POCO_WINDOWS_SERVICE_APP_H
#define POCO_WINDOWS_SERVICE_APP_H

#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING

#include <Poco/Util/ServerApplication.h>

class App : public Poco::Util::ServerApplication
{
public:
    App() = default;

    ~App() override = default;

protected:
    void initialize(Application& self) override;

    void uninitialize() override;

    int main(const std::vector<std::string>& args) override;
};


#endif //POCO_WINDOWS_SERVICE_APP_H
