//===----------------- Utils.h - Utilities for Remote RTL -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Utilities for data transfer through protobuf and debugging.
//
//===----------------------------------------------------------------------===//

#ifndef UTILS_H
#define UTILS_H

#include "BaseUtils.h"
#include "Debug.h"
#include "omptarget.h"
#include "rtl.h"
#include <messages.pb.h>
#include <string>

namespace transport::grpc {

struct ClientManagerConfigTy {
  std::vector<std::string> ServerAddresses;
  uint64_t MaxSize;
  uint64_t BlockSize;
  int Timeout;

  ClientManagerConfigTy()
      : ServerAddresses({"0.0.0.0:50051"}), MaxSize(1 << 30),
        BlockSize(UINT64_MAX), Timeout(5) {
    // TODO: Error handle for incorrect inputs
    if (const char *Env = std::getenv("LIBOMPTARGET_RPC_ADDRESS")) {
      ServerAddresses.clear();
      std::string AddressString = Env;
      const std::string Delimiter = ",";

      size_t Pos;
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
    if (const char *Env1 = std::getenv("LIBOMPTARGET_RPC_LATENCY"))
      Timeout = std::stoi(Env1);
  }
};

void dump(const void *Start, const void *End);
void dump(__tgt_offload_entry *Entry);
void dump(transport::messages::TargetOffloadEntry Entry);
void dump(__tgt_target_table *Table);
void dump(__tgt_device_image *Image);

void dump(char *Begin, int32_t Size, const std::string &Title = "");
void dump(int Offset, char *Begin, char *End);
void dump(const char *Begin, const char *End, const std::string &Title = "");
} // namespace transport::grpc

#endif
