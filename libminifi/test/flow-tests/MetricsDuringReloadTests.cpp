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
#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "c2/C2MetricsPublisher.h"
#include "c2/C2Utils.h"
#include "core/state/MetricsPublisherStore.h"
#include "properties/Configuration.h"
#include "CustomProcessors.h"
#include "unit/Catch.h"
#include "unit/TestControllerWithFlow.h"

namespace {

std::string createFlow(const std::string& processor_name) {
  return R"(
Flow Controller:
  name: MiNiFi Flow
Processors:
- name: )" + processor_name + R"(
  id: 2438e3c8-015a-1000-79ca-83af40ec1991
  class: org.apache.nifi.processors.standard.TestFlowFileGenerator
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 10 ms
  auto-terminated relationships list:
  - success
  Properties:
    Batch Size: 1
Connections: []
Remote Processing Groups: []
Controller Services: []
)";
}

constexpr std::string_view UnparseableFlow = "this is not a valid flow configuration";

std::unique_ptr<TestControllerWithFlow> createTestController(const std::string& flow) {
  auto test_controller = std::make_unique<TestControllerWithFlow>(flow.c_str(), /* setup_flow = */ false);
  test_controller->configuration_->set(minifi::Configuration::nifi_c2_root_classes,
      "DeviceInfoNode,AgentInformation,FlowInformation");
  test_controller->setupFlow();
  return test_controller;
}

}  // namespace

TEST_CASE("Metrics can be collected while the flow is reloaded", "[MetricsDuringReload]") {
  auto test_controller = createTestController(createFlow("Processor One"));
  test_controller->startFlow();

  auto* metrics_publisher_store = test_controller->metrics_publisher_store_;
  REQUIRE(metrics_publisher_store != nullptr);
  auto c2_metrics_publisher = std::dynamic_pointer_cast<minifi::c2::C2MetricsPublisher>(
      metrics_publisher_store->getMetricsPublisher(minifi::c2::C2_METRICS_PUBLISHER).lock());
  REQUIRE(c2_metrics_publisher != nullptr);
  REQUIRE_FALSE(c2_metrics_publisher->getHeartbeatNodes(true).empty());

  std::atomic<bool> reloads_done{false};
  std::atomic<size_t> heartbeat_nodes{0};
  std::atomic<size_t> described_nodes{0};

  // Readers: the heartbeat and the describe path, i.e. what the C2 agent's two worker threads do. A failure
  // here shows up as a hang, a TSan report or an ASan report rather than a failed assertion, because REQUIRE
  // is not safe to call from a non-Catch2 thread.
  auto heartbeat_thread = std::thread([&] {
    while (!reloads_done) {
      heartbeat_nodes += c2_metrics_publisher->getHeartbeatNodes(false).size();
    }
  });
  auto describe_thread = std::thread([&] {
    while (!reloads_done) {
      if (const auto node = c2_metrics_publisher->getMetricsNode("FlowInformation")) {
        described_nodes += node->serialized_nodes.size();
      }
    }
  });

  // Writer: alternate between two flows. The unparseable payload is rejected before the live flow is
  // touched, which must leave the running flow and the readers alone.
  size_t successful_reloads = 0;
  for (int i = 0; i < 12; ++i) {
    if (i % 4 == 3) {
      CHECK_FALSE(test_controller->controller_->applyConfiguration("/flows/bad", std::string{UnparseableFlow}).has_value());
    } else if (test_controller->controller_->applyConfiguration(
        "/flows/" + std::to_string(i), createFlow(i % 2 == 0 ? "Processor Two" : "Processor One")).has_value()) {
      ++successful_reloads;
    }
  }
  reloads_done = true;
  heartbeat_thread.join();
  describe_thread.join();

  CHECK(successful_reloads == 9);
  CHECK(heartbeat_nodes > 0);
  CHECK(described_nodes > 0);

  // The flow survived all that and still reports itself.
  const auto components = test_controller->controller_->getComponentStates();
  REQUIRE(components.size() == 2);
  CHECK(components[0].name == "Processor Two");
  CHECK(components[1].name == "FlowController");
}

TEST_CASE("getComponentStates lists every processor and then the agent", "[MetricsDuringReload]") {
  auto test_controller = createTestController(createFlow("Processor One"));
  auto& flow_controller = *test_controller->controller_;

  SECTION("before the flow is started") {
    const auto components = flow_controller.getComponentStates();
    REQUIRE(components.size() == 2);
    CHECK(components[0].name == "Processor One");
    CHECK(components[1].name == "FlowController");
    CHECK_FALSE(components[1].running);
  }

  SECTION("after the flow is started") {
    test_controller->startFlow();
    const auto components = flow_controller.getComponentStates();
    REQUIRE(components.size() == 2);
    CHECK(components[0].name == "Processor One");
    CHECK(components[1].name == "FlowController");
    CHECK(components[1].running);
  }

  SECTION("the processor list follows a flow update") {
    test_controller->startFlow();
    REQUIRE(flow_controller.applyConfiguration("/flows/2", createFlow("Processor Two")).has_value());

    const auto components = flow_controller.getComponentStates();
    REQUIRE(components.size() == 2);
    CHECK(components[0].name == "Processor Two");
    CHECK(components[1].name == "FlowController");
  }

  SECTION("a rejected flow update leaves the component list alone") {
    test_controller->startFlow();
    REQUIRE_FALSE(flow_controller.applyConfiguration("/flows/bad", std::string{UnparseableFlow}).has_value());

    const auto components = flow_controller.getComponentStates();
    REQUIRE(components.size() == 2);
    CHECK(components[0].name == "Processor One");
    CHECK(components[1].name == "FlowController");
  }
}
