#include <string>
#include "uDataPacketCacheService/version.hpp"

using namespace UDataPacketCacheService;

int Version::getMajor() noexcept
{
    return uDataPacketCacheService_MAJOR;
}

int Version::getMinor() noexcept
{
    return uDataPacketCacheService_MINOR;
}

int Version::getPatch() noexcept
{
    return uDataPacketCacheService_PATCH;
}

//NOLINTBEGIN(bugprone-easily-swappable-parameters)
bool Version::isAtLeast(const int major, const int minor,
                        const int patch) noexcept
//NOLINTEND(bugprone-easily-swappable-parameters)
{
    if (uDataPacketCacheService_MAJOR < major){return false;}
    if (uDataPacketCacheService_MAJOR > major){return true;}
    if (uDataPacketCacheService_MINOR < minor){return false;}
    if (uDataPacketCacheService_MINOR > minor){return true;}
    if (uDataPacketCacheService_PATCH < patch){return false;}
    return true;
}

std::string Version::getVersion() noexcept
{
    std::string version{uDataPacketCacheService_VERSION};
    return version;
}

std::string Version::getTag() noexcept
{
    std::string tag{uDataPacketCacheService_GITTAG};
    return tag;
}

std::string Version::getVersionWithTag() noexcept
{
    auto tag = Version::getTag();
    if (tag.empty())
    {
        return Version::getVersion();
    }
    else
    {
        return Version::getVersion() + "-" + tag;
    }
}
