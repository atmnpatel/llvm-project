#include "Serializer.h"

#include <utility>

std::string SerializerTy::emptyMessage() { return "0"; }

std::string SerializerTy::I32(int32_t Value) {
  messages::I32 Request(Value);
  return {Request.Message, Request.MessageSize};
}

int32_t SerializerTy::I32(std::string_view Message) {
  messages::I32 Response(Message);
  return Response.Value;
}

std::string SerializerTy::I64(int64_t Value) {
  messages::I64 Request(Value);
  return {Request.Message, Request.MessageSize};
}

int64_t SerializerTy::I64(std::string_view Message) {
  messages::I64 Response(Message);
  return Response.Value;
}

std::string SerializerTy::TargetBinaryDescription(__tgt_bin_desc *TBD) {
  messages::TargetBinaryDescription Request(TBD);
  return {Request.Message, Request.MessageSize};
}

__tgt_bin_desc *SerializerTy::TargetBinaryDescription(
    std::string_view Message,
    std::unordered_map<const void *, __tgt_device_image *> &DeviceImages) {
  auto *TBD = new __tgt_bin_desc();
  messages::TargetBinaryDescription Response(Message, TBD,
                                                           DeviceImages);
  return TBD;
}

std::string SerializerTy::Pointer(uintptr_t Pointer) {
  messages::Pointer Request(Pointer);
  return {Request.Message, Request.MessageSize};
}

void *SerializerTy::Pointer(std::string_view Message) {
  messages::Pointer Response(Message);
  return Response.Value;
}

std::string SerializerTy::Binary(int32_t DeviceId, __tgt_device_image *Image) {
  messages::Binary Request(DeviceId, Image);
  return {Request.Message, Request.MessageSize};
}

std::pair<int32_t, __tgt_device_image *>
SerializerTy::Binary(std::string_view Message) {
  messages::Binary Response(std::move(Message));
  return {Response.DeviceId, (__tgt_device_image *)Response.Image};
}

std::string SerializerTy::TargetTable(__tgt_target_table *TT) {
  messages::TargetTable Request(TT);
  return {Request.Message, Request.MessageSize};
}

__tgt_target_table *SerializerTy::TargetTable(
    std::string_view Message,
    std::unordered_map<void *, void *> &HostToRemoteTargetTableMap) {
  messages::TargetTable Table(Message);
  return Table.Table;
}

std::string SerializerTy::DataAlloc(int32_t DeviceId, int64_t Size,
                                    void *HstPtr) {
  messages::DataAlloc Request(DeviceId, Size, HstPtr);
  return {Request.Message, Request.MessageSize};
}

std::tuple<int32_t, int64_t, void *>
SerializerTy::DataAlloc(std::string_view Message) {
  messages::DataAlloc Request(Message);
  return {Request.DeviceId, Request.AllocSize, (void *)Request.HstPtr};
}

std::string SerializerTy::DataDelete(int32_t DeviceId, void *TgtPtr) {
  messages::DataDelete Request(DeviceId, TgtPtr);
  return {Request.Message, Request.MessageSize};
}

std::tuple<int32_t, void *> SerializerTy::DataDelete(std::string_view Message) {
  messages::DataDelete Request(Message);
  return {Request.DeviceId, (void *)Request.TgtPtr};
}

std::string SerializerTy::DataSubmit(int32_t DeviceId, void *TgtPtr,
                                     void *HstPtr, int64_t Size) {
  messages::DataSubmit Request(DeviceId, TgtPtr, HstPtr, Size);
  return {Request.Message, Request.MessageSize};
}

std::tuple<int32_t, void *, void *, int64_t>
SerializerTy::DataSubmit(std::string_view Message) {
  messages::DataSubmit Request(Message);

  return {Request.DeviceId, (void *)Request.TgtPtr, (void *)Request.HstPtr,
          (int64_t)Request.DataSize};
}

std::string SerializerTy::DataRetrieve(int32_t DeviceId, void *TgtPtr,
                                       int64_t Size) {
  messages::DataRetrieve Request(DeviceId, TgtPtr, Size);
  return {Request.Message, Request.MessageSize};
}

std::tuple<int32_t, void *, int64_t>
SerializerTy::DataRetrieve(std::string_view Message) {
  messages::DataRetrieve Request(Message);
  return {Request.DeviceId, (void *)Request.TgtPtr, Request.DataSize};
}

std::string SerializerTy::Data(void *DataBuffer, size_t Size, int32_t Value) {
  messages::Data Request(Value, (char *)DataBuffer, Size);
  return {Request.Message, Request.MessageSize};
}

std::tuple<void *, size_t, int32_t>
SerializerTy::Data(std::string_view Message) {
  messages::Data Request(Message);
  return {(void *)Request.DataBuffer, Request.DataSize, Request.Value};
}

std::string
SerializerTy::TargetRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
                           ptrdiff_t *TgtOffsets, int32_t ArgNum,
                           std::unordered_map<void *, void *> RemoteEntries) {
  messages::TargetRegion Request(DeviceId, TgtEntryPtr, TgtArgs,
                                               TgtOffsets, ArgNum);
  return {Request.Message, Request.MessageSize};
}

std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t>
SerializerTy::TargetRegion(std::string_view Message) {
  messages::TargetRegion Request(Message);

  return {Request.DeviceId, (void *)Request.TgtEntryPtr,
          (void **)Request.TgtArgs, (ptrdiff_t *)Request.TgtOffsets,
          Request.ArgNum};
}

std::string SerializerTy::TargetTeamRegion(
    int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs, ptrdiff_t *TgtOffsets,
    int32_t ArgNum, int32_t TeamNum, int32_t ThreadLimit,
    uint64_t LoopTripCount, std::unordered_map<void *, void *> RemoteEntries) {
  messages::TargetTeamRegion Request(
      DeviceId, TgtEntryPtr, TgtArgs, TgtOffsets, ArgNum, TeamNum, ThreadLimit,
      LoopTripCount);
  return {Request.Message, Request.MessageSize};
}

std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t, int32_t, int32_t,
           uint64_t>
SerializerTy::TargetTeamRegion(std::string_view Message) {
  messages::TargetTeamRegion Request(Message);
  return {Request.DeviceId,    Request.TgtEntryPtr,  Request.TgtArgs,
          Request.TgtOffsets,  Request.ArgNum,       Request.TeamNum,
          Request.ThreadLimit, Request.LoopTripCount};
}
