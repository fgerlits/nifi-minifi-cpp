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

function(use_bundled_poco SOURCE_DIR BINARY_DIR)
    message("Using bundled poco")

    # Define byproduct
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(BYPRODUCT_SUFFIX "d")
    else()
        set(BYPRODUCT_SUFFIX "")
    endif()
    if (WIN32)
        set(BYPRODUCT_EXT "lib")
    else()
        set(BYPRODUCT_EXT "a")
    endif()
    set(BYPRODUCT "lib/libPocoFoundation${BYPRODUCT_SUFFIX}.${BYPRODUCT_EXT}")

    # Set build options
    set(POCO_BIN_DIR "${BINARY_DIR}/thirdparty/poco-install" CACHE STRING "" FORCE)

    set(POCO_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
                "-DCMAKE_INSTALL_PREFIX=${POCO_BIN_DIR}"
                "-DBUILD_SHARED_LIBS=OFF"
                "-DENABLE_ENCODINGS=OFF"
                "-DENABLE_XML=OFF"
                "-DENABLE_JSON=FALSE"
                "-DENABLE_MONGODB=OFF"
                "-DENABLE_DATA_SQLITE=OFF"
                "-DENABLE_REDIS=OFF"
                "-DENABLE_UTIL=OFF"
                "-DENABLE_NET=OFF"
                "-DENABLE_ZIP=OFF"
                "-DENABLE_PAGECOMPILER=OFF"
                "-DENABLE_PAGECOMPILER_FILE2PAGE=OFF"
                "-DENABLE_NETSSL=OFF"
                "-DENABLE_CRYPTO=OFF"
                "-DENABLE_JWT=OFF")

    # Build project
    set(POCO_GITHUB_RELEASE_URL https://github.com/pocoproject/poco/archive/poco-1.10.1-release.tar.gz)
    set(POCO_GITHUB_RELEASE_HASH "SHA256=44592a488d2830c0b4f3bfe4ae41f0c46abbfad49828d938714444e858a00818")

    ExternalProject_Add(
            poco-external
            URL "${POCO_GITHUB_RELEASE_URL}"
            URL_HASH "${POCO_GITHUB_RELEASE_HASH}"
            CMAKE_ARGS ${POCO_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(POCO_FOUND "YES" CACHE STRING "" FORCE)
    set(POCO_INCLUDE_DIRS "${POCO_BIN_DIR}/include" CACHE STRING "" FORCE)
    set(POCO_LIBRARIES "${POCO_BIN_DIR}/${BYPRODUCT}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_POCO_INCLUDE_DIRS=${POCO_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_POCO_LIBRARIES=${POCO_LIBRARIES}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${POCO_INCLUDE_DIRS})

    add_library(poco STATIC IMPORTED)
    set_target_properties(poco PROPERTIES IMPORTED_LOCATION "${POCO_LIBRARIES}")
    add_dependencies(poco poco-external)
    set_property(TARGET poco APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${POCO_INCLUDE_DIRS}")
endfunction(use_bundled_poco)
