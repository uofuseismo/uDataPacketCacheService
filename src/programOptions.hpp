#ifndef PROGRAM_OPTIONS_HPP
#define PROGRAM_OPTIONS_HPP
#include <chrono>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <string>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include "uDataPacketCacheService/grpcServerOptions.hpp"
#include "uDataPacketCacheService/grpcClientOptions.hpp"
#include "otelOptions.hpp"

#define APPLICATION_NAME "uDataPacketCacheService"

namespace
{

struct ProgramOptions
{
    UDataPacketCacheService::OTelOptions::HTTPMetrics otelHTTPMetricsOptions;
    UDataPacketCacheService::OTelOptions::HTTPLog otelHTTPLogOptions;
    UDataPacketCacheService::OTelOptions::GRPCMetrics otelGRPCMetricsOptions;
    UDataPacketCacheService::OTelOptions::GRPCLog otelGRPCLogOptions;
    std::string applicationName{APPLICATION_NAME};
    std::chrono::seconds printSummaryInterval{std::chrono::minutes {15}};
    int verbosity{3};
    bool exportLogs{false};
    bool exportLogsWithHTTP{true};
    bool exportMetrics{false};
    bool exportMetricsWithHTTP{true};
};

std::pair<std::filesystem::path, bool>
    parseCommandLineOptions(int argc, char *argv[])
{
    std::filesystem::path iniFile;
    boost::program_options::options_description desc(R"""(
The uDataPacketCacheService is a high-speed, in-memory utility for serving
data packets to (near) real-time applications.

    uDataPacketCacheService --ini=service.ini

Allowed options)""");
    desc.add_options()
        ("help", "Produces this help message")
        ("ini",  boost::program_options::value<std::string> (), 
                 "The initialization file for this executable");
    boost::program_options::variables_map vm; 
    //NOLINTBEGIN(misc-include-cleaner)
    auto parsedMap
        = boost::program_options::parse_command_line(argc, argv, desc);
    //NOLINTEND(misc-include-cleaner)
    boost::program_options::store(parsedMap, vm);
    boost::program_options::notify(vm);
    if (vm.count("help"))
    {   
        std::cout << desc << "\n";
        return {iniFile, true};
    }   
    if (vm.count("ini"))
    {   
        iniFile = vm["ini"].as<std::string> (); 
        if (!std::filesystem::exists(iniFile))
        {
            throw std::runtime_error("Initialization file: "
                                   + std::string {iniFile}
                                   + " does not exist");
        }
    }   
    return {iniFile, false};
}

[[nodiscard]] std::string
loadStringFromFile(const std::filesystem::path &path)
{
    std::string result;
    if (!std::filesystem::exists(path)){return result;}
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open " + path.string());
    }   
    std::stringstream sstr;
    sstr << file.rdbuf();
    file.close(); 
    result = sstr.str();
    return result;
}

[[nodiscard]]
std::pair<std::chrono::milliseconds, std::chrono::milliseconds>
getOTelMetricsIntervalAndTimeOut(
    boost::property_tree::ptree &propertyTree,
    const std::string &section,
    const std::chrono::milliseconds &defaultExportInterval,
    const std::chrono::milliseconds &defaultExportTimeOut)
{
    int64_t exportInterval = defaultExportInterval.count();
    exportInterval
        = propertyTree.get<int64_t> (
            section + ".exportIntervalInMilliSeconds",
            exportInterval);
    if (exportInterval <= 0)
    {
        throw std::runtime_error("Export interval must be positive");
    }
    int64_t exportTimeOut = defaultExportTimeOut.count();
    exportTimeOut
        = propertyTree.get<int64_t> (
            section + ".exportTimeOutInMilliSeconds",
            exportTimeOut);
    if (exportTimeOut <= 0)
    {
        throw std::invalid_argument("Export time out must be positive");
    }
    return std::pair {std::chrono::milliseconds {exportInterval},
                      std::chrono::milliseconds {exportTimeOut}};
}

[[nodiscard]]
std::string getOTelCollectorURL(boost::property_tree::ptree &propertyTree,
                                const std::string &section)
{
    std::string result;
    const std::string otelCollectorHost
        = propertyTree.get<std::string> (section + ".host", "");
    const uint16_t otelCollectorPort
        = propertyTree.get<uint16_t> (section + ".port", 4218);
    if (!otelCollectorHost.empty())
    {
        result = otelCollectorHost + ":"
               + std::to_string(otelCollectorPort);
    }
    return result;
}

[[nodiscard]] UDataPacketCacheService::GRPCServerOptions
    getGRPCServerOptions(
    const boost::property_tree::ptree &propertyTree,
    const std::string &section)
{
    UDataPacketCacheService::GRPCServerOptions options;

    auto host
        = propertyTree.get<std::string> (section + ".host",
                                         options.getHost());
    if (host.empty())
    {
        throw std::runtime_error(section + ".host is empty");
    }
    options.setHost(host);

    uint16_t port{50000};
    options.setPort(port);

    port = propertyTree.get<uint16_t> (section + ".port", options.getPort());
    options.setPort(port);

    auto serverKey
        = propertyTree.get<std::string> (section + ".clientKey", "");
    auto serverCertificate
        = propertyTree.get<std::string> (section + ".serverCertificate", "");
    if (!serverKey.empty() && !serverCertificate.empty())
    {
        if (!std::filesystem::exists(serverKey))
        {
            throw std::invalid_argument("gRPC server Key file "
                                      + serverKey
                                      + " does not exist");
        }
        if (!std::filesystem::exists(serverCertificate))
        {
            throw std::invalid_argument("gRPC server certificate file "
                                      + serverCertificate
                                      + " does not exist");
        }
        options.setServerKey(loadStringFromFile(serverKey));
        options.setServerCertificate(loadStringFromFile(serverCertificate));
    }

    auto accessToken
        = propertyTree.get_optional<std::string> (section + ".accessToken");
    if (accessToken)
    {
        if (options.getServerCertificate() == std::nullopt)
        {
            throw std::invalid_argument(
                "Must set server certificate to use access token");
        }
        options.setAccessToken(*accessToken);
    }

    auto clientCertificate
        = propertyTree.get<std::string> (section + ".clientCertificate", "");
    if (!clientCertificate.empty())
    {
        if (!std::filesystem::exists(clientCertificate))
        {
            throw std::invalid_argument("gRPC client certificate file "
                                      + clientCertificate
                                      + " does not exist");
        }
        options.setClientCertificate(loadStringFromFile(clientCertificate));
    }

    auto enableReflection
        = propertyTree.get<bool> (section + ".enableReflection", false);
    options.disableReflection();
    if (enableReflection){options.enableReflection();}

    return options;
}

[[nodiscard]] UDataPacketCacheService::GRPCClientOptions
    getGRPCClientOptions(
    const boost::property_tree::ptree &propertyTree,
    const std::string &section)
{
    UDataPacketCacheService::GRPCClientOptions options;

    auto host
        = propertyTree.get<std::string> (section + ".host",
                                         options.getHost());
    if (host.empty())
    {   
        throw std::runtime_error(section + ".host is empty");
    }   
    options.setHost(host);

    uint16_t port{50000};
    options.setPort(port);

    port = propertyTree.get<uint16_t> (section + ".port", options.getPort());
    options.setPort(port); 

    auto serverCertificate
        = propertyTree.get<std::string> (section + ".serverCertificate", "");
    if (!serverCertificate.empty())
    {   
        if (!std::filesystem::exists(serverCertificate))
        {
            throw std::invalid_argument("gRPC server certificate file "
                                      + serverCertificate
                                      + " does not exist");
        }
        options.setServerCertificate(loadStringFromFile(serverCertificate));
    }   

    auto accessToken
        = propertyTree.get_optional<std::string> (section + ".accessToken");
    if (accessToken)
    {   
        if (options.getServerCertificate() == std::nullopt)
        {
            throw std::invalid_argument(
                "Must set server certificate to use access token");
        }
        options.setAccessToken(*accessToken);
    }

    auto clientKey
        = propertyTree.get<std::string> (section + ".clientKey", "");
    auto clientCertificate
        = propertyTree.get<std::string> (section + ".clientCertificate", "");
    if (!clientKey.empty() && !clientCertificate.empty())
    {
        if (!std::filesystem::exists(clientKey))
        {
            throw std::invalid_argument("gRPC client key file "
                                      + clientKey
                                      + " does not exist");
        }
        if (!std::filesystem::exists(clientCertificate))
        {
            throw std::invalid_argument("gRPC client certificate file "
                                      + clientCertificate
                                      + " does not exist");
        }
        options.setClientKey(loadStringFromFile(clientKey));
        options.setClientCertificate(loadStringFromFile(clientCertificate));
    }
    return options;
}

::ProgramOptions parseIniFile(const std::filesystem::path &iniFile)
{
    ::ProgramOptions options;
    if (!std::filesystem::exists(iniFile)){return options;}
    // Parse the initialization file
    boost::property_tree::ptree propertyTree;
    boost::property_tree::ini_parser::read_ini(iniFile, propertyTree);

    // Application name
    options.applicationName
        = propertyTree.get<std::string> ("General.applicationName",
                                         options.applicationName);
    if (options.applicationName.empty())
    {
        options.applicationName = APPLICATION_NAME;
    }
    options.verbosity
        = propertyTree.get<int> ("General.verbosity", options.verbosity);

    auto summaryIntervalInMinutes
        = static_cast<int> (options.printSummaryInterval.count());
    summaryIntervalInMinutes
        = propertyTree.get<int> ("General.printSummaryIntervalInMinutes",
                                 summaryIntervalInMinutes);
    options.printSummaryInterval
        = std::chrono::minutes {summaryIntervalInMinutes};

    return options;
}

}

#endif
