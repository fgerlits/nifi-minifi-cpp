/* Licensed to the Apache Software Foundation (ASF) under one or more
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
#include "utils/BackTrace.h"

#include "utils/OptionalUtils.h"

#ifndef HAS_EXECINFO
BackTrace TraceResolver::getBackTrace(std::string thread_name, std::thread::native_handle_type /*thread_handle*/) {
  back_trace_ = BackTrace(std::move(thread_name));
  return std::move(back_trace_);
}
#else

#include <cxxabi.h>
#include <execinfo.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#ifdef __linux__
#include <link.h>
#endif
#include <csignal>
#include <pthread.h>
#include <semaphore.h>
#include <array>
#include <cerrno>
#include <iostream>
#include <utility>
#include <memory>

#include "utils/Deleters.h"

namespace {
  std::optional<std::string> demangle_symbol(const char* symbol_name) {
    int status = 0;
    std::unique_ptr<char, org::apache::nifi::minifi::utils::FreeDeleter> demangled(abi::__cxa_demangle(symbol_name, nullptr, nullptr, &status));
    if (status == 0) {
      std::string demangled_name = demangled.get();
      return { demangled_name };
    } else {
      return {};
    }
  }

  /**
   * Handoff state for the single in-flight backtrace. The TraceResolver singleton serializes all
   * getBackTrace() calls via its mutex_, so only one capture is ever in progress. The signalled
   * target thread fills the buffer and posts the semaphore (both async-signal-safe); the requesting
   * thread waits on the semaphore and then symbolizes.
   */
  struct CaptureState {
    static constexpr size_t TRACE_BUFFER_SIZE = 128;
    std::array<void*, TRACE_BUFFER_SIZE> stack_buffer{};
    int trace_size{0};
    sem_t sem{};

    CaptureState() {
      sem_init(&sem, 0, 0);
      // Warm up backtrace()/libgcc so its one-time lazy dlopen (which allocates, and must therefore
      // not run inside a signal handler) happens here, in a normal context, at first use.
      backtrace(stack_buffer.data(), static_cast<int>(stack_buffer.size()));
    }
    ~CaptureState() { sem_destroy(&sem); }
  };

  CaptureState& captureState() {
    static CaptureState state;
    return state;
  }

  void captureRawTrace() {
    CaptureState& cap = captureState();
    cap.trace_size = backtrace(cap.stack_buffer.data(), static_cast<int>(cap.stack_buffer.size()));
  }

  void resolveRawTrace(const uint8_t frames_to_skip) {
    const CaptureState& cap = captureState();
    // We can skip the innermost frames (the capture function and the signal handler)
    for (int i = frames_to_skip; i < cap.trace_size; i++) {
      const char* file_name = "???";
      uintptr_t symbol_offset = 0;

      // Translate the address to symbolic information
      Dl_info dl_info{};
#ifdef __linux__
      struct link_map* l_map = nullptr;
      int res = dladdr1(cap.stack_buffer[i], &dl_info, reinterpret_cast<void**>(&l_map), RTLD_DL_LINKMAP);
#else  // __linux__
      int res = dladdr(cap.stack_buffer[i], &dl_info);
#endif  // __linux__
      if (res == 0 || dl_info.dli_fname == nullptr || dl_info.dli_fname[0] == '\0') {
        // We could not determine symbolic information for this address
        TraceResolver::getResolver().addTraceLine(file_name, nullptr, symbol_offset);
        continue;
      }

      // Determine the filename of the shared object
      if (dl_info.dli_fname != nullptr) {
        const char* last_slash = strrchr(dl_info.dli_fname, '/');
        // If the shared object name is a full path, we still only want the filename component
        if (last_slash != nullptr) {
          file_name = last_slash + 1;
        } else {
          file_name = dl_info.dli_fname;
        }
      }

      const std::string symbol_name = dl_info.dli_sname
          ? demangle_symbol(dl_info.dli_sname).value_or(file_name)
          : file_name;

      // Determine our offset
      uintptr_t base_address = 0;
      if (dl_info.dli_sname != nullptr) {
        // If we could determine our symbol we will display our offset from the address of the symbol
        base_address = reinterpret_cast<uintptr_t>(dl_info.dli_saddr);
      } else {
        // Otherwise we will display our offset from base address of the shared object
#ifdef __linux__
        // glibc uses l_addr from the link map instead of dli_fbase in backtrace_symbols for calculating this offset
        // I could not find a difference between the two in my limited measurements, but we will use it too, just to be sure
        if (l_map != nullptr) {
          dl_info.dli_fbase = reinterpret_cast<void*>(l_map->l_addr);  // NOLINT(performance-no-int-to-ptr)
        }
#endif  // __linux__
        base_address = reinterpret_cast<uintptr_t>(dl_info.dli_fbase);
      }
      symbol_offset = reinterpret_cast<uintptr_t>(cap.stack_buffer[i]) - base_address;

      TraceResolver::getResolver().addTraceLine(file_name, symbol_name.c_str(), symbol_offset);
    }
  }

  void handler(int /*signr*/, siginfo_t* /*info*/, void* /*secret*/) {
    const int saved_errno = errno;
    captureRawTrace();
    sem_post(&captureState().sem);
    errno = saved_errno;
  }

  void emplace_handler() {
    struct sigaction sa{};
    sigfillset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handler;
    sigaction(SIGUSR2, &sa, nullptr);
  }
}  // namespace

BackTrace TraceResolver::getBackTrace(std::string thread_name, std::thread::native_handle_type thread_handle) {
  std::scoped_lock guard(mutex_);
  CaptureState& cap = captureState();
  back_trace_ = BackTrace(std::move(thread_name));

  if (0 == thread_handle || pthread_equal(pthread_self(), thread_handle)) {
    captureRawTrace();
    resolveRawTrace(1);
  } else {
    emplace_handler();
    if (pthread_kill(thread_handle, SIGUSR2) != 0) {
      return std::move(back_trace_);
    }
    while (sem_wait(&cap.sem) != 0 && errno == EINTR) {
    }
    resolveRawTrace(1);
  }
  return std::move(back_trace_);
}

#endif  // HAS_EXECINFO
