#ifndef UDATA_PACKET_CACHE_SERVICE_METRICS_SINGLETON_HPP
#define UDATA_PACKET_CACHE_SERVICE_METRICS_SINGLETON_HPP
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
    /// @brief Resets the counters and clears maps.  This is useful for unit tests.
    void resetCounters();
private:
    MetricsSingleton() = default;
    ~MetricsSingleton() = default;
};
/// @brief Convenience function to instantiate the metrics singleton once at the
///        beginning of the main program.
void initializeMetricsSingleton();
}
#endif
