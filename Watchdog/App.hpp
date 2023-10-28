#ifndef POCO_WINDOWS_SERVICE_APP_H
#define POCO_WINDOWS_SERVICE_APP_H


#include <Poco/Util/ServerApplication.h>

class App : public Poco::Util::ServerApplication {

public:
    App() = default;

    ~App() = default;

protected:
    void initialize(Application &self);

    void uninitialize();

    int main(const std::vector<std::string> &args);


};


#endif //POCO_WINDOWS_SERVICE_APP_H