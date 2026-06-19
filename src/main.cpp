#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "uDataPacketCacheService/service.hpp"
//#include "logger.hpp"

using namespace UDataPacketCacheService;

class Process
{
public:
    std::unique_ptr<Service> mService{nullptr}; 
};

int main()
{

    // Create the logger
    std::shared_ptr<spdlog::logger> logger{nullptr};
    try 
    {   
        //logger = UFilterPickerPickBroker::Logger::initialize(programOptions);
    }   
    catch (const std::exception &e) 
    {   
        //NOLINTNEXTLINE(misc-include-cleaner)
        auto consoleLogger = spdlog::stdout_color_st("console");
        SPDLOG_LOGGER_CRITICAL(consoleLogger,
                               "Failed to initialize logger because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }   

    return EXIT_SUCCESS;
}
