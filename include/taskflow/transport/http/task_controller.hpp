#pragma once

#include "taskflow/application/task_list.hpp"
#include "taskflow/application/tasks.hpp"
#include "taskflow/transport/http/identity_controller.hpp"

#include <string_view>

namespace taskflow::transport::http {

class TaskController {
public:
  explicit TaskController(const application::TaskUseCases &use_cases);
  TaskController(const application::TaskUseCases &use_cases,
                 const application::TaskListUseCase &list_use_case);

  [[nodiscard]] ControllerResponse create(const application::AuthenticatedPrincipal &actor,
                                          std::string_view json_body) const;
  [[nodiscard]] ControllerResponse read(const application::AuthenticatedPrincipal &actor,
                                        std::string_view task_id) const;
  [[nodiscard]] ControllerResponse update(const application::AuthenticatedPrincipal &actor,
                                          std::string_view task_id,
                                          std::string_view json_body) const;
  [[nodiscard]] ControllerResponse transition(const application::AuthenticatedPrincipal &actor,
                                              std::string_view task_id,
                                              std::string_view json_body) const;
  [[nodiscard]] ControllerResponse assign(const application::AuthenticatedPrincipal &actor,
                                          std::string_view task_id,
                                          std::string_view json_body) const;
  [[nodiscard]] ControllerResponse remove(const application::AuthenticatedPrincipal &actor,
                                          std::string_view task_id,
                                          std::string_view json_body) const;
  [[nodiscard]] ControllerResponse list(const application::AuthenticatedPrincipal &actor,
                                        application::NormalizedTaskQuery query,
                                        domain::PageRequest page) const;

private:
  const application::TaskUseCases *use_cases_;
  const application::TaskListUseCase *list_use_case_{nullptr};
};

} // namespace taskflow::transport::http
