#pragma once

#include "Serialization.h"

using namespace transport::ucx;

struct SerializerTy {
  std::string emptyMessage();

  std::string I32(int32_t Value);
  int32_t I32(std::string_view Message);

  std::string I64(int64_t Value);
  int64_t I64(std::string_view Message);

  std::string TargetBinaryDescription(__tgt_bin_desc *TBD);
  __tgt_bin_desc *
  TargetBinaryDescription(std::string_view Message,
                          std::unordered_map<const void *, __tgt_device_image *>
                              &DeviceImages);

  std::string Pointer(uintptr_t Pointer);
  void *Pointer(std::string_view Message);

  std::string Binary(int32_t DeviceId, __tgt_device_image *Image);
  std::pair<int32_t, __tgt_device_image *> Binary(std::string_view Message);

  std::string TargetTable(__tgt_target_table *TT);
  __tgt_target_table *TargetTable(
      std::string_view Message,
      std::unordered_map<void *, void *> &HostToRemoteTargetTableMap);

  std::string DataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr);
  std::tuple<int32_t, int64_t, void *> DataAlloc(std::string_view Message);

  std::string DataDelete(int32_t DeviceId, void *TgtPtr);
  std::tuple<int32_t, void *> DataDelete(std::string_view Message);

  std::string DataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                         int64_t Size);
  std::tuple<int32_t, void *, void *, int64_t>
  DataSubmit(std::string_view Message);

  std::string DataRetrieve(int32_t DeviceId, void *TgtPtr,
                           int64_t Size);
  std::tuple<int32_t, void *, int64_t>
  DataRetrieve(std::string_view Message);

  std::string Data(void *DataBuffer, size_t DataSize, int32_t Value);
  std::tuple<void *, size_t, int32_t> Data(std::string_view Message);

  std::string
  TargetRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
               ptrdiff_t *TgtOffsets, int32_t ArgNum,
               std::unordered_map<void *, void *> RemoteEntries);
  std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t>
  TargetRegion(std::string_view Message);

  std::string
  TargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
                   ptrdiff_t *TgtOffsets, int32_t ArgNum, int32_t TeamNum,
                   int32_t ThreadLimit, uint64_t LoopTripCount,
                   std::unordered_map<void *, void *> RemoteEntries);
  std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t, int32_t, int32_t,
             uint64_t>
  TargetTeamRegion(std::string_view Message);
};
