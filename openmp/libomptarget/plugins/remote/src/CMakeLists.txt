##===----------------------------------------------------------------------===##
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
##===----------------------------------------------------------------------===##
#
# Build a plugin for remote offloading.
#
##===----------------------------------------------------------------------===##

cmake_minimum_required(VERSION 3.15)

# Define the suffix for the runtime messaging dumps.
add_definitions(-DTARGET_NAME=REMOTE)

include_directories(${LIBOMPTARGET_SRC_DIR})
include_directories(${LIBOMPTARGET_INCLUDE_DIR})
include_directories(${GRPC_INCLUDE_DIR})
include_directories(${RPC_INCLUDE_DIR})
include_directories(${TRANSPORTS_DIR})
include_directories(${UCX_DIR}/include)
include_directories(${GRPC_DIR}/include)

set(LLVM_LINK_COMPONENTS Support)

add_llvm_library(omptarget.rtl.remote SHARED
        ${LIBOMPTARGET_SRC_FILES}
        ${GRPC_SRC_FILES}
        ${TRANSPORTS_DIR}/grpc/Client.cpp
        ${TRANSPORTS_DIR}/ucx/Base.cpp
        ${TRANSPORTS_DIR}/ucx/Client.cpp
        ${TRANSPORTS_DIR}/ucx/Serialization.cpp
        ${TRANSPORTS_DIR}/ucx/Serializer.cpp
        ${TRANSPORTS_DIR}/ucx/Utils.cpp
        BaseClient.cpp
        Utils.cpp
        rtl.cpp
        )

llvm_update_compile_flags(omptarget.rtl.remote)

target_compile_features(omptarget.rtl.remote PRIVATE cxx_std_20)

# Install plugin under the lib destination folder.
install(TARGETS omptarget.rtl.remote LIBRARY DESTINATION "${OPENMP_INSTALL_LIBDIR}")

target_link_libraries(omptarget.rtl.remote PRIVATE
        protobuf
        grpc++
        dl
        ucp
        ucs
        uct
        absl::synchronization
        "-L ${GRPC_DIR}/lib"
        "-L ${UCX_DIR}/lib"
        ${OPENMP_PTHREAD_LIB}
        # "-ggdb"
        # "-g"
        # "-fno-omit-frame-pointer"
        "-O3"
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/../../exports"
        )

# Report to the parent scope that we are building a plugin for RPC.
set(LIBOMPTARGET_SYSTEM_TARGETS "${LIBOMPTARGET_SYSTEM_TARGETS} remote" PARENT_SCOPE)
