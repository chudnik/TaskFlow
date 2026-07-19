#include "taskflow/platform/structured_logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#if TASKFLOW_HAS_SPDLOG
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_sinks.h>
#endif

namespace taskflow::platform {
namespace {

[[nodiscard]] std::string escape_json(const std::string_view value) {
  std::ostringstream output;
  for (const char raw_character : value) {
    const auto character = static_cast<unsigned char>(raw_character);
    switch (character) {
    case '"':
      output << "\\\"";
      break;
    case '\\':
      output << "\\\\";
      break;
    case '\b':
      output << "\\b";
      break;
    case '\f':
      output << "\\f";
      break;
    case '\n':
      output << "\\n";
      break;
    case '\r':
      output << "\\r";
      break;
    case '\t':
      output << "\\t";
      break;
    default:
      if (character < 0x20) {
        output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
               << static_cast<unsigned int>(character) << std::dec;
      } else {
        output << static_cast<char>(character);
      }
    }
  }
  return output.str();
}

void append_string(std::ostringstream &output, const std::string_view key,
                   const std::string_view value) {
  output << "\"" << escape_json(key) << "\":\"" << escape_json(value) << "\"";
}

[[nodiscard]] std::string utc_timestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &time);
#else
  gmtime_r(&time, &utc);
#endif
  std::ostringstream output;
  output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3) << std::setfill('0')
         << milliseconds.count() << 'Z';
  return output.str();
}

void validate_context(std::string_view id, std::string_view operation) {
  if (id.empty() || operation.empty()) {
    throw std::invalid_argument{"correlation ID and operation must not be empty"};
  }
}

} // namespace

CorrelationContext CorrelationContext::request(std::string request_id, std::string route) {
  validate_context(request_id, route);
  return {
      .kind = CorrelationKind::request, .id = std::move(request_id), .operation = std::move(route)};
}

CorrelationContext CorrelationContext::job(std::string job_id, std::string job_type) {
  validate_context(job_id, job_type);
  return {.kind = CorrelationKind::job, .id = std::move(job_id), .operation = std::move(job_type)};
}

std::string format_json_log(const LogRecord &record) {
  std::ostringstream output;
  output << '{';
  append_string(output, "timestamp", record.timestamp);
  output << ',';
  append_string(output, "level", record.level);
  output << ',';
  append_string(output, "service", record.service);
  output << ',';
  append_string(output, "correlation_id", record.correlation.id);
  output << ',';
  append_string(output, record.correlation.kind == CorrelationKind::request ? "route" : "job_type",
                record.correlation.operation);
  output << ',';
  append_string(output, "outcome", record.outcome);
  output << ",\"latency_ms\":" << record.latency_ms << ',';
  append_string(output, "message", record.message);
  output << ",\"fields\":{";

  bool first = true;
  for (const auto &field : record.fields) {
    if (field.sensitivity == FieldSensitivity::personal_data) {
      continue;
    }
    if (!first) {
      output << ',';
    }
    append_string(output, field.key,
                  field.sensitivity == FieldSensitivity::secret ? "<redacted>" : field.value);
    first = false;
  }
  output << "}}";
  return output.str();
}

class StructuredLogger::Impl {
public:
  Impl(std::string service, const std::string &level) : service_(std::move(service)) {
    if (service_.empty()) {
      throw std::invalid_argument{"logger service must not be empty"};
    }
#if TASKFLOW_HAS_SPDLOG
    auto sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
    logger_ = std::make_shared<spdlog::logger>(service_, std::move(sink));
    logger_->set_pattern("%v");
    logger_->set_level(spdlog::level::from_str(level));
#else
    static_cast<void>(level);
#endif
  }

  void emit(const std::string_view level, const CorrelationContext &correlation,
            const std::string_view outcome, const std::uint64_t latency_ms,
            const std::string_view message, std::vector<LogField> fields) const {
    const auto json = format_json_log({.timestamp = utc_timestamp(),
                                       .level = std::string{level},
                                       .service = service_,
                                       .correlation = correlation,
                                       .outcome = std::string{outcome},
                                       .latency_ms = latency_ms,
                                       .message = std::string{message},
                                       .fields = std::move(fields)});
#if TASKFLOW_HAS_SPDLOG
    logger_->log(spdlog::level::from_str(std::string{level}), "{}", json);
#else
    std::cout << json << '\n';
#endif
  }

private:
  std::string service_;
#if TASKFLOW_HAS_SPDLOG
  std::shared_ptr<spdlog::logger> logger_;
#endif
};

StructuredLogger::StructuredLogger(std::string service, std::string level)
    : impl_(std::make_shared<Impl>(std::move(service), level)) {}
StructuredLogger::~StructuredLogger() = default;
StructuredLogger::StructuredLogger(const StructuredLogger &) = default;
StructuredLogger &StructuredLogger::operator=(const StructuredLogger &) = default;
StructuredLogger::StructuredLogger(StructuredLogger &&) noexcept = default;
StructuredLogger &StructuredLogger::operator=(StructuredLogger &&) noexcept = default;

void StructuredLogger::log(const std::string_view level, const CorrelationContext &correlation,
                           const std::string_view outcome, const std::uint64_t latency_ms,
                           const std::string_view message, std::vector<LogField> fields) const {
  impl_->emit(level, correlation, outcome, latency_ms, message, std::move(fields));
}

} // namespace taskflow::platform
