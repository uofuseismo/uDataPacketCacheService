#include <mutex>
#include "uDataPacketCacheService/metricsSingleton.hpp"

using namespace UDataPacketCacheService;

MetricsSingleton &MetricsSingleton::getInstance()
{
    std::mutex mutex;
    const std::scoped_lock lock{mutex};
    static MetricsSingleton instance;
    return instance;
}

void UDataPacketCacheService::initializeMetricsSingleton()
{
    MetricsSingleton::getInstance();
}

