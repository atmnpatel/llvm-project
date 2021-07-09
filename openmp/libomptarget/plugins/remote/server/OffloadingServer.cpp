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

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <iostream>
#include <thread>

#include "grpc/Server.h"
#include "ucx/Server.h"

int main() {
  auto *Protocol = std::getenv("LIBOMPTARGET_RPC_PROTOCOL");

  if (!Protocol || !strcmp(Protocol, "gRPC")) {
    transport::grpc::ClientManagerConfigTy Config;

    transport::grpc::RemoteOffloadImpl Service(Config.MaxSize, Config.BlockSize);

    grpc::ServerBuilder Builder;
    Builder.AddListeningPort(Config.ServerAddresses[0],
                             grpc::InsecureServerCredentials());
    Builder.RegisterService(&Service);
    Builder.SetMaxMessageSize(INT_MAX);
    std::unique_ptr<grpc::Server> Server(Builder.BuildAndStart());
    if (getDebugLevel())
      std::cerr << "Server listening on " << Config.ServerAddresses[0]
                << std::endl;

    Server->Wait();

    return 0;
  }

  if (!strcmp(Protocol, "UCX")) {
    transport::ucx::Server *Server;

    auto *Serialization = std::getenv("LIBOMPTARGET_RPC_SERIALIZATION");
    if (!Serialization || !strcmp(Serialization, "Custom"))
      Server = (transport::ucx::Server *) new transport::ucx::CustomServer;
    else if (!strcmp(Serialization, "Protobuf"))
      Server = (transport::ucx::Server *) new transport::ucx::ProtobufServer;
    else
      llvm::report_fatal_error("Invalid Serialization Option");

    auto *ConnectionInfoStr = std::getenv("LIBOMPTARGET_RPC_ADDRESS");
    auto ConnectionInfo = ConnectionInfoStr ? std::string(ConnectionInfoStr) : ":13337";
    auto Config = transport::ucx::ConnectionConfigTy(ConnectionInfo);

    Server->listenForConnections(Config);

    return 0;
  }
}
