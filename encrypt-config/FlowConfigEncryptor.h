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
#pragma once

#include <string>
#include <filesystem>

#include "Utils.h"
#include "core/flow/AdaptiveConfiguration.h"

namespace org::apache::nifi::minifi::encrypt_config::flow_config_encryptor {

void encryptSensitiveValuesInFlowConfig(const EncryptionKeys& keys, const std::filesystem::path& minifi_home, const std::filesystem::path& flow_config_path);
void encryptSensitiveValuesInFlowConfig(const EncryptionKeys& keys, const std::filesystem::path& minifi_home, const std::filesystem::path& flow_config_path,
    const std::string& component_id, const std::string& property_name, const std::string& property_value);

}  // namespace org::apache::nifi::minifi::encrypt_config::flow_config_encryptor
