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
  std::atomic<bool> *IsCompleted;
  SendFutureTy(RequestStatus *Request, RequestStatus *Context,
               std::atomic<bool> *IsCompleted)
      : Request(Request), Context(Context), IsCompleted(IsCompleted) {}
  SendFutureTy()
      : Request(nullptr), Context(nullptr), IsCompleted(nullptr) {}
};

struct SendFutureHandleTy {
  uint64_t Tag;
  std::atomic<bool> *IsCompleted;
};

struct SendTaskTy {
  char Kind;
  std::string Buffer;
  std::atomic<bool> *Completed;
  uint64_t Tag;
  SendTaskTy(char Kind, std::string Buffer, std::atomic<bool> *Completed,
             uint64_t Tag)
      : Kind(Kind), Buffer(Buffer), Completed(Completed), Tag(Tag) {}
};

struct RecvTaskTy {
  uint64_t Tag;
  std::string *Message;
  std::atomic<bool> *Completed;
  uint64_t *TagHandle;
  RecvTaskTy(uint64_t Tag, std::string *Message, std::atomic<bool> *Completed,
             uint64_t *TagHandle)
      : Tag(Tag), Message(Message), Completed(Completed), TagHandle(TagHandle) {
  }
};

struct RecvFutureHandleTy {
  std::string *Buffer;
  std::atomic<bool> *IsCompleted;
  uint64_t *Tag;
};

/* Struct to hold the handle for asynchronous transmissions */
struct RecvFutureTy {
  char Kind;
  RequestStatus *Request;
  std::string *Message;
  std::atomic<bool> *IsCompleted;
  RecvFutureTy(char Kind, RequestStatus *Request, std::string *Message,
               std::atomic<bool> *IsCompleted)
      : Kind(Kind), Request(Request), Message(Message),
        IsCompleted(IsCompleted) {}
  RecvFutureTy() : Kind(0), Request(nullptr), Message(nullptr) {}
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

struct ServerContextTy {
  std::vector<ucp_conn_request_h> ConnRequests;
  ucp_listener_h Listener;
};

} // namespace transport::ucx
