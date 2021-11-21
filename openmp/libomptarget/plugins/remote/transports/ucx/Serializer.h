#pragma once

#include "Serialization.h"

struct SerializerTy {
  static std::string EmptyMessage() { return "0"; }

  virtual ~SerializerTy() = default;

  virtual std::string I32(int32_t Value) = 0;
  virtual int32_t I32(std::string_view Message) = 0;

  virtual std::string I64(int64_t Value) = 0;
  virtual int64_t I64(std::string_view Message) = 0;

  virtual std::string TargetBinaryDescription(__tgt_bin_desc *TBD) = 0;
  virtual __tgt_bin_desc *TargetBinaryDescription(
      std::string_view Message,
      std::unordered_map<const void *, __tgt_device_image *> &DeviceImages) = 0;

  virtual std::string Pointer(uintptr_t Pointer) = 0;
  virtual void *Pointer(std::string_view Message) = 0;

  virtual std::string Binary(int32_t DeviceId, __tgt_device_image *Image) = 0;
  virtual std::pair<int32_t, __tgt_device_image *>
  Binary(std::string_view Message) = 0;

  virtual std::string TargetTable(__tgt_target_table *TT) = 0;
  virtual __tgt_target_table *TargetTable(
      std::string_view Message,
      std::unordered_map<void *, void *> &HostToRemoteTargetTableMap) = 0;

  virtual std::string DataAlloc(int32_t DeviceId, int64_t Size,
                                void *HstPtr) = 0;
  virtual std::tuple<int32_t, int64_t, void *>
  DataAlloc(std::string_view Message) = 0;

  virtual std::string DataDelete(int32_t DeviceId, void *TgtPtr) = 0;
  virtual std::tuple<int32_t, void *> DataDelete(std::string_view Message) = 0;

  virtual std::string DataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                                 int64_t Size) = 0;
  virtual std::tuple<int32_t, void *, void *, int64_t>
  DataSubmit(std::string_view Message) = 0;

  virtual std::string DataRetrieve(int32_t DeviceId, void *TgtPtr,
                                   int64_t Size) = 0;
  virtual std::tuple<int32_t, void *, int64_t>
  DataRetrieve(std::string_view Message) = 0;

  virtual std::string Data(void *DataBuffer, size_t DataSize,
                           int32_t Value) = 0;
  virtual std::tuple<void *, size_t, int32_t> Data(std::string_view Message) = 0;

  virtual std::string
  TargetRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
               ptrdiff_t *TgtOffsets, int32_t ArgNum,
               std::unordered_map<void *, void *> RemoteEntries) = 0;
  virtual std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t>
  TargetRegion(std::string_view Message) = 0;

  virtual std::string
  TargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
                   ptrdiff_t *TgtOffsets, int32_t ArgNum, int32_t TeamNum,
                   int32_t ThreadLimit, uint64_t LoopTripCount,
                   std::unordered_map<void *, void *> RemoteEntries) = 0;
  virtual std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t, int32_t,
                     int32_t, uint64_t>
  TargetTeamRegion(std::string_view Message) = 0;
};

class ProtobufSerializerTy : public SerializerTy {
  std::string I32(int32_t Value) override;
  int32_t I32(std::string_view Message) override;

  std::string I64(int64_t Value) override;
  int64_t I64(std::string_view Message) override;

  std::string TargetBinaryDescription(__tgt_bin_desc *TBD) override;
  __tgt_bin_desc *
  TargetBinaryDescription(std::string_view Message,
                          std::unordered_map<const void *, __tgt_device_image *>
                              &DeviceImages) override;

  std::string Pointer(uintptr_t Pointer) override;
  void *Pointer(std::string_view Message) override;

  std::string Binary(int32_t DeviceId, __tgt_device_image *Image) override;
  std::pair<int32_t, __tgt_device_image *> Binary(std::string_view Message) override;

  std::string TargetTable(__tgt_target_table *TT) override;
  __tgt_target_table *TargetTable(
      std::string_view Message,
      std::unordered_map<void *, void *> &HostToRemoteTargetTableMap) override;

  std::string DataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) override;
  std::tuple<int32_t, int64_t, void *> DataAlloc(std::string_view Message) override;

  std::string DataDelete(int32_t DeviceId, void *TgtPtr) override;
  std::tuple<int32_t, void *> DataDelete(std::string_view Message) override;

  std::string DataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                         int64_t Size) override;
  std::tuple<int32_t, void *, void *, int64_t>
  DataSubmit(std::string_view Message) override;

  std::string DataRetrieve(int32_t DeviceId, void *TgtPtr,
                           int64_t Size) override;
  std::tuple<int32_t, void *, int64_t>
  DataRetrieve(std::string_view Message) override;

  std::string Data(void *DataBuffer, size_t DataSize, int32_t Value) override;
  std::tuple<void *, size_t, int32_t> Data(std::string_view Message) override;

  std::string
  TargetRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
               ptrdiff_t *TgtOffsets, int32_t ArgNum,
               std::unordered_map<void *, void *> RemoteEntries) override;
  std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t>
  TargetRegion(std::string_view Message) override;

  std::string
  TargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
                   ptrdiff_t *TgtOffsets, int32_t ArgNum, int32_t TeamNum,
                   int32_t ThreadLimit, uint64_t LoopTripCount,
                   std::unordered_map<void *, void *> RemoteEntries) override;
  std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t, int32_t, int32_t,
             uint64_t>
  TargetTeamRegion(std::string_view Message) override;
};

class CustomSerializerTy : public SerializerTy {
  std::string I32(int32_t Value) override;
  int32_t I32(std::string_view Message) override;

  std::string I64(int64_t Value) override;
  int64_t I64(std::string_view Message) override;

  std::string TargetBinaryDescription(__tgt_bin_desc *TBD) override;
  __tgt_bin_desc *
  TargetBinaryDescription(std::string_view Message,
                          std::unordered_map<const void *, __tgt_device_image *>
                              &DeviceImages) override;

  std::string Pointer(uintptr_t Pointer) override;
  void *Pointer(std::string_view Message) override;

  std::string Binary(int32_t DeviceId, __tgt_device_image *Image) override;
  std::pair<int32_t, __tgt_device_image *> Binary(std::string_view Message) override;

  std::string TargetTable(__tgt_target_table *TT) override;
  __tgt_target_table *TargetTable(
      std::string_view Message,
      std::unordered_map<void *, void *> &HostToRemoteTargetTableMap) override;

  std::string DataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) override;
  std::tuple<int32_t, int64_t, void *> DataAlloc(std::string_view Message) override;

  std::string DataDelete(int32_t DeviceId, void *TgtPtr) override;
  std::tuple<int32_t, void *> DataDelete(std::string_view Message) override;

  std::string DataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                         int64_t Size) override;
  std::tuple<int32_t, void *, void *, int64_t>
  DataSubmit(std::string_view Message) override;

  std::string DataRetrieve(int32_t DeviceId, void *TgtPtr,
                           int64_t Size) override;
  std::tuple<int32_t, void *, int64_t>
  DataRetrieve(std::string_view Message) override;

  std::string Data(void *DataBuffer, size_t DataSize, int32_t Value) override;
  std::tuple<void *, size_t, int32_t> Data(std::string_view Message) override;

  std::string
  TargetRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
               ptrdiff_t *TgtOffsets, int32_t ArgNum,
               std::unordered_map<void *, void *> RemoteEntries) override;
  std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t>
  TargetRegion(std::string_view Message) override;

  std::string
  TargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
                   ptrdiff_t *TgtOffsets, int32_t ArgNum, int32_t TeamNum,
                   int32_t ThreadLimit, uint64_t LoopTripCount,
                   std::unordered_map<void *, void *> RemoteEntries) override;
  std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t, int32_t, int32_t,
             uint64_t>
  TargetTeamRegion(std::string_view Message) override;
};
