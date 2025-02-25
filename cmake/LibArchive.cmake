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

include(FetchContent)

set(LIBARCHIVE_STATIC "1" CACHE STRING "" FORCE)
set(ENABLE_MBEDTLS "OFF" CACHE STRING "" FORCE)
set(ENABLE_NETTLE "OFF" CACHE STRING "" FORCE)
set(ENABLE_LIBB2 "OFF" CACHE STRING "" FORCE)
set(ENABLE_LZ4 "OFF" CACHE STRING "" FORCE)
set(ENABLE_LZO "OFF" CACHE STRING "" FORCE)
set(ENABLE_ZSTD "OFF" CACHE STRING "" FORCE)
set(ENABLE_ZLIB "ON" CACHE STRING "" FORCE)
set(ENABLE_LIBXML2 "OFF" CACHE STRING "" FORCE)
set(ENABLE_EXPAT "OFF" CACHE STRING "" FORCE)
set(ENABLE_PCREPOSIX "OFF" CACHE STRING "" FORCE)
set(ENABLE_TAR "OFF" CACHE STRING "" FORCE) # This does not disable the tar format, just the standalone tar command line utility
set(ENABLE_CPIO "OFF" CACHE STRING "" FORCE)
set(ENABLE_CAT "OFF" CACHE STRING "" FORCE)
set(ENABLE_XATTR "ON" CACHE STRING "" FORCE)
set(ENABLE_ACL "ON" CACHE STRING "" FORCE)
set(ENABLE_ICONV "OFF" CACHE STRING "" FORCE)
set(ENABLE_TEST "OFF" CACHE STRING "" FORCE)
set(ENABLE_WERROR "OFF" CACHE STRING "" FORCE)
set(ENABLE_OPENSSL "ON" CACHE STRING "" FORCE)

if (ENABLE_LZMA)
    set(ENABLE_LZMA "ON" CACHE STRING "" FORCE)
else()
    set(ENABLE_LZMA "OFF" CACHE STRING "" FORCE)
endif()

if (ENABLE_BZIP2)
    set(ENABLE_BZip2 "ON" CACHE STRING "" FORCE)
else()
    set(ENABLE_BZip2 "OFF" CACHE STRING "" FORCE)
endif()

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/libarchive/libarchive.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(archive_static
        URL "https://github.com/libarchive/libarchive/releases/download/v3.4.2/libarchive-3.4.2.tar.gz"
        URL_HASH "SHA256=b60d58d12632ecf1e8fad7316dc82c6b9738a35625746b47ecdcaf4aed176176"
        PATCH_COMMAND ${PC})

FetchContent_MakeAvailable(archive_static)

if (ENABLE_BZIP2)
  add_dependencies(archive_static BZip2::BZip2)
  set_property(TARGET archive_static APPEND PROPERTY INTERFACE_LINK_LIBRARIES BZip2::BZip2)
endif()
