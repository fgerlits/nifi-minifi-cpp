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

#define CATCH_CONFIG_RUNNER
#include <memory>
#include <string>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "catch2/catch_session.hpp"
#include "utils/AutoPersistor.h"
#include "controllers/keyvalue/KeyValueStateStorage.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "unit/ProvenanceTestHelper.h"
#include "repository/VolatileContentRepository.h"
#include "utils/file/FileUtils.h"

namespace {
  std::string config_yaml; // NOLINT
}

int main(int argc, char* argv[]) {
  Catch::Session session;

  auto cli = session.cli()
      | Catch::Clara::Opt{config_yaml, "config-yaml"}
          ["--config-yaml"]
          ("path to the config.yaml containing the VolatileMapStateStorageTest controller service configuration");
  session.cli(cli);

  int ret = session.applyCommandLine(argc, argv);
  if (ret != 0) {
    return ret;
  }

  if (config_yaml.empty()) {
    std::cerr << "Missing --config-yaml <path>. It must contain the path to the config.yaml containing the VolatileMapStateStorageTest controller service configuration." << std::endl;
    return -1;
  }

  return session.run();
}

class VolatileMapStateStorageTestFixture {
 public:
  VolatileMapStateStorageTestFixture() {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setTrace<minifi::controllers::KeyValueStateStorage>();
    LogTestController::getInstance().setTrace<minifi::controllers::AutoPersistor>();

    std::filesystem::current_path(testController.createTempDirectory());

    configuration->set(minifi::Configure::nifi_flow_configuration_file, config_yaml);
    content_repo->initialize(configuration);

    process_group = yaml_config->getRoot();
    auto* key_value_store_service_node = process_group->findControllerService("testcontroller");
    REQUIRE(key_value_store_service_node != nullptr);
    key_value_store_service_node->enable();

    controller = std::dynamic_pointer_cast<minifi::controllers::KeyValueStateStorage>(
            key_value_store_service_node->getControllerServiceImplementation());
    REQUIRE(controller != nullptr);
  }

  VolatileMapStateStorageTestFixture(VolatileMapStateStorageTestFixture&&) = delete;
  VolatileMapStateStorageTestFixture(const VolatileMapStateStorageTestFixture&) = delete;
  VolatileMapStateStorageTestFixture& operator=(VolatileMapStateStorageTestFixture&&) = delete;
  VolatileMapStateStorageTestFixture& operator=(const VolatileMapStateStorageTestFixture&) = delete;

  virtual ~VolatileMapStateStorageTestFixture() {
    std::filesystem::current_path(minifi::utils::file::get_executable_dir());
    LogTestController::getInstance().reset();
  }

 protected:
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  std::unique_ptr<core::YamlConfiguration> yaml_config = std::make_unique<core::YamlConfiguration>(core::ConfigurationContext{
      .flow_file_repo = test_repo,
      .content_repo = content_repo,
      .configuration = configuration,
      .path = config_yaml,
      .filesystem = std::make_shared<utils::file::FileSystem>(),
      .sensitive_values_encryptor = utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{utils::crypto::XSalsa20Cipher::generateKey()}}
  });
  std::unique_ptr<core::ProcessGroup> process_group;

  std::shared_ptr<minifi::controllers::KeyValueStateStorage> controller;

  TestController testController;
};

TEST_CASE_METHOD(VolatileMapStateStorageTestFixture, "VolatileMapStateStorageTest set and get", "[basic]") {
  const char* key = "foobar";
  const char* value = "234";
  REQUIRE(true == controller->set(key, value));

  std::string res;
  REQUIRE(true == controller->get(key, res));
  REQUIRE(value == res);
}

TEST_CASE_METHOD(VolatileMapStateStorageTestFixture, "VolatileMapStateStorageTestFixture set and get all", "[basic]") {
  const std::unordered_map<std::string, std::string> kvs = {
          {"foobar", "234"},
          {"buzz", "value"},
  };
  for (const auto& kv : kvs) {
    REQUIRE(true == controller->set(kv.first, kv.second));
  }

  std::unordered_map<std::string, std::string> kvs_res;
  REQUIRE(true == controller->get(kvs_res));
  REQUIRE(kvs == kvs_res);
}

TEST_CASE_METHOD(VolatileMapStateStorageTestFixture, "VolatileMapStateStorageTestFixture set and overwrite", "[basic]") {
  const char* key = "foobar";
  const char* value = "234";
  const char* new_value = "baz";
  REQUIRE(true == controller->set(key, value));
  REQUIRE(true == controller->set(key, new_value));

  std::string res;
  REQUIRE(true == controller->get(key, res));
  REQUIRE(new_value == res);
}

TEST_CASE_METHOD(VolatileMapStateStorageTestFixture, "VolatileMapStateStorageTestFixture set and remove", "[basic]") {
  const char* key = "foobar";
  const char* value = "234";
  REQUIRE(true == controller->set(key, value));
  REQUIRE(true == controller->remove(key));

  std::string res;
  REQUIRE(false == controller->get(key, res));
}


TEST_CASE_METHOD(VolatileMapStateStorageTestFixture, "VolatileMapStateStorageTestFixture set and clear", "[basic]") {
  const std::unordered_map<std::string, std::string> kvs = {
          {"foobar", "234"},
          {"buzz", "value"},
  };
  for (const auto& kv : kvs) {
    REQUIRE(true == controller->set(kv.first, kv.second));
  }
  REQUIRE(true == controller->clear());

  std::unordered_map<std::string, std::string> kvs_res;

  /* Make sure we can still insert after we cleared */
  const char* key = "foo";
  const char* value = "bar";
  REQUIRE(true == controller->set(key, value));
  std::string res;
  REQUIRE(true == controller->get(key, res));
  REQUIRE(value == res);
}
