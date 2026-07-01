#include <chrono>
#include <memory>
#include <stdexcept>
#include <utility>
#include "uDataPacketCacheService/serviceOptions.hpp"
#include "uDataPacketCacheService/grpcServerOptions.hpp"

using namespace UDataPacketCacheService;

class ServiceOptions::ServiceOptionsImpl
{
public:
    GRPCServerOptions mGRPCOptions; 
    std::chrono::milliseconds mMaximumConnectionAge{std::chrono::seconds {30}};
    std::chrono::milliseconds mMaximumGracePeriod{1};
    CompressionAlgorithm mCompressionAlgorithm{CompressionAlgorithm::Deflate};
    int mMaximumRequestQueueSize{32}; 
    int mMaximumRequestMessageSizeInBytes{1024};
    int mMaximumConcurrentStreams{64};
    bool mHasGRPCOptions{false};
};

/// Constructor
ServiceOptions::ServiceOptions() :
    pImpl(std::make_unique<ServiceOptionsImpl> ())
{
}

/// Copy constructor
ServiceOptions::ServiceOptions(const ServiceOptions &options)
{
    *this = options;
}

/// Move constructor
ServiceOptions::ServiceOptions(ServiceOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
ServiceOptions &ServiceOptions::operator=(const ServiceOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<ServiceOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
ServiceOptions &ServiceOptions::operator=(ServiceOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Destructor
ServiceOptions::~ServiceOptions() = default;

/// gRPC client options
void ServiceOptions::setGRPCOptions(const GRPCServerOptions &options)
{
    pImpl->mGRPCOptions = options;
    pImpl->mHasGRPCOptions = true;
}

GRPCServerOptions ServiceOptions::getGRPCOptions() const
{
    if (!hasGRPCOptions())
    {   
        throw std::runtime_error("gRPC service options not set");
    }   
    return pImpl->mGRPCOptions;
}

bool ServiceOptions::hasGRPCOptions() const noexcept
{
    return pImpl->mHasGRPCOptions;
}

/// Maximum request size
void ServiceOptions::setMaximumRequestQueueSize(const int maximumQueueSize)
{
    if (maximumQueueSize <= 0)
    {
        throw std::invalid_argument("Maximum queue size must be positive");
    }
    pImpl->mMaximumRequestQueueSize = maximumQueueSize;
}

int ServiceOptions::getMaximumRequestQueueSize() const noexcept
{
    return pImpl->mMaximumRequestQueueSize;
}

/// Maximum request message size
void ServiceOptions::setMaximumRequestMessageSizeInBytes(int maximumMessageSize)
{
    if (maximumMessageSize <= 0)
    {
        throw std::invalid_argument(
            "Maximum request message size must be positive");
    }
    pImpl->mMaximumRequestMessageSizeInBytes = maximumMessageSize;
}

int ServiceOptions::getMaximumRequestMessageSizeInBytes() const noexcept
{
    return pImpl->mMaximumRequestMessageSizeInBytes;
}

/// Maximum connection age
void ServiceOptions::setMaximumConnectionAge(
    const std::chrono::milliseconds &duration)
{
    if (duration.count() < 1)
    {
        throw std::invalid_argument("Max connection duration must be positive");
    }
    pImpl->mMaximumConnectionAge = duration;
}

std::chrono::milliseconds 
ServiceOptions::getMaximumConnectionAge() const noexcept
{
    return pImpl->mMaximumConnectionAge;
}

/// Maximum grace period
void ServiceOptions::setMaximumConnectionAgeGracePeriod(
    const std::chrono::milliseconds &period)
{
    if (period.count() < 0)
    {
        throw std::invalid_argument("Grace period must be non-negative");
    }
    pImpl->mMaximumGracePeriod = period;
}

std::chrono::milliseconds
ServiceOptions::getMaximumConnectionAgeGracePeriod() const noexcept
{
    return pImpl->mMaximumGracePeriod;
}

/// Maximum concurrent streams
void ServiceOptions::setMaximumNumberOfConcurrentStreams(const int maxStreams)
{
    if (maxStreams < 1)
    {
        throw std::invalid_argument(
           "Max number of concurrent streams must be positive");
    }
    pImpl->mMaximumConcurrentStreams = maxStreams;
}

int ServiceOptions::getMaximumNumberOfConcurrentStreams() const noexcept
{
    return pImpl->mMaximumConcurrentStreams;
}

/// Maximum number of concurrent streams
void ServiceOptions::setCompressionAlgorithm(
    const CompressionAlgorithm algorithm) noexcept
{
    pImpl->mCompressionAlgorithm = algorithm;
}

ServiceOptions::CompressionAlgorithm 
ServiceOptions::getCompressionAlgorithm() const noexcept
{
    return pImpl->mCompressionAlgorithm;
}
