#pragma once

#include "Utils.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormatVariadic.h"

#include "omptarget.h"

namespace transport::ucx::messages {

class BaseMessageTy {
protected:
  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  void serialize(T &Value) {
    *(T *)CurBuffer = Value;
    CurBuffer += sizeof(Value);
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
  BaseMessageTy() = default;
  BaseMessageTy(size_t Size);
  BaseMessageTy(std::string_view MessageBuffer);
  virtual ~BaseMessageTy() = default;

  char* Message;
  char *CurBuffer;
  size_t MessageSize = 0;
};

struct I32 : public BaseMessageTy {
  int32_t Value;

  explicit I32(int32_t Value);
  I32(std::string_view MessageBuffer);
};

struct I64 : public BaseMessageTy {
  int64_t Value;

  explicit I64(int64_t Value);
  I64(std::string_view MessageBuffer);
};

struct Pointer : public BaseMessageTy {
  void *Value;

  explicit Pointer(uintptr_t Value);
  Pointer(std::string_view MessageBuffer);
};

struct TargetBinaryDescription : public BaseMessageTy {
  explicit TargetBinaryDescription(__tgt_bin_desc *TBD);
  TargetBinaryDescription(std::string_view MessageBuffer, __tgt_bin_desc *TBD,
                          std::unordered_map<const void *, __tgt_device_image *>
                              &HostToRemoteDeviceImage);
};

struct Binary : public BaseMessageTy {
  int32_t DeviceId;
  void *Image;

  Binary(int32_t DeviceId, __tgt_device_image *Image);
  Binary(std::string_view MessageBuffer);
};

struct TargetTable : public BaseMessageTy {
  __tgt_target_table *Table;

  TargetTable(__tgt_target_table *Table);
  TargetTable(std::string_view MessageBuffer);
};

struct DataAlloc : public BaseMessageTy {
  int32_t DeviceId;
  int64_t AllocSize;
  void *HstPtr;

  DataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr);
  DataAlloc(std::string_view MessageBuffer);
};

struct DataDelete : public BaseMessageTy {
  int32_t DeviceId;
  void *TgtPtr;

  DataDelete(int32_t DeviceId, void *TgtPtr);
  DataDelete(std::string_view MessageBuffer);
};

struct DataSubmit : public BaseMessageTy {
  int32_t DeviceId;
  void *TgtPtr, *HstPtr;
  int64_t DataSize;

  DataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr, int64_t DataSize);
  DataSubmit(std::string_view MessageBuffer);
};

struct DataRetrieve : public BaseMessageTy {
  int32_t DeviceId;
  void *TgtPtr;
  int64_t DataSize;

  DataRetrieve(int32_t DeviceId, void *TgtPtr, int64_t DataSize);
  DataRetrieve(std::string_view MessageBuffer);
};

struct Data : public BaseMessageTy {
  int32_t Value;
  void *DataBuffer;
  size_t DataSize;

  Data(int32_t Value, char *DataBuffer, size_t DataSize);
  Data(std::string_view MessageBuffer);
};

struct TargetRegion : public BaseMessageTy {
  int32_t DeviceId;
  void *TgtEntryPtr;
  void **TgtArgs;
  ptrdiff_t *TgtOffsets;
  int32_t ArgNum;

  TargetRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
               ptrdiff_t *TgtOffsets, int32_t ArgNum);
  TargetRegion(std::string_view MessageBuffer);
};

struct TargetTeamRegion : public BaseMessageTy {
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
  TargetTeamRegion(std::string_view MessageBuffer);
};

} // namespace transport::ucx::messages
