#ifndef METRICS_HPP
#define METRICS_HPP
#include <utility>
#include <memory>
#include <string>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/exporters/otlp/otlp_http.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>
#ifdef WITH_OTLP_GRPC
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h>
#endif
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/meter_context.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/provider.h>
#include "otelOptions.hpp"

namespace
{

bool metricsInitialized{false};

opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    receivedPacketsCounter;
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    invalidPacketsCounter;
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    importOverflowPacketsCounter;
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    invalidAccessCounter;
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    invalidRequestCounter;
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    serverErrorCounter;
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
    successfulRPCCounter;

void initializeHTTP(
    const bool exportMetrics,
    // NOLINTNEXTLINE(misc-include-cleaner)
    const auto &otelHTTPMetricsOptions)
{
    if (!exportMetrics){return;}
    namespace otel = opentelemetry;
    otel::exporter::otlp::OtlpHttpMetricExporterOptions exporterOptions;
    exporterOptions.url = otelHTTPMetricsOptions.url
                        + otelHTTPMetricsOptions.suffix;
    //exporterOptions.console_debug = debug != "" && debug != "0" && debug != "no";
    exporterOptions.content_type
        = otel::exporter::otlp::HttpRequestContentType::kBinary;

    auto exporter
        = otel::exporter::otlp::OtlpHttpMetricExporterFactory::Create(
             exporterOptions);
    // Initialize and set the global MeterProvider
    otel::sdk::metrics::PeriodicExportingMetricReaderOptions readerOptions;
    readerOptions.export_interval_millis
        = otelHTTPMetricsOptions.exportInterval;
    readerOptions.export_timeout_millis
        = otelHTTPMetricsOptions.exportTimeOut;

    auto reader
        = otel::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
             std::move(exporter),
             readerOptions);

    auto context = otel::sdk::metrics::MeterContextFactory::Create();
    context->AddMetricReader(std::move(reader));

    auto metricsProvider
        = otel::sdk::metrics::MeterProviderFactory::Create(
             std::move(context));

    const std::shared_ptr<otel::metrics::MeterProvider>
        provider(std::move(metricsProvider));
    otel::sdk::metrics::Provider::SetMeterProvider(provider);
    metricsInitialized = true;
}

#ifdef WITH_OTLP_GRPC
void initializeGRPC(
    const bool exportMetrics,
    // NOLINTNEXTLINE(misc-include-cleaner)
    const auto &otelGRPCMetricsOptions)
{
    if (!exportMetrics){return;}
    namespace otel = opentelemetry;
    otel::exporter::otlp::OtlpGrpcMetricExporterOptions exporterOptions;
    exporterOptions.endpoint = otelGRPCMetricsOptions.url;
    exporterOptions.use_ssl_credentials = false;
    if (!otelGRPCMetricsOptions.certificatePath.empty())
    {   
        exporterOptions.use_ssl_credentials = true;
        exporterOptions.ssl_credentials_cacert_path
           = otelGRPCMetricsOptions.certificatePath;
    }   
    auto exporter
        = otel::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(
             exporterOptions);

    // Initialize and set the global MeterProvider
    otel::sdk::metrics::PeriodicExportingMetricReaderOptions readerOptions;
    readerOptions.export_interval_millis
        = otelGRPCMetricsOptions.exportInterval;
    readerOptions.export_timeout_millis
        = otelGRPCMetricsOptions.exportTimeOut;

    auto reader
        = otel::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
             std::move(exporter),
             readerOptions);

    auto context = otel::sdk::metrics::MeterContextFactory::Create();
    context->AddMetricReader(std::move(reader));

    auto metricsProvider
        = otel::sdk::metrics::MeterProviderFactory::Create(
             std::move(context));

    const std::shared_ptr<otel::metrics::MeterProvider>
        provider(std::move(metricsProvider));
    otel::sdk::metrics::Provider::SetMeterProvider(provider);
    metricsInitialized = true;
}
#endif

}

namespace UDataPacketCacheService::Metrics
{

void initialize(
    // NOLINTNEXTLINE(misc-include-cleaner)
    const ProgramOptions &options)
{
    if (options.exportMetricsWithHTTP)
    {
        return ::initializeHTTP(options.exportMetrics,
                                options.otelHTTPMetricsOptions);
    }
    else
    {
#ifdef WITH_OTLP_GRPC
        return ::initializeGRPC(options.exportMetrics,
                                options.otelGRPCMetricsOptions);
#else
        throw std::runtime_error("Recompile with Conan and OTLP_GRPC");
#endif
    }
}

void cleanup()
{
    if (metricsInitialized)
    {
        const std::shared_ptr<opentelemetry::metrics::MeterProvider> none;
        opentelemetry::sdk::metrics::Provider::SetMeterProvider(none);
    }
    metricsInitialized = false;
}

void observeNumberOfPacketsReceived(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {   
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        auto &instance = MetricsSingleton::getInstance();
        auto value = instance.getPacketsReceivedCount();
        observer->Observe(value);
    }   
}

void observeNumberOfInvalidPacketsReceived(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {   
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        auto &instance = MetricsSingleton::getInstance();
        auto value = instance.getInvalidPacketsReceivedCount();
        observer->Observe(value);
    }
}

void observeNumberOfOverflowPackets(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <   
            opentelemetry::nostd::shared_ptr
            <   
                opentelemetry::metrics::ObserverResultT<int64_t>
            >   
        > (observerResult))
    {   
        auto observer = opentelemetry::nostd::get
        <   
            opentelemetry::nostd::shared_ptr
            <   
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        auto &instance = MetricsSingleton::getInstance();
        auto value = instance.getImportOverflowPacketCount();
        observer->Observe(value);
    }   
}

void observeNumberOfInvalidAccesses(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {   
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        auto &instance = MetricsSingleton::getInstance();
        auto value = instance.getInvalidAccessCount();
        observer->Observe(value);
    }
}

void observeNumberOfInvalidRequests(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {   
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        auto &instance = MetricsSingleton::getInstance();
        auto value = instance.getInvalidRequestCount();
        observer->Observe(value);
    }
}

void observeNumberOfServerErrors(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {   
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        auto &instance = MetricsSingleton::getInstance();
        auto value = instance.getServerErrorCount();
        observer->Observe(value);
    }   
}

void observeNumberOfSuccesses(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {   
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        auto &instance = MetricsSingleton::getInstance();
        auto value = instance.getSuccessfulRPCCount();
        observer->Observe(value);
    }   
}


}

#endif
