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

#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

class TraceResolver;

class BackTrace {
 public:
  BackTrace() = default;

  explicit BackTrace(std::string name)
      : name_(std::move(name)) {
  }

  [[nodiscard]] std::vector<std::string> getTraces() const {
    return trace_;
  }

  [[nodiscard]] std::string getName() const {
    return name_;
  }

 protected:
  void addLine(std::string symbol_line) {
    trace_.emplace_back(std::move(symbol_line));
  }

 private:
  std::string name_;
  std::vector<std::string> trace_;
  friend class TraceResolver;
};

class TraceResolver {
 public:
  BackTrace getBackTrace(std::string thread_name, std::thread::native_handle_type thread);
  BackTrace getBackTrace(std::string thread_name) {
#ifdef WIN32
    return BackTrace(std::move(thread_name));
#else
    return getBackTrace(std::move(thread_name), pthread_self());
#endif
  }

  static TraceResolver& getResolver() {
    static TraceResolver resolver;
    return resolver;
  }

  void addTraceLine(const char* file_name, const char* symbol_name, uintptr_t symbol_offset) {
    std::stringstream line;
    line << file_name;
    if (symbol_name != nullptr) {
      line << " @ " << symbol_name << " + " << symbol_offset;
    }
    back_trace_.addLine(line.str());
  }

 private:
  TraceResolver() = default;

  BackTrace back_trace_;
  mutable std::mutex mutex_;
};
