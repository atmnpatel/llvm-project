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

#define CLIENT_DBG(...)                                                        \
  {                                                                            \
    if (DebugLevel > 0) {                                                      \
      fprintf(stderr, "[[Client]] --> ");                                      \
      fprintf(stderr, __VA_ARGS__);                                            \
      fprintf(stderr, "\n");                                                   \
    }                                                                          \
  }

#define SERVER_DBG(...)                                                        \
  {                                                                            \
    if (DebugLevel > 0) {                                                      \
      fprintf(stderr, "[[Server]] --> ");                                      \
      fprintf(stderr, __VA_ARGS__);                                            \
      fprintf(stderr, "\n");                                                   \
    }                                                                          \
  }

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

namespace transport {
namespace ucx {

struct MemoryTy {
  void *Addr;
  size_t Size;
};

static const size_t SlabSize = 1 << 20;
using AllocatorTyy =
    llvm::BumpPtrAllocatorImpl<llvm::MallocAllocator, SlabSize, SlabSize, 128>;

struct AllocatorTy {
  AllocatorTyy Allocator;
  std::mutex AllocatorMtx;
  void *Allocate(size_t Size, size_t Alignment) {
    std::lock_guard Guard(AllocatorMtx);
    return Allocator.Allocate(Size, Alignment);
  }
};

struct RPCConfig {
  std::vector<std::string> ServerAddresses;
  uint64_t MaxSize;
  uint64_t BlockSize;
  RPCConfig() {
    ServerAddresses = {"0.0.0.0:50051"};
    MaxSize = 1 << 30;
    BlockSize = 1 << 20;

    // TODO: Error handle for incorrect inputs
    if (const char *Env = std::getenv("LIBOMPTARGET_RPC_ADDRESS")) {
      ServerAddresses.clear();
      std::string AddressString = Env;
      const std::string Delimiter = ",";

      size_t Pos = 0;
      std::string Token;
      while ((Pos = AddressString.find(Delimiter)) != std::string::npos) {
        Token = AddressString.substr(0, Pos);
        ServerAddresses.push_back(Token);
        AddressString.erase(0, Pos + Delimiter.length());
      }
      ServerAddresses.push_back(AddressString);
    }
    if (const char *Env = std::getenv("LIBOMPTARGET_RPC_ALLOCATOR_MAX"))
      MaxSize = std::stoi(Env);
    if (const char *Env = std::getenv("LIBOMPTARGET_RPC_BLOCK_SIZE"))
      BlockSize = std::stoi(Env);
  }
};

void parseEnvironment(RPCConfig &Config);

const uint16_t PortStringLength = 8;
const uint16_t IPStringLength = 50;

struct DataSubmitTy {
  int32_t DeviceId;
  void *HstPtr;
  void *TgtPtr;
  int64_t Size;
};

struct RunTargetTeamRegionTy {
  int32_t DeviceId;
  void *TgtEntryPtr;
  void **TgtArgs;
  ptrdiff_t *TgtOffsets;
  int32_t ArgNum;
  int32_t TeamNum;
  int32_t ThreadLimit;
  uint64_t LoopTripCount;
};

struct RunTargetRegionTy {
  int32_t DeviceId;
  void *TgtEntryPtr;
  void **TgtArgs;
  ptrdiff_t *TgtOffsets;
  int32_t ArgNum;
};

struct SlabTy {
  char *Begin;
  char *Cur;
  size_t Size = 0;
  SlabTy() {}
  SlabTy(char *Begin, char *Cur, size_t Size = 0)
      : Begin(Begin), Cur(Cur), Size(Size) {}
};

using SlabListTy = std::deque<SlabTy>;

struct RequestStatus {
  int Complete;
};

/* Struct to hold the handle for asynchronous transmissions */
struct SendFutureTy {
  RequestStatus *Request;
  RequestStatus *Context;
  const char *Message;
  SendFutureTy(RequestStatus *Request, RequestStatus *Context, const char *Message)
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

struct ConnectionConfigTy {
  std::string Address;
  uint16_t Port;

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

      size_t Pos;
      std::string Token;

      do {
        Pos = (AddressString.find(Delimiter) != std::string::npos)
                  ? AddressString.find(Delimiter)
                  : AddressString.length();
        Token = AddressString.substr(0, Pos);
        ConnectionConfigs.push_back(Token);
        AddressString.erase(0, Pos + Delimiter.length());
      } while ((Pos = AddressString.find(Delimiter)) != std::string::npos);
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

void dump(char *Begin, int32_t Size, const std::string &Title = "");
void dump(int Offset, char *Begin, char *End);
void dump(const char *Begin, const char *End, const std::string &Title = "");

} // namespace ucx
} // namespace transport

namespace llvm {
template <> struct DenseMapInfo<transport::ucx::MemoryTy> {
  static transport::ucx::MemoryTy getEmptyKey() {
    return {nullptr, static_cast<size_t>(~0)};
  }

  static transport::ucx::MemoryTy getTombstoneKey() {
    return {nullptr, static_cast<size_t>(~1)};
  }

  static unsigned getHashValue(const transport::ucx::MemoryTy &Base) {
    return std::hash<void *>()(Base.Addr) ^ std::hash<size_t>()(Base.Size);
  }

  static bool isEqual(const transport::ucx::MemoryTy &LHS,
                      const transport::ucx::MemoryTy &RHS) {
    return LHS.Addr == RHS.Addr && LHS.Size == RHS.Size;
  }
};
} // namespace llvm
