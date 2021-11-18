#pragma once

#include <cstdio>
#include "messages.pb.h"
#include "omptarget.h"

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

/// Loads a target binary description into protobuf.
void loadTargetBinaryDescription(const __tgt_bin_desc *Desc,
                                 transport::messages::TargetBinaryDescription &Request);

/// Unload a target binary description from protobuf. The map is used to keep
/// track of already copied device images.
void unloadTargetBinaryDescription(
    const transport::messages::TargetBinaryDescription *Request, __tgt_bin_desc *Desc,
    std::unordered_map<const void *, __tgt_device_image *>
    &HostToRemoteDeviceImage);

/// Frees argument as constructed by loadTargetBinaryDescription
void freeTargetBinaryDescription(__tgt_bin_desc *Desc);

/// Copies from TargetOffloadEntry protobuf to a tgt_bin_desc during unloading.
void copyOffloadEntry(const transport::messages::TargetOffloadEntry &EntryResponse,
                      __tgt_offload_entry *Entry);

/// Copies from tgt_bin_desc into TargetOffloadEntry protobuf during loading.
void copyOffloadEntry(const __tgt_offload_entry *Entry,
                      transport::messages::TargetOffloadEntry *EntryResponse);

/// Shallow copy of offload entry from tgt_bin_desc to TargetOffloadEntry
/// during loading.
void shallowCopyOffloadEntry(const __tgt_offload_entry *Entry,
                             transport::messages::TargetOffloadEntry *EntryResponse);

/// Copies DeviceOffloadEntries into table during unloading.
void copyOffloadEntry(const transport::messages::DeviceOffloadEntry &EntryResponse,
                      __tgt_offload_entry *Entry);

/// Loads tgt_target_table into a TargetTable protobuf message.
void loadTargetTable(const __tgt_target_table *Table, transport::messages::TargetTable &TableResponse);

/// Unloads from a target_table from protobuf.
void unloadTargetTable(
    const transport::messages::TargetTable &TableResponse, __tgt_target_table *Table,
    std::unordered_map<void *, void *> &HostToRemoteTargetTableMap);

/// Frees argument as constructed by unloadTargetTable
void freeTargetTable(__tgt_target_table *Table);

void dump(const __tgt_bin_desc *Desc);
void dump(std::string Buffer);
void dump(const char *Begin, int32_t Size, const std::string &Title = "");
void dump(int Offset, const char *Begin, const char *End);
void dump(const char *Begin, const char *End, const std::string &Title = "");
