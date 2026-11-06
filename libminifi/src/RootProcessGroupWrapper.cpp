/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "RootProcessGroupWrapper.h"

namespace org::apache::nifi::minifi {

void RootProcessGroupWrapper::updatePropertyValue(const std::string& processor_name, const std::string& property_name, const std::string& property_value) {
  if (root_ != nullptr) {
    root_->updatePropertyValue(processor_name, property_name, property_value);
  }
}

std::string RootProcessGroupWrapper::getName() const {
  return getIdentity().name;
}

utils::Identifier RootProcessGroupWrapper::getComponentUUID() const {
  return getIdentity().uuid;
}

std::string RootProcessGroupWrapper::getVersion() const {
  return getIdentity().version;
}

RootProcessGroupWrapper::RootIdentity RootProcessGroupWrapper::getIdentity() const {
  std::shared_lock lock(component_cache_mutex_);
  return cached_identity_;
}

std::vector<state::ComponentState> RootProcessGroupWrapper::getProcessorStates() const {
  std::shared_lock lock(component_cache_mutex_);
  std::vector<state::ComponentState> states;
  states.reserve(cached_processors_.size());
  for (const auto* processor : cached_processors_) {
    states.push_back({.name = processor->getName(), .uuid = processor->getUUID(), .running = processor->isRunning()});
  }
  return states;
}

void RootProcessGroupWrapper::setComponentCacheRoot(core::ProcessGroup* root) {
  std::vector<core::Processor*> processors;
  RootIdentity identity;
  if (root) {
    root->getAllProcessors(processors);
    identity = {.name = root->getName(), .uuid = root->getUUID(), .version = std::to_string(root->getVersion())};
  }

  std::unique_lock lock(component_cache_mutex_);
  cached_processors_ = std::move(processors);
  cached_identity_ = std::move(identity);
}

void RootProcessGroupWrapper::setNewRoot(std::unique_ptr<core::ProcessGroup> new_root) {
  if (!new_root) {
    logger_->log_error("New flow to be set was empty!");
    return;
  }

  if (metrics_publisher_store_) { metrics_publisher_store_->clearMetricNodes(); }
  if (controller_socket_protocol_) {
    controller_socket_protocol_->setRoot(nullptr);
  }
  setComponentCacheRoot(nullptr);
  backup_root_ = std::move(root_);
  root_ = std::move(new_root);
  processor_to_controller_.clear();
  setComponentCacheRoot(root_.get());
  if (metrics_publisher_store_) { metrics_publisher_store_->loadMetricNodes(root_.get()); }
  if (controller_socket_protocol_) {
    controller_socket_protocol_->setRoot(root_.get());
  }
}

void RootProcessGroupWrapper::restoreBackup() {
  if (metrics_publisher_store_) { metrics_publisher_store_->clearMetricNodes(); }
  if (controller_socket_protocol_) {
    controller_socket_protocol_->setRoot(nullptr);
  }
  setComponentCacheRoot(nullptr);
  root_ = std::move(backup_root_);
  processor_to_controller_.clear();
  setComponentCacheRoot(root_.get());
  if (metrics_publisher_store_) { metrics_publisher_store_->loadMetricNodes(root_.get()); }
  if (controller_socket_protocol_) {
    controller_socket_protocol_->setRoot(root_.get());
  }
}

void RootProcessGroupWrapper::clearBackup() {
  backup_root_ = nullptr;
}

void RootProcessGroupWrapper::stopProcessing(TimerDrivenSchedulingAgent& timer_scheduler,
                          EventDrivenSchedulingAgent& event_scheduler,
                          CronDrivenSchedulingAgent& cron_scheduler) {
  if (!root_) {
    return;
  }
  // stop source processors first
  root_->stopProcessing(timer_scheduler, event_scheduler, cron_scheduler, [] (const core::Processor* proc) -> bool {
    return !proc->hasIncomingConnections();
  });
  // we enable C2 to progressively increase the timeout
  // in case it sees that waiting for a little longer could
  // allow the FlowFiles to be processed
  auto shutdown_start = std::chrono::steady_clock::now();
  while ((std::chrono::steady_clock::now() - shutdown_start) < loadShutdownTimeoutFromConfiguration().value_or(std::chrono::milliseconds{0}) &&
      root_->getTotalFlowFileCount() != 0) {
    std::this_thread::sleep_for(shutdown_check_interval_);
  }
  // shutdown all other processors as well
  root_->stopProcessing(timer_scheduler, event_scheduler, cron_scheduler);
}

void RootProcessGroupWrapper::drainConnections() {
  if (root_) {
    root_->drainConnections();
  }
}

void RootProcessGroupWrapper::getConnections(std::map<std::string, core::Connectable*>& connectionMap) {
  if (root_) {
    root_->getConnections(connectionMap);
  }
}

void RootProcessGroupWrapper::getFlowFileContainers(std::map<std::string, core::Connectable*>& containers) const {
  if (root_) {
    root_->getFlowFileContainers(containers);
  }
}

bool RootProcessGroupWrapper::startProcessing(TimerDrivenSchedulingAgent& timer_scheduler,
                           EventDrivenSchedulingAgent& event_scheduler,
                           CronDrivenSchedulingAgent& cron_scheduler) {
  if (!root_) {
    return false;
  }

  root_->startProcessing(timer_scheduler, event_scheduler, cron_scheduler);
  return true;
}

void RootProcessGroupWrapper::clearConnection(const std::string &connection) {
  if (root_ == nullptr) {
    return;
  }
  logger_->log_info("Attempting to clear connection {}", connection);
  std::map<std::string, Connection*> connections;
  root_->getConnections(connections);
  auto conn = connections.find(connection);
  if (conn != connections.end()) {
    logger_->log_info("Clearing connection {}", connection);
    conn->second->drain(true);
  }
}

state::StateController* RootProcessGroupWrapper::getProcessorController(const std::string& id_or_name,
    const std::function<gsl::not_null<std::unique_ptr<state::ProcessorController>>(core::Processor&)>& controllerFactory) {
  if (!root_) {
    return nullptr;
  }

  return utils::Identifier::parse(id_or_name)
    | utils::andThen([this](utils::Identifier id) { return utils::optional_from_ptr(root_->findProcessorById(id)); })
    | utils::orElse([this, &id_or_name] { return utils::optional_from_ptr(root_->findProcessorByName(id_or_name)); })
    | utils::transform([this, &controllerFactory](gsl::not_null<core::Processor*> proc) -> gsl::not_null<state::ProcessorController*> {
      return utils::optional_from_ptr(processor_to_controller_[proc->getUUID()].get())
          | utils::valueOrElse([this, proc, &controllerFactory] {
            return gsl::make_not_null((processor_to_controller_[proc->getUUID()] = controllerFactory(*proc)).get());
          });
    })
    | utils::valueOrElse([this, &id_or_name]() -> state::ProcessorController* {
      logger_->log_error("Could not get processor controller for requested id/name \"{}\", because the processor was not found", id_or_name);
      return nullptr;
    });
}

std::optional<std::chrono::milliseconds> RootProcessGroupWrapper::loadShutdownTimeoutFromConfiguration() {
  return configuration_->get(Configure::nifi_flowcontroller_drain_timeout) | utils::andThen([] (const auto& s) { return parsing::parseDuration(s) | utils::toOptional(); });
}

}  // namespace org::apache::nifi::minifi
