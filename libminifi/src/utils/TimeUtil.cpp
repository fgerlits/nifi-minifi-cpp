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

#include "utils/TimeUtil.h"
#include "range/v3/algorithm/contains.hpp"

namespace org::apache::nifi::minifi::utils::timeutils {

using namespace std::literals::chrono_literals;
static std::mutex global_clock_mtx;
static std::shared_ptr<SteadyClock> global_clock{std::make_shared<SteadyClock>()};

std::shared_ptr<SteadyClock> getClock() {
  std::lock_guard lock(global_clock_mtx);
  return global_clock;
}

// test-only utility to specify what clock to use
void setClock(std::shared_ptr<SteadyClock> clock) {
  std::lock_guard lock(global_clock_mtx);
  global_clock = std::move(clock);
}

std::optional<std::chrono::system_clock::time_point> parseRfc3339(const std::string& str) {
  std::istringstream stream(str);
  std::chrono::year_month_day date_part{};
  stream >> std::chrono::parse("%F", date_part);

  if (stream.fail())
    return std::nullopt;

  constexpr std::string_view accepted_delimiters = "tT_ ";
  char delimiter_char = 0;
  stream.get(delimiter_char);

  if (stream.fail() || !ranges::contains(accepted_delimiters, delimiter_char))
    return std::nullopt;

  std::chrono::system_clock::duration time_part;
  if (str.ends_with('Z') || str.ends_with('z')) {
    stream >> std::chrono::parse("%T", time_part);
    if (stream.fail())
      return std::nullopt;
    stream.get();
  } else {
    stream >> std::chrono::parse("%T%Ez", time_part);
  }

  if (stream.fail() || (stream.peek() && !stream.eof()))
    return std::nullopt;

  return std::chrono::sys_days(date_part) + time_part;
}

}  // namespace org::apache::nifi::minifi::utils::timeutils
