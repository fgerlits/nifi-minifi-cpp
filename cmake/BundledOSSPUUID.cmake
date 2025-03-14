#
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
#

function(use_bundled_osspuuid SOURCE_DIR BINARY_DIR)
    message("Using bundled ossp-uuid")

    set(PATCH_FILE1 "${SOURCE_DIR}/thirdparty/ossp-uuid/ossp-uuid-mac-fix.patch")
    set(PATCH_FILE2 "${SOURCE_DIR}/thirdparty/ossp-uuid/ossp-uuid-no-prog.patch")
    set(PATCH_FILE3 "${SOURCE_DIR}/thirdparty/ossp-uuid/ossp-uuid-update-config-guess.patch")
    set(PC ${Bash_EXECUTABLE} -c "set -x &&\
            (\"${Patch_EXECUTABLE}\" -p1 -R -s -f --dry-run -i \"${PATCH_FILE1}\" || \"${Patch_EXECUTABLE}\" -p1 -N -i \"${PATCH_FILE1}\") &&\
            (\"${Patch_EXECUTABLE}\" -p1 -R -s -f --dry-run -i \"${PATCH_FILE2}\" || \"${Patch_EXECUTABLE}\" -p1 -N -i \"${PATCH_FILE2}\") &&\
            (\"${Patch_EXECUTABLE}\" -p1 -R -s -f --dry-run -i \"${PATCH_FILE3}\" || \"${Patch_EXECUTABLE}\" -p1 -N -i \"${PATCH_FILE3}\") ")

    # Define byproducts
    set(BYPRODUCTS "lib/libuuid.a"
                   "lib/libuuid++.a")

    set(OSSPUUID_BIN_DIR "${BINARY_DIR}/thirdparty/ossp-uuid-install" CACHE STRING "" FORCE)

    FOREACH(BYPRODUCT ${BYPRODUCTS})
        LIST(APPEND OSSPUUID_LIBRARIES_LIST "${OSSPUUID_BIN_DIR}/${BYPRODUCT}")
    ENDFOREACH(BYPRODUCT)

    # Build project
    if(NOT WIN32)
        string(APPEND ADDITIONAL_COMPILER_FLAGS "-fPIC ")
    endif()
    if(APPLE)
        string(APPEND ADDITIONAL_COMPILER_FLAGS "-isysroot ${CMAKE_OSX_SYSROOT} ")
    endif()
    set(CONFIGURE_COMMAND ./configure "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}" "CFLAGS=${PASSTHROUGH_CMAKE_C_FLAGS} ${ADDITIONAL_COMPILER_FLAGS}" "CXXFLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS} ${ADDITIONAL_COMPILER_FLAGS}" --enable-shared=no --with-cxx --without-perl --without-php --without-pgsql "--prefix=${BINARY_DIR}/thirdparty/ossp-uuid-install")

    string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type)
    if(build_type MATCHES debug)
        list(APPEND CONFIGURE_COMMAND --enable-debug=yes)
    endif()

    ExternalProject_Add(
            ossp-uuid-external
            URL "https://deb.debian.org/debian/pool/main/o/ossp-uuid/ossp-uuid_1.6.2.orig.tar.gz"
            URL_HASH "SHA256=11a615225baa5f8bb686824423f50e4427acd3f70d394765bdff32801f0fd5b0"
            BUILD_IN_SOURCE true
            SOURCE_DIR "${BINARY_DIR}/thirdparty/ossp-uuid-src"
            BUILD_COMMAND make
            CMAKE_COMMAND ""
            UPDATE_COMMAND ""
            INSTALL_COMMAND make install
            BUILD_BYPRODUCTS ${OSSPUUID_LIBRARIES_LIST}
            CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
            PATCH_COMMAND ${PC}
            STEP_TARGETS build
            EXCLUDE_FROM_ALL TRUE
            DOWNLOAD_NO_PROGRESS TRUE
            TLS_VERIFY TRUE
    )

    # Set variables
    set(UUID_FOUND "YES" CACHE STRING "" FORCE)
    set(UUID_INCLUDE_DIRS "${OSSPUUID_BIN_DIR}/include" CACHE STRING "" FORCE)
    set(UUID_LIBRARY "${OSSPUUID_BIN_DIR}/lib/libuuid.a" CACHE STRING "" FORCE)
    set(UUID_CPP_LIBRARY "${OSSPUUID_BIN_DIR}/lib/libuuid++.a" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${UUID_INCLUDE_DIRS})

    add_library(OSSP::libuuid STATIC IMPORTED)
    set_target_properties(OSSP::libuuid PROPERTIES IMPORTED_LOCATION "${UUID_LIBRARY}")
    add_dependencies(OSSP::libuuid ossp-uuid-external)
    set_property(TARGET OSSP::libuuid APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${UUID_INCLUDE_DIRS}")

    add_library(OSSP::libuuid++ STATIC IMPORTED)
    set_target_properties(OSSP::libuuid++ PROPERTIES IMPORTED_LOCATION "${UUID_CPP_LIBRARY}")
    add_dependencies(OSSP::libuuid++ ossp-uuid-external)
    set_property(TARGET OSSP::libuuid++ APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${UUID_INCLUDE_DIRS}")
endfunction(use_bundled_osspuuid)
