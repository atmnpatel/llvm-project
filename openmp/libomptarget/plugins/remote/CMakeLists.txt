##===----------------------------------------------------------------------===##
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
##===----------------------------------------------------------------------===##
#
# Build a plugin (client) and server for remote offloading.
#
##===----------------------------------------------------------------------===#

if (NOT(CMAKE_SYSTEM_NAME MATCHES "Linux"))
  libomptarget_say("Not building remote offloading plugin: only support Linux hosts.")
  return()
endif()

if (NOT(LIBOMPTARGET_ENABLE_EXPERIMENTAL_REMOTE_PLUGIN))
  return()
endif()

find_package(Protobuf)
find_package(gRPC CONFIG)

if (Protobuf_FOUND AND gRPC_FOUND)
  libomptarget_say("Building remote offloading plugin.")

  set(GRPC_SRC_FILES
      ${CMAKE_CURRENT_SOURCE_DIR}/src/openmp.grpc.pb.cc
      ${CMAKE_CURRENT_SOURCE_DIR}/src/openmp.pb.cc
  )

  set(GRPC_INCLUDE_DIR
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  set(RPC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/)
  set(RPC_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/)
  
  add_subdirectory(src)
  add_subdirectory(server)
else()
  libomptarget_say("Not building remote offloading plugin: required libraries were not found.")
endif()

