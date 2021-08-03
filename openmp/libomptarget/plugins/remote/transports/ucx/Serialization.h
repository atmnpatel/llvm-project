#pragma once

#include "Utils.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "llvm/ADT/AllocatorList.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormatVariadic.h"

#include "llvm/Support/Allocator.h"

#include <forward_list>
#include <functional>
#include <list>

#include "omptarget.h"
#include "ucx.pb.h"

namespace transport::ucx {

using namespace openmp::libomptarget::ucx;

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

namespace custom {
class MessageTy {
protected:
  size_t MessageSize = 0;
  char *Buffer;
  char *CurBuffer;

  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  void serialize(T &Value) {
    std::memcpy((void *)((CurBuffer += sizeof(Value)) - sizeof(Value)), &Value,
                sizeof(Value));
  }

  void serialize(uintptr_t Value);
  void serialize(char *String);
  void serialize(__tgt_offload_entry *Entry);
  void serialize(__tgt_device_image *Image);
  void serialize(void *BufferStart, void *BufferEnd);

  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  void deserialize(T &Value) {
    std::memcpy(&Value, (CurBuffer += sizeof(Value)) - sizeof(Value),
                sizeof(Value));
  }

  void *deserializePointer();
  void deserialize(char *&String);
  void deserialize(__tgt_offload_entry *&Entry);
  void deserialize(__tgt_device_image *&Image);
  void deserialize(void *&BufferStart, void *&BufferEnd);

public:
  MessageTy(bool Empty = false);
  MessageTy(size_t Size);
  MessageTy(char * MessageBuffer);
  virtual ~MessageTy() = default;
  std::pair<char *, size_t> getBuffer();
};

struct I32 : public MessageTy {
  int32_t Value;

  explicit I32(int32_t Value);
  I32(std::string MessageBuffer);
};

struct I64 : public MessageTy {
  int64_t Value;

  explicit I64(int64_t Value);
  I64(std::string MessageBuffer);
};

struct Pointer : public MessageTy {
  void *Value;

  explicit Pointer(uintptr_t Value);
  Pointer(std::string MessageBuffer);
};

struct TargetBinaryDescription : public MessageTy {
  explicit TargetBinaryDescription(__tgt_bin_desc *TBD);
  TargetBinaryDescription(std::string &MessageBuffer, __tgt_bin_desc * TBD,
                          std::unordered_map<const void *, __tgt_device_image *>
                              &HostToRemoteDeviceImage);
};

struct Binary : public MessageTy {
  int32_t DeviceId;
  void *Image;

  Binary(int32_t DeviceId, __tgt_device_image *Image);
  Binary(std::string MessageBuffer);
};

struct TargetTable : public MessageTy {
  __tgt_target_table *Table;

  TargetTable(__tgt_target_table *Table);
  TargetTable(std::string MessageBuffer);
};

struct DataAlloc : public MessageTy {
  int32_t DeviceId;
  int64_t AllocSize;
  void *HstPtr;

  DataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr);
  DataAlloc(std::string MessageBuffer);
};

struct DataDelete : public MessageTy {
  int32_t DeviceId;
  void *TgtPtr;

  DataDelete(int32_t DeviceId, void *TgtPtr);
  DataDelete(std::string MessageBuffer);
};

struct DataSubmit : public MessageTy {
  int32_t DeviceId;
  void *TgtPtr, *HstPtr;
  int64_t DataSize;

  DataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr, int64_t DataSize);
  DataSubmit(std::string MessageBuffer);
};

struct DataRetrieve : public MessageTy {
  int32_t DeviceId;
  void *HstPtr, *TgtPtr;
  int64_t DataSize;

  DataRetrieve(int32_t DeviceId, void *HstPtr, void *TgtPtr, int64_t DataSize);
  DataRetrieve(std::string MessageBuffer);
};

struct Data : public MessageTy {
  int32_t Value;
  void *DataBuffer;
  size_t DataSize;

  Data(int32_t Value, char *DataBuffer, size_t DataSize);
  Data(std::string MessageBuffer);
};

struct TargetRegion : public MessageTy {
  int32_t DeviceId;
  void *TgtEntryPtr;
  void **TgtArgs;
  ptrdiff_t *TgtOffsets;
  int32_t ArgNum;

  TargetRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
               ptrdiff_t *TgtOffsets, int32_t ArgNum);
  TargetRegion(std::string MessageBuffer);
};

struct TargetTeamRegion : public MessageTy {
  int32_t DeviceId;
  void *TgtEntryPtr;
  void **TgtArgs;
  ptrdiff_t *TgtOffsets;
  int32_t ArgNum;
  int32_t TeamNum;
  int32_t ThreadLimit;
  uint64_t LoopTripCount;

  TargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
                   ptrdiff_t *TgtOffsets, int32_t ArgNum, int32_t TeamNum,
                   int32_t ThreadLimit, uint64_t LoopTripCount);
  TargetTeamRegion(std::string MessageBuffer);
};
} // namespace custom

/// Unload a target binary description from protobuf. The map is used to keep
/// track of already copied device images.
void unloadTargetBinaryDescription(
    const openmp::libomptarget::ucx::TargetBinaryDescription *Request,
    __tgt_bin_desc *&Desc,
    std::unordered_map<const void *, __tgt_device_image *>
        &HostToRemoteDeviceImage);

/// Loads tgt_target_table into a TargetTable protobuf message.
void loadTargetTable(__tgt_target_table *Table,
                     openmp::libomptarget::ucx::TargetTable &TableResponse,
                     __tgt_device_image *Image);

/// Copies from TargetOffloadEntry protobuf to a tgt_bin_desc during unloading.
void copyOffloadEntry(
    const openmp::libomptarget::ucx::TargetOffloadEntry &EntryResponse,
    __tgt_offload_entry *Entry);

/// Copies from tgt_bin_desc into TargetOffloadEntry protobuf during loading.
void copyOffloadEntry(
    const __tgt_offload_entry *Entry,
    openmp::libomptarget::ucx::TargetOffloadEntry *EntryResponse);

/// Shallow copy of offload entry from tgt_bin_desc to TargetOffloadEntry
/// during loading.
void shallowCopyOffloadEntry(
    const __tgt_offload_entry *Entry,
    openmp::libomptarget::ucx::TargetOffloadEntry *EntryResponse);

/// Copies DeviceOffloadEntries into table during unloading.
void copyOffloadEntry(const transport::ucx::DeviceOffloadEntry &EntryResponse,
                      __tgt_offload_entry *Entry);

/// Loads a target binary description into protobuf.
void loadTargetBinaryDescription(
    const __tgt_bin_desc *Desc,
    openmp::libomptarget::ucx::TargetBinaryDescription &Request);

/// Unloads from a target_table from protobuf.
void unloadTargetTable(
    openmp::libomptarget::ucx::TargetTable &TableResponse,
    __tgt_target_table *Table,
    std::unordered_map<void *, void *> &HostToRemoteTargetTableMap);

} // namespace transport::ucx
