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
  std::string Message;
  SendFutureTy(std::string Message) : Request(nullptr), Context(new RequestStatus()), Message(Message) {}

  ~SendFutureTy() {
    delete Context;
  }
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

enum MessageKind : char {
  RegisterLib,
  UnregisterLib,
  IsValidBinary,
  GetNumberOfDevices,
  InitDevice,
  InitRequires,
  LoadBinary,
  DataAlloc,
  DataDelete,
  DataSubmit,
  DataRetrieve,
  RunTargetRegion,
  RunTargetTeamRegion,
  Count
};

// Allocator adaptor that interposes construct() calls to
// convert value initialization into default initialization.
template <typename T, typename A=std::allocator<T>>
class default_init_allocator : public A {
  typedef std::allocator_traits<A> a_t;
public:
  template <typename U> struct rebind {
    using other =
        default_init_allocator<
            U, typename a_t::template rebind_alloc<U>
            >;
  };

  constexpr T* allocate(size_t n) {
    return new T[n];
  }
};

using MessageBufferTy = std::basic_string<char, std::char_traits<char>, default_init_allocator<char>>;

struct MessageTy {
  MessageKind Kind;
  MessageBufferTy Buffer;
};

} // namespace transport::ucx
