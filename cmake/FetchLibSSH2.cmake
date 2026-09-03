# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

message(STATUS "Using bundled libssh2 via FetchContent")

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

include(FetchContent)

if (WIN32)
    set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/libssh2/fix-windows-ioctl.patch")
    set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")
else()
    set(PC "")
endif()

FetchContent_Declare(
        libssh2
        GIT_REPOSITORY https://github.com/libssh2/libssh2
        GIT_TAG        4884fc6102b32b76f0b1606a76477abe0e68ee51  # head of the master branch as of 2026-09-03
        PATCH_COMMAND "${PC}"
        SYSTEM
        OVERRIDE_FIND_PACKAGE
)

set(ENABLE_ZLIB_COMPRESSION ON CACHE BOOL "" FORCE)
set(CRYPTO_BACKEND "OpenSSL" CACHE STRING "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libssh2)

target_link_libraries(libssh2_static PUBLIC OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)

if (NOT TARGET Libssh2::libssh2)
    add_library(Libssh2::libssh2 ALIAS libssh2_static)
endif()
