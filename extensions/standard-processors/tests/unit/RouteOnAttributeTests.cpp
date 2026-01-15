/**
 *
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
#include <array>
#include <memory>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include <RouteOnAttribute.h>
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"

TEST_CASE("RouteOnAttributeMatchedTest", "[routeOnAttributeMatchedTest]") {
  TestController test_controller;

  test_controller.getLogTestController().setDebug<minifi::processors::UpdateAttribute>();
  test_controller.getLogTestController().setDebug<minifi::processors::RouteOnAttribute>();
  test_controller.getLogTestController().setDebug<TestPlan>();
  test_controller.getLogTestController().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = test_controller.createPlan();

  plan->addProcessor("GenerateFlowFile", "generate");

  const auto &update_proc = plan->addProcessor("UpdateAttribute", "update", core::Relationship("success", "description"), true);
  plan->setDynamicProperty(update_proc, "route_condition_attr", "true");

  const auto &route_proc = plan->addProcessor("RouteOnAttribute", "route", core::Relationship("success", "description"), true);
  route_proc->setAutoTerminatedRelationships(std::array{core::Relationship("unmatched", "description")});
  plan->setDynamicProperty(route_proc, "route_matched", "${route_condition_attr}");

  const auto &update_matched_proc = plan->addProcessor("UpdateAttribute", "update_matched", core::Relationship("route_matched", "description"), true);
  plan->setDynamicProperty(update_matched_proc, "route_check_attr", "good");

  plan->addProcessor("LogAttribute", "log", core::Relationship("success", "description"), true);

  test_controller.runSession(plan, false);  // generate
  test_controller.runSession(plan, false);  // update
  test_controller.runSession(plan, false);  // route
  test_controller.runSession(plan, false);  // update_matched
  test_controller.runSession(plan, false);  // log

  REQUIRE(test_controller.getLogTestController().contains("key:route_check_attr value:good"));

  test_controller.getLogTestController().reset();
}

TEST_CASE("RouteOnAttributeUnmatchedTest", "[routeOnAttributeUnmatchedTest]") {
  TestController test_controller;

  test_controller.getLogTestController().setDebug<minifi::processors::UpdateAttribute>();
  test_controller.getLogTestController().setDebug<minifi::processors::RouteOnAttribute>();
  test_controller.getLogTestController().setDebug<TestPlan>();
  test_controller.getLogTestController().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = test_controller.createPlan();

  plan->addProcessor("GenerateFlowFile", "generate");

  const auto &update_proc = plan->addProcessor("UpdateAttribute", "update", core::Relationship("success", "description"), true);
  plan->setDynamicProperty(update_proc, "route_condition_attr", "false");

  const auto &route_proc = plan->addProcessor("RouteOnAttribute", "route", core::Relationship("success", "description"), true);
  plan->setDynamicProperty(route_proc, "route_matched", "${route_condition_attr}");

  const auto &update_matched_proc = plan->addProcessor("UpdateAttribute", "update_matched", core::Relationship("unmatched", "description"), true);
  plan->setDynamicProperty(update_matched_proc, "route_check_attr", "good");

  plan->addProcessor("LogAttribute", "log", core::Relationship("success", "description"), true);

  test_controller.runSession(plan, false);  // generate
  test_controller.runSession(plan, false);  // update
  test_controller.runSession(plan, false);  // route
  test_controller.runSession(plan, false);  // update_matched
  test_controller.runSession(plan, false);  // log

  REQUIRE(test_controller.getLogTestController().contains("key:route_check_attr value:good"));

  test_controller.getLogTestController().reset();
}
