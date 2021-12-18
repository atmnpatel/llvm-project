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
#include "messages.pb.h"
#include <string>

namespace transport::grpc {

using namespace transport::messages;

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

/// Loads a target binary description into protobuf.
void loadTargetBinaryDescription(const __tgt_bin_desc *Desc,
                                 TargetBinaryDescription &Request);

/// Unload a target binary description from protobuf. The map is used to keep
/// track of already copied device images.
void unloadTargetBinaryDescription(
    const TargetBinaryDescription *Request, __tgt_bin_desc *Desc,
    std::unordered_map<const void *, __tgt_device_image *>
    &HostToRemoteDeviceImage);

/// Frees argument as constructed by loadTargetBinaryDescription
void freeTargetBinaryDescription(__tgt_bin_desc *Desc);

/// Copies from TargetOffloadEntry protobuf to a tgt_bin_desc during unloading.
void copyOffloadEntry(const TargetOffloadEntry &EntryResponse,
                      __tgt_offload_entry *Entry);

/// Copies from tgt_bin_desc into TargetOffloadEntry protobuf during loading.
void copyOffloadEntry(const __tgt_offload_entry *Entry,
                      TargetOffloadEntry *EntryResponse);

/// Shallow copy of offload entry from tgt_bin_desc to TargetOffloadEntry
/// during loading.
void shallowCopyOffloadEntry(const __tgt_offload_entry *Entry,
                             TargetOffloadEntry *EntryResponse);

/// Copies DeviceOffloadEntries into table during unloading.
void copyOffloadEntry(const DeviceOffloadEntry &EntryResponse,
                      __tgt_offload_entry *Entry);

/// Loads tgt_target_table into a TargetTable protobuf message.
void loadTargetTable(const __tgt_target_table *Table, TargetTable &TableResponse);

/// Unloads from a target_table from protobuf.
void unloadTargetTable(
    const TargetTable &TableResponse, __tgt_target_table *Table,
    std::unordered_map<void *, void *> &HostToRemoteTargetTableMap);

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
