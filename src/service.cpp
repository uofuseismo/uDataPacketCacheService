#include <memory>
#include <spdlog/logger.h>
#include <uDataPacketCacheServiceAPI/v1/data_request.pb.h>
#include <uDataPacketCacheServiceAPI/v1/bulk_request.pb.h>
#include "uDataPacketCacheService/service.hpp"
#include "uDataPacketCacheService/serviceOptions.hpp"
#include "uDataPacketCacheService/circularBufferMap.hpp"

using namespace UDataPacketCacheService;

namespace
{

}

class Service::ServiceImpl
{
public:
    std::shared_ptr<CircularBufferMap> mCircularBufferMap{nullptr};
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
};

/// Destructor
Service::~Service() = default;
