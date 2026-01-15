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
#include <memory>

#include "unit/TestBase.h"
#include "unit/Catch.h"

#include "LogAttribute.h"
#include "UpdateAttribute.h"
#include "GenerateFlowFile.h"

TEST_CASE("UpdateAttributeTest", "[updateAttributeTest]") {
  TestController test_controller;

  test_controller.getLogTestController().setDebug<minifi::processors::UpdateAttribute>();
  test_controller.getLogTestController().setDebug<TestPlan>();
  test_controller.getLogTestController().setDebug<minifi::processors::LogAttribute>();
  std::shared_ptr<TestPlan> plan = test_controller.createPlan();

  plan->addProcessor("GenerateFlowFile", "generate");
  const auto &update_proc = plan->addProcessor("UpdateAttribute", "update", core::Relationship("success", "description"), true);
  plan->addProcessor("LogAttribute", "log", core::Relationship("success", "description"), true);

  plan->setDynamicProperty(update_proc, "test_attr_1", "test_val_1");
  plan->setDynamicProperty(update_proc, "test_attr_2", "test_val_2");

  test_controller.runSession(plan, false);  // generate
  test_controller.runSession(plan, false);  // update
  test_controller.runSession(plan, false);  // log

  REQUIRE(test_controller.getLogTestController().contains("key:test_attr_1 value:test_val_1"));
  REQUIRE(test_controller.getLogTestController().contains("key:test_attr_2 value:test_val_2"));

  test_controller.getLogTestController().reset();
}
