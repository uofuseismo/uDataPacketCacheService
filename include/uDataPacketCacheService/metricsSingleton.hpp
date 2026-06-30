#ifndef UDATA_PACKET_CACHE_SERVICE_METRICS_SINGLETON_HPP
#define UDATA_PACKET_CACHE_SERVICE_METRICS_SINGLETON_HPP
#include <atomic>
#include <cstdint>
#include <memory>
namespace UDataPacketCacheService
{
/// @class MetricsSingleton metricsSingleton.hpp
/// @brief This class allows metrics to be sent to the OTel collector.
/// @copyright Ben Baker (University of Utah) distributed under the MIT
///            NO AI license.
class MetricsSingleton
{
public:
    /// @result An instance of the singleton.  This is thread-safe.
    [[maybe_unused]] static MetricsSingleton &getInstance();

    /// @brief Increments the number of packets received counter.
    void incrementPacketsReceivedCounter() noexcept;
    /// @result The number of packets received.
    [[nodiscard]] int64_t getPacketsReceivedCount() const noexcept;

    /// @brief Increments the number of invalid packets received counter.
    void incrementInvalidPacketsReceivedCounter() noexcept;
    /// @result The number of invalid packets received.
    [[nodiscard]] int64_t getInvalidPacketsReceivedCount() const noexcept;

    /// @brief Increments the number of imported packets that were dropped 
    ///        because the import queue is full.  This indicates
    ///        that the thread adding packets to the circular buffer map
    ///        is moving too slow. 
    void incrementImportOverflowPacketCounter() noexcept;
    /// @result The number of imported packets dropped because the import queue
    ///         is full.
    [[nodiscard]] int64_t getImportOverflowPacketCount() const noexcept;

    /// @brief Increments the number of invalid access attempts.
    void incrementInvalidAccessCounter() noexcept;
    /// @result The number of invalid accesses.
    [[nodiscard]] int64_t getInvalidAccessCount() const noexcept;

    /// @brief Increments the number of invalid requests.
    void incrementInvalidRequestCounter();
    /// @result The number of invalid requests.
    [[nodiscard]] int64_t getInvalidRequestsCount() const noexcept;

    /// @brief Increments the number of server side errors.
    void incrementServerErrorCounter() noexcept;
    /// @result The number of server errors.
    [[nodiscard]] int64_t getServerErrorCount() const noexcept;

    /// @brief Increments the number of successful RPCs.
    void incrementSuccessfulRPCCounter() noexcept;
    /// @result The number of successful RPCs.
    [[nodiscard]] int64_t getSuccessfulRCPCount() const noexcept;

    /// @brief Resets the counters and clears maps.  This is useful for unit tests.
    void resetCounters();
private:
    MetricsSingleton() = default;
    ~MetricsSingleton() = default;
    std::atomic<int64_t> mPacketsReceivedCounter{0};
    std::atomic<int64_t> mInvalidPacketsReceivedCounter{0};
    std::atomic<int64_t> mImportOverflowPacketCounter{0};
    std::atomic<int64_t> mSuccessfulRPCCounter{0};
    std::atomic<int64_t> mInvalidAccessCounter{0};
    std::atomic<int64_t> mInvalidRequestsCounter{0};
    std::atomic<int64_t> mServerErrorCounter{0};
};
/// @brief Convenience function to instantiate the metrics singleton once at the
///        beginning of the main program.
void initializeMetricsSingleton();
}
#endif
