#include <algorithm>
#include <atomic>
#include <cmath>
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
    mSuccessfulRPCCounter.store(0);
    mInvalidAccessCounter.store(0);
    mInvalidRequestsCounter.store(0);
    mServerErrorCounter.store(0);
    mNumberOfClients.store(0);
    mMaximumNumberOfClients = 0;
}

void UDataPacketCacheService::initializeMetricsSingleton()
{
    MetricsSingleton::getInstance();
}

/// Number of packets received
void MetricsSingleton::incrementPacketsReceivedCounter() noexcept
{
    mPacketsReceivedCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getPacketsReceivedCount() const noexcept
{
    return mPacketsReceivedCounter.load(std::memory_order_relaxed);
}

/// Number of invalid packets received
void MetricsSingleton::incrementInvalidPacketsReceivedCounter() noexcept
{
    mInvalidPacketsReceivedCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getInvalidPacketsReceivedCount() const noexcept
{
    return mInvalidPacketsReceivedCounter.load(std::memory_order_relaxed);
}

/// Overflow on the import queue
void MetricsSingleton::incrementImportOverflowPacketCounter() noexcept
{
    mImportOverflowPacketCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getImportOverflowPacketCount() const noexcept
{
    return mImportOverflowPacketCounter.load(std::memory_order_relaxed);
}

/// Invalid access
void MetricsSingleton::incrementInvalidAccessCounter() noexcept
{
    mInvalidAccessCounter.fetch_add(1, std::memory_order_relaxed);
}
    
int64_t MetricsSingleton::getInvalidAccessCount() const noexcept
{
    return mInvalidAccessCounter.load(std::memory_order_relaxed);
}

// Invalid request
void MetricsSingleton::incrementInvalidRequestCounter()
{
     mInvalidRequestsCounter.fetch_add(1, std::memory_order_relaxed);
}
    
int64_t MetricsSingleton::getInvalidRequestCount() const noexcept
{
    return mInvalidRequestsCounter.load(std::memory_order_relaxed);
}

// Server error
void MetricsSingleton::incrementServerErrorCounter() noexcept
{
    mServerErrorCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getServerErrorCount() const noexcept
{
    return mServerErrorCounter.load(std::memory_order_relaxed);
}

// Successful RPC
void MetricsSingleton::incrementSuccessfulRPCCounter() noexcept
{
    mSuccessfulRPCCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getSuccessfulRPCCount() const noexcept
{
    return mSuccessfulRPCCounter.load(std::memory_order_relaxed);
}

/// Utilization
double MetricsSingleton::getServiceUtilization() const noexcept
{
    auto nClients =  getNumberOfClients(); 
    auto maxClients = getMaximumNumberOfClients();
    auto utilization
        = static_cast<double> (std::max(0, nClients))
         /std::max(1, maxClients);
    return std::max(0.0, std::min(1.0, utilization));
}

/// Clients
int MetricsSingleton::getMaximumNumberOfClients() const noexcept
{
    return mMaximumNumberOfClients;
}

void MetricsSingleton::incrementNumberOfClients() noexcept
{
    mNumberOfClients.fetch_add(1, std::memory_order::seq_cst);
}

void MetricsSingleton::decrementNumberOfClients() noexcept
{
    auto previous = mNumberOfClients.fetch_sub(1, std::memory_order::seq_cst);
    if (previous <= 0)
    {
        mNumberOfClients.store(0, std::memory_order::seq_cst);
    } 
}

int MetricsSingleton::getNumberOfClients() const noexcept
{
    return mNumberOfClients.load(std::memory_order_relaxed);
}
