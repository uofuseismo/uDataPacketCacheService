#ifndef UDATA_PACKET_CACHE_SERVICE_SERVICE_OPTIONS_HPP
#define UDATA_PACKET_CACHE_SERVICE_SERVICE_OPTIONS_HPP
#include <chrono>
#include <memory>
namespace UDataPacketCacheService
{
 class GRPCServerOptions;
}
namespace UDataPacketCacheService
{
/*!
 * @class ServiceOptions "serviceOptions.hpp"
 * @brief Defines the options for the service component of the application.
 * @copyright Ben Baker (University of Utah) distributed under the MIT NO AI license.
 */
class ServiceOptions
{
public:
    enum class CompressionAlgorithm
    {
        None,
        Deflate,
        GZIP
    };
public:
    /// @brief Constructor.
    ServiceOptions();
    /// @brief Copy constructor.
    ServiceOptions(const ServiceOptions &options);
    /// @brief Move constructor.
    ServiceOptions(ServiceOptions &&options) noexcept;

    /// @brief Sets the gRPC server options.
    void setGRPCOptions(const GRPCServerOptions &options);
    /// @result The server options.
    [[nodiscard]] GRPCServerOptions getGRPCOptions() const;
    /// @result True indicates the gPRC options were set.
    [[nodiscard]] bool hasGRPCOptions() const noexcept;

    /// @brief Sets the maximum request queue size.
    /// @param[in] maximumRequestQueueSize   The maximum request queue size.
    /// @throws std::invalid_argument if this is not positive.
    void setMaximumRequestQueueSize(int maximumRequestQueueSize); 
    /// @result The maximum request queue size.  After this point the service
    ///         returns UNAVAILABLE gRPC errors.
    /// @note By default this is 32.
    [[nodiscard]] int getMaximumRequestQueueSize() const noexcept;

    /// @brief Sets the compression algorithm for the RPC that
    ///        serves time series data.
    void setCompressionAlgorithm(CompressionAlgorithm algorithm) noexcept;
    /// @result The compression algorithm for the time series serving RPC.
    [[nodiscard]] CompressionAlgorithm getCompressionAlgorithm() const noexcept;

    /// @brief Sets the maximum request message size (in bytes).
    /// @param[in] maximumMessageSize   The maximum message size in bytes.
    /// @throws std::invalid_argument if this is not positive.
    void setMaximumRequestMessageSizeInBytes(int maximumMessageSize);
    /// @result The maximum request message size in bytes.  
    /// @note By default this is 1024. 
    [[nodiscard]] int getMaximumRequestMessageSizeInBytes() const noexcept;
 
    /// @brief Sets the maximum connection age.  After this amount of time
    ///        the connection is terminated.
    void setMaximumConnectionAge(const std::chrono::milliseconds &maxConnectionAge);
    /// @result The maximum connection age.
    [[nodiscard]] std::chrono::milliseconds getMaximumConnectionAge() const noexcept;

    /// @brief After the maximum connection age is hit, this is the amount of
    ///        of time to wait for the RPC to finish.
    void setMaximumConnectionAgeGracePeriod(const std::chrono::milliseconds &maxGracePeriod);
    /// @result The maximum grace period for RPCs belonging to connections
    ///         that look stale.
    [[nodiscard]] std::chrono::milliseconds getMaximumConnectionAgeGracePeriod() const noexcept;

    /// @brief Sets the maximum number of concurrent connection after which 
    ///        point the service is over-saturated.
    void setMaximumNumberOfConcurrentStreams(const int maxStreams);
    /// @result The maximum number of concurrent connections.
    [[nodiscard]] int getMaximumNumberOfConcurrentStreams() const noexcept;

    /// @brief Sets the maximum request duration.
    //void setMaximumRequestDuration(const std::chrono::milliseconds &duration);
    /// @result After this amount of time requests
    //std::chrono::milliseconds getMaximumRequestDuration() const noexcept;

    /// @brief Copy assignment.
    /// @result A deep copy of the input options.
    ServiceOptions& operator=(const ServiceOptions &options);
    /// @brief Move assignment.
    /// @result The memory from options moved to this.
    ServiceOptions& operator=(ServiceOptions &&options) noexcept;
    /// @brief Destructor.
    ~ServiceOptions();
private:
    class ServiceOptionsImpl;
    std::unique_ptr<ServiceOptionsImpl> pImpl;
};
}
#endif
