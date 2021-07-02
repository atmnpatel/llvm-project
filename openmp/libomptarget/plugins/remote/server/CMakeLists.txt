##===----------------------------------------------------------------------===##
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
##===----------------------------------------------------------------------===##
#
# Build server for remote offloading.
#
##===----------------------------------------------------------------------===##

include_directories(${LIBOMPTARGET_SRC_DIR})
include_directories(${LIBOMPTARGET_INCLUDE_DIR})
include_directories(${GRPC_INCLUDE_DIR})
include_directories(${RPC_INCLUDE_DIR})
include_directories(${TRANSPORTS_DIR})
include_directories(/home/apatel/dev/ucx/install/include)

set(LLVM_LINK_COMPONENTS
        Support
        )

add_llvm_executable(openmp-offloading-server
        ${LIBOMPTARGET_SRC_FILES}
        OffloadingServer.cpp
        ${TRANSPORTS_DIR}/grpc/Client.cpp
        ${TRANSPORTS_DIR}/grpc/Server.cpp
        ${TRANSPORTS_DIR}/grpc/Utils.cpp
        ${GRPC_SRC_FILES}
        )

llvm_update_compile_flags(openmp-offloading-server)

target_compile_features(openmp-offloading-server PRIVATE cxx_std_20)

target_link_libraries(openmp-offloading-server PRIVATE
        protobuf
        grpc++
        dl
        ${OPENMP_PTHREAD_LIB}
        "-fopenmp"
        "-g -fno-omit-frame-pointer"
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/../../exports"
        )
