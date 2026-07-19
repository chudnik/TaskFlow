#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace taskflow::platform {

enum class CorrelationKind { request, job };

struct CorrelationContext {
  CorrelationKind kind;
  std::string id;
  std::string operation;

  [[nodiscard]] static CorrelationContext request(std::string request_id, std::string route);
  [[nodiscard]] static CorrelationContext job(std::string job_id, std::string job_type);
};

enum class FieldSensitivity { public_value, secret, personal_data };

struct LogField {
  std::string key;
  std::string value;
  FieldSensitivity sensitivity{FieldSensitivity::public_value};
};

struct LogRecord {
  std::string timestamp;
  std::string level;
  std::string service;
  CorrelationContext correlation;
  std::string outcome;
  std::uint64_t latency_ms;
  std::string message;
  std::vector<LogField> fields;
};

[[nodiscard]] std::string format_json_log(const LogRecord &record);

class StructuredLogger {
public:
  StructuredLogger(std::string service, std::string level);
  ~StructuredLogger();
  StructuredLogger(const StructuredLogger &);
  StructuredLogger &operator=(const StructuredLogger &);
  StructuredLogger(StructuredLogger &&) noexcept;
  StructuredLogger &operator=(StructuredLogger &&) noexcept;

  void log(std::string_view level, const CorrelationContext &correlation, std::string_view outcome,
           std::uint64_t latency_ms, std::string_view message,
           std::vector<LogField> fields = {}) const;

private:
  class Impl;
  std::shared_ptr<Impl> impl_;
};

} // namespace taskflow::platform
