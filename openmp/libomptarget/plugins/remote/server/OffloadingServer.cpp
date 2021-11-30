//===------------- OffloadingServer.cpp - Server Application --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Offloading server for remote host.
//
//===----------------------------------------------------------------------===//

#include <future>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <iostream>
#include <thread>

#include "grpc/Server.h"
#include "ucx/Server.h"

std::promise<void> ShutdownPromise;

int main() {
  auto *Transport = std::getenv("LIBOMPTARGET_RPC_TRANSPORT");

  // Prevent hierarchical offloading
  PM->RTLs.BlocklistedRTLs = {"libomptarget.rtl.remote.so"};

  // Initialize Devices
  std::call_once(PM->RTLs.initFlag, &RTLsTy::LoadRTLs, &PM->RTLs);

  if (!Transport || !strcmp(Transport, "gRPC")) {
    transport::grpc::ClientManagerConfigTy Config;

    transport::grpc::RemoteOffloadImpl Service(Config.MaxSize,
                                               Config.BlockSize);

    grpc::ServerBuilder Builder;
    Builder.AddListeningPort(Config.ServerAddresses[0],
                             grpc::InsecureServerCredentials());
    Builder.RegisterService(&Service);
    Builder.SetMaxMessageSize(INT_MAX);
    std::unique_ptr<grpc::Server> Server(Builder.BuildAndStart());
    if (getDebugLevel())
      std::cerr << "Server listening on " << Config.ServerAddresses[0]
                << std::endl;

    auto WaitForServer = [&]() { Server->Wait(); };

    std::thread ServerThread(WaitForServer);

    auto ShutdownFuture = ShutdownPromise.get_future();
    ShutdownFuture.wait();
    Server->Shutdown();
    ServerThread.join();

    return 0;
  }

  if (!strcmp(Transport, "UCX")) {
    transport::ucx::ServerTy *Server;

    auto *Serialization = std::getenv("LIBOMPTARGET_RPC_SERIALIZATION");
    if (!Serialization || !strcmp(Serialization, "Custom"))
      Server = new transport::ucx::ServerTy(transport::ucx::SerializerType::Custom);
    else if (!strcmp(Serialization, "Protobuf"))
      Server = new transport::ucx::ServerTy(transport::ucx::SerializerType::Protobuf);
    else
      ERR("Invalid Serialization Option")

    auto *ConnectionInfoStr = std::getenv("LIBOMPTARGET_RPC_ADDRESS");
    auto ConnectionInfo =
        ConnectionInfoStr ? std::string(ConnectionInfoStr) : ":50051";
    auto Config = transport::ucx::ConnectionConfigTy(ConnectionInfo);

    Server->listenForConnections(Config);

    return 0;
  }
}
