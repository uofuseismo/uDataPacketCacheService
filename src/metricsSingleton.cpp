#include <atomic>
#include <cstdint>
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

void MetricsSingleton::resetCounters()
{
    mPacketsReceivedCounter.store(0);
    mInvalidPacketsReceivedCounter.store(0);
    mImportOverflowPacketCounter.store(0);
}

void UDataPacketCacheService::initializeMetricsSingleton()
{
    MetricsSingleton::getInstance();
}

/// Number of packets received
void MetricsSingleton::incrementPacketsReceivedCounter()
{
    mPacketsReceivedCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getPacketsReceivedCount() const noexcept
{
    return mPacketsReceivedCounter.load(std::memory_order_relaxed);
}

/// Number of invalid packets received
void MetricsSingleton::incrementInvalidPacketsReceivedCounter()
{
    mInvalidPacketsReceivedCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getInvalidPacketsReceivedCount() const noexcept
{
    return mInvalidPacketsReceivedCounter.load(std::memory_order_relaxed);
}

/// Overflow on the import queue
void MetricsSingleton::incrementImportOverflowPacketCounter()
{
    mImportOverflowPacketCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getImportOverflowPacketCount() const noexcept
{
    return mImportOverflowPacketCounter.load(std::memory_order_relaxed);
}
