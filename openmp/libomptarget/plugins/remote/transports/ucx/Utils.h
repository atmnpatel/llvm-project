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

#include "BaseUtils.h"
#include "messages.pb.h"
#include "omptarget.h"
#include "ucp/api/ucp.h"
#include "ucs/type/status.h"
#include "llvm/ADT/DenseMap.h"
#include <arpa/inet.h>
#include <cstring>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#define TAG_MASK 0x0fffffffffffff

#define ERR(...) llvm::report_fatal_error("Fatal error");
  // llvm::report_fatal_error(llvm::formatv(__VA_ARGS__).str());

namespace transport::ucx {

enum class SerializerType { Custom, Protobuf };

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
  ConnectionConfigTy(std::string Addr);
  void dump() const;
};

struct ManagerConfigTy {
  std::vector<ConnectionConfigTy> ConnectionConfigs;
  uint64_t BufferSize;

  ManagerConfigTy();
  void dump() const;
};

const uint16_t PortStringLength = 8;
const uint16_t IPStringLength = 50;

std::string getIP(const sockaddr_storage *SocketAddress);
std::string getPort(const sockaddr_storage *SocketAddress);

} // namespace transport::ucx
