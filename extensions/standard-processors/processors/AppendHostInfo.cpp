/**
 * @file AppendHostInfo.cpp
 * AppendHostInfo class implementation
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
#include "AppendHostInfo.h"

#ifndef __USE_POSIX
#define __USE_POSIX
#endif /* __USE_POSIX */

#include <memory>
#include <string>
#include <regex>
#include <algorithm>
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/FlowFile.h"
#include "core/Resource.h"
#include "utils/net/NetworkInterfaceInfo.h"
#include "utils/net/DNS.h"

namespace org::apache::nifi::minifi::processors {

void AppendHostInfo::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void AppendHostInfo::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  std::unique_lock unique_lock(shared_mutex_);
  hostname_attribute_name_ = context.getProperty(HostAttribute) | utils::orThrow("AppendHostInfo::HostAttribute is a required Property");
  ipaddress_attribute_name_ = context.getProperty(IPAttribute) | utils::orThrow("AppendHostInfo::IPAttribute is a required Property");
  interface_name_filter_ = context.getProperty(InterfaceNameFilter)
      | utils::toOptional()
      | utils::filter([](const std::string& inf) { return !inf.empty(); });

  if (auto refresh_policy = context.getProperty(RefreshPolicy); refresh_policy && *refresh_policy == REFRESH_POLICY_ON_TRIGGER)
    refresh_on_trigger_ = true;
  else
    refreshHostInfo();
}

void AppendHostInfo::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  const std::shared_ptr<core::FlowFile> flow = session.get();
  if (!flow)
    return;

  {
    std::shared_lock shared_lock(shared_mutex_);
    if (refresh_on_trigger_) {
      shared_lock.unlock();
      std::unique_lock unique_lock(shared_mutex_);
      refreshHostInfo();
    }
  }

  {
    std::shared_lock shared_lock(shared_mutex_);
    flow->addAttribute(hostname_attribute_name_, hostname_);
    if (ipaddresses_.has_value()) {
      flow->addAttribute(ipaddress_attribute_name_, ipaddresses_.value());
    }
  }

  session.transfer(flow, Success);
}

void AppendHostInfo::refreshHostInfo() {
  hostname_ = org::apache::nifi::minifi::utils::net::getMyHostName();
  auto filter = [this](const utils::NetworkInterfaceInfo& interface_info) -> bool {
    const bool has_ipv4_address = interface_info.hasIpV4Address();
    const bool matches_regex_or_empty_regex = (!interface_name_filter_.has_value()) || std::regex_match(interface_info.getName(), interface_name_filter_.value());
    return has_ipv4_address && matches_regex_or_empty_regex;
  };
  auto network_interface_infos = utils::NetworkInterfaceInfo::getNetworkInterfaceInfos(filter);
  std::ostringstream oss;
  if (network_interface_infos.empty()) {
    ipaddresses_ = std::nullopt;
  } else {
    for (auto& network_interface_info : network_interface_infos) {
      auto& ip_v4_addresses = network_interface_info.getIpV4Addresses();
      std::copy(std::begin(ip_v4_addresses), std::end(ip_v4_addresses), std::ostream_iterator<std::string>(oss, ","));
    }
    ipaddresses_ = oss.str();
    ipaddresses_.value().pop_back();  // to remove trailing comma
  }
}

REGISTER_RESOURCE(AppendHostInfo, Processor);

}  // namespace org::apache::nifi::minifi::processors
