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
include_directories(${UCX_DIR}/include)
include_directories(${GRPC_DIR}/include)

set(LLVM_LINK_COMPONENTS Support)

add_llvm_executable(openmp-offloading-server
        ${LIBOMPTARGET_SRC_FILES}
        ${GRPC_SRC_FILES}
        ${TRANSPORTS_DIR}/grpc/Server.cpp
        ${TRANSPORTS_DIR}/ucx/Server.cpp
        ${TRANSPORTS_DIR}/ucx/Base.cpp
        ${TRANSPORTS_DIR}/ucx/Serialization.cpp
        ${TRANSPORTS_DIR}/ucx/Serializer.cpp
        ${TRANSPORTS_DIR}/ucx/Utils.cpp
        OffloadingServer.cpp
        ../src/Utils.cpp
        )

llvm_update_compile_flags(openmp-offloading-server)

target_compile_features(openmp-offloading-server PRIVATE cxx_std_20)

add_dependencies(openmp-offloading-server omptarget)

target_link_directories(openmp-offloading-server PRIVATE ${OPENMP_INSTALL_LIBDIR})

target_link_libraries(openmp-offloading-server PRIVATE
        protobuf
        grpc++
        dl
        ucp
        ucs
        uct
        absl::synchronization
        omptarget
        "-L ${GRPC_DIR}/lib"
        "-L ${UCX_DIR}/lib"
        ${OPENMP_PTHREAD_LIB}
        # "-ggdb"
        # "-g"
        "-O3 -fno-omit-frame-pointer"
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/../../exports"
        )
