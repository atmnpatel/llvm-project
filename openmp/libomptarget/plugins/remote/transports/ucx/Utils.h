//===----------------- Utils.h - Utilities for Remote RTL -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "omptarget.h"
#include <arpa/inet.h>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>

#include "BaseUtils.h"
#include "ucx.pb.h"
#include "llvm/ADT/AllocatorList.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/AllocatorBase.h"

#include <sstream>
#include <thread>
#include <ucx.pb.h>

#define TAG_MASK 0x0fffffffffffff

#define ERR(...) llvm::report_fatal_error(llvm::formatv(__VA_ARGS__).str());

namespace transport::ucx {

enum class SerializerType {
  Custom,
  Protobuf
};

const uint16_t PortStringLength = 8;
const uint16_t IPStringLength = 50;

struct RequestStatus {
  int Complete;
};

/* Struct to hold the handle for asynchronous transmissions */
struct SendFutureTy {
  RequestStatus *Request;
  RequestStatus *Context;
  const char *Message;
  SendFutureTy(RequestStatus *Request, RequestStatus *Context,
               const char *Message)
      : Request(Request), Context(Context), Message(Message) {}
  SendFutureTy() : Request(nullptr), Context(nullptr), Message(nullptr) {}
};

/* Struct to hold the handle for asynchronous transmissions */
struct ReceiveFutureTy {
  RequestStatus *Request;
  char *Message;
  ReceiveFutureTy(RequestStatus *Request, char *Message)
      : Request(Request), Message(Message) {}
  ReceiveFutureTy() : Request(nullptr), Message(nullptr) {}
};

extern std::vector<std::string> MessageKindToString;

struct ConnectionConfigTy {
  std::string Address;
  uint16_t Port;

  ConnectionConfigTy() = default;

  ConnectionConfigTy(std::string Addr) {
    const std::string Delimiter = ":";
    size_t Pos;
    if ((Pos = Addr.find(Delimiter)) != std::string::npos) {
      Address = Addr.substr(0, Pos);
      Port = std::stoi(Addr.substr(Pos + 1, Addr.length() - Pos));
    }
  }

  void dump() const { printf("  Connection: %s:%d\n", Address.c_str(), Port); }
};

struct ManagerConfigTy {
  std::vector<ConnectionConfigTy> ConnectionConfigs;
  uint64_t BufferSize;

  ManagerConfigTy() {
    if (const char *Env = std::getenv("LIBOMPTARGET_RPC_ADDRESS")) {
      std::string AddressString = Env;
      const std::string Delimiter = ",";

      do {
        auto Pos = (AddressString.find(Delimiter) != std::string::npos)
                       ? AddressString.find(Delimiter)
                       : AddressString.length();
        auto Token = AddressString.substr(0, Pos);
        ConnectionConfigs.emplace_back(Token);
        AddressString.erase(0, Pos + Delimiter.length());
      } while (!AddressString.empty());
    } else
      ConnectionConfigs.emplace_back("0.0.0.0:13337");

    if (const char *Env = std::getenv("LIBOMPTARGET_RPC_BLOCK_SIZE"))
      BufferSize = std::stoi(Env);
    else
      BufferSize = 1 << 20;
  }

  void dump() const {
    printf("Manager Config Dump:\n");
    printf("  Buffer Size: %lu\n", BufferSize);
    for (const auto &ConnectionConfig : ConnectionConfigs)
      ConnectionConfig.dump();
  }
};

std::string getIP(const sockaddr_storage *SocketAddress);
std::string getPort(const sockaddr_storage *SocketAddress);

} // namespace transport::ucx