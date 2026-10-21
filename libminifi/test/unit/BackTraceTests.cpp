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
#ifndef NDEBUG  // TestBase.h #undef's NDEBUG
#define IS_DEBUG 1
#endif

#include <algorithm>
#include <utility>
#include <future>
#include <memory>
#include <string>

#include <iostream>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/BackTrace.h"
#include "utils/Monitors.h"
#include "utils/ThreadPool.h"

using namespace std::literals::chrono_literals;

TEST_CASE("Simple backtrace test", "[BT1]") {
  const BackTrace trace = TraceResolver::getResolver().getBackTrace("BT1");
#ifdef HAS_EXECINFO
  const auto lines = trace.getTraces();
for (const auto& line : lines) {
  std::cout << "test 1: " << line << '\n';
}
std::cout << "\n#################################################\n\n";
  CHECK(!lines.empty());
  CHECK(std::ranges::any_of(lines,
      [](const std::string& line) { return line.contains(" @ ") && line.contains(" + "); }));
#ifdef IS_DEBUG
  CHECK(std::ranges::any_of(lines,
      [](const std::string& line) { return line.contains("getBackTrace"); }));
#endif
#endif
}

void inner_function(std::atomic_flag& ready_to_check, std::atomic_flag& checking_done) {
  ready_to_check.test_and_set();
  ready_to_check.notify_all();
  checking_done.wait(false);
}

void outer_function(std::atomic_flag& ready_to_check, std::atomic_flag& checking_done) {
  inner_function(ready_to_check, checking_done);
}


TEST_CASE("Backtrace test with thread pool", "[BT2]") {
  std::atomic_flag ready_for_checking;
  std::atomic_flag done_with_checking;

  constexpr std::string_view thread_pool_name = "Winnie the pool";
  constexpr size_t number_of_worker_threads = 3;
  utils::ThreadPool pool(number_of_worker_threads, std::string{thread_pool_name});
  utils::Worker worker([&]() -> utils::TaskRescheduleInfo {
    outer_function(ready_for_checking, done_with_checking);
    return utils::TaskRescheduleInfo::Done();
  }, "id");
  std::future<utils::TaskRescheduleInfo> future;
  pool.execute(std::move(worker), future);

  pool.start();
  {
    ready_for_checking.wait(false);
    std::vector<BackTrace> traces = pool.getTraces();
    CHECK(traces.size() <= number_of_worker_threads);
    REQUIRE(!traces.empty());
    for (const auto &trace : traces) {
      CHECK(trace.getName().starts_with(thread_pool_name));
      for (const auto& line : trace.getTraces()) {
        std::cout << "test 2: " << line << '\n';
      }
      std::cout << "\n#################################################\n\n";
    }
#ifdef HAS_EXECINFO
    const auto& first_worker_trace = traces.front();
    const auto lines = first_worker_trace.getTraces();
    CHECK(std::ranges::any_of(lines,
        [](const std::string& line) { return line.contains(" @ ") && line.contains(" + "); }));
    CHECK(std::ranges::any_of(lines,
        [](const std::string& line) { return line.contains("run_tasks"); }));
#endif
    done_with_checking.test_and_set();
    done_with_checking.notify_all();
  }
  future.wait();
}
