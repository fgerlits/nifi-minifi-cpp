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

set(LIBARCHIVE_STATIC CACHE INTERNAL 1)
set(ENABLE_MBEDTLS CACHE INTERNAL OFF)
set(ENABLE_NETTLE CACHE INTERNAL OFF)
set(ENABLE_LIBB2 CACHE INTERNAL OFF)
set(ENABLE_LZ4 CACHE INTERNAL OFF)
set(ENABLE_LZO CACHE INTERNAL OFF)
set(ENABLE_ZSTD CACHE INTERNAL OFF)
set(ENABLE_ZLIB CACHE INTERNAL ON)
set(ENABLE_LIBXML2 CACHE INTERNAL OFF)
set(ENABLE_EXPAT CACHE INTERNAL OFF)
set(ENABLE_PCREPOSIX CACHE INTERNAL OFF)
set(ENABLE_TAR CACHE INTERNAL OFF) # This does not disable the tar format, just the standalone tar command line utility
set(ENABLE_CPIO CACHE INTERNAL OFF)
set(ENABLE_CAT CACHE INTERNAL OFF)
set(ENABLE_XATTR CACHE INTERNAL ON)
set(ENABLE_ACL CACHE INTERNAL ON)
set(ENABLE_ICONV CACHE INTERNAL OFF)
set(ENABLE_TEST CACHE INTERNAL OFF)
set(ENABLE_WERROR CACHE INTERNAL OFF)
set(ENABLE_OPENSSL CACHE INTERNAL ON)

if (ENABLE_LZMA)
    set(ENABLE_LZMA CACHE INTERNAL ON)
else()
    set(ENABLE_LZMA CACHE INTERNAL OFF)
endif()

if (ENABLE_BZIP2)
    set(ENABLE_BZip2 CACHE INTERNAL ON)
else()
    set(ENABLE_BZip2 CACHE INTERNAL OFF)
endif()

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/libarchive/libarchive.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(libarchive
        URL "https://github.com/libarchive/libarchive/releases/download/v3.4.2/libarchive-3.4.2.tar.gz"
        URL_HASH "SHA256=b60d58d12632ecf1e8fad7316dc82c6b9738a35625746b47ecdcaf4aed176176"
        PATCH_COMMAND ${PC})

FetchContent_MakeAvailable(libarchive)

add_library(libarchive INTERFACE)
add_library(LibArchive::LibArchive ALIAS libarchive)
