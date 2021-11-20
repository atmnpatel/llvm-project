#include "Serializer.h"
#include "llvm/Support/ErrorHandling.h"

#include <utility>

std::string CustomSerializerTy::I32(int32_t Value) {
  transport::ucx::custom::I32 Message(Value);
  return Message.Message;
}

int32_t CustomSerializerTy::I32(std::string Message) {
  transport::ucx::custom::I32 Response(Message);
  return Response.Value;
}

std::string CustomSerializerTy::I64(int64_t Value) {
  transport::ucx::custom::I64 Message(Value);
  return Message.Message;
}

int64_t CustomSerializerTy::I64(std::string Message) {
  transport::ucx::custom::I64 Response(Message);
  return Response.Value;
}

std::string CustomSerializerTy::TargetBinaryDescription(__tgt_bin_desc *TBD) {
  transport::ucx::custom::TargetBinaryDescription Response(TBD);
  return Response.Message;
}

__tgt_bin_desc *CustomSerializerTy::TargetBinaryDescription(
    std::string Message,
    std::unordered_map<const void *, __tgt_device_image *> &DeviceImages) {
  auto *TBD = new __tgt_bin_desc();
  transport::ucx::custom::TargetBinaryDescription Response(Message, TBD,
                                                           DeviceImages);
  return TBD;
}

std::string CustomSerializerTy::Pointer(uintptr_t Pointer) {
  transport::ucx::custom::Pointer Message(Pointer);
  return Message.Message;
}

void *CustomSerializerTy::Pointer(std::string Message) {
  transport::ucx::custom::Pointer Response(Message);
  return Response.Value;
}

std::string CustomSerializerTy::Binary(int32_t DeviceId,
                                       __tgt_device_image *Image) {
  transport::ucx::custom::Binary Response(DeviceId, Image);
  return Response.Message;
}

std::pair<int32_t, __tgt_device_image *>
CustomSerializerTy::Binary(std::string Message) {
  transport::ucx::custom::Binary Response(std::move(Message));
  return {Response.DeviceId, (__tgt_device_image *)Response.Image};
}

std::string CustomSerializerTy::TargetTable(__tgt_target_table *TT) {
  transport::ucx::custom::TargetTable Table(TT);
  return Table.Message;
}

__tgt_target_table *CustomSerializerTy::TargetTable(
    std::string Message,
    std::unordered_map<void *, void *> &HostToRemoteTargetTableMap) {
  transport::ucx::custom::TargetTable Table(Message);
  return Table.Table;
}

std::string CustomSerializerTy::DataAlloc(int32_t DeviceId, int64_t Size,
                                          void *HstPtr) {
  transport::ucx::custom::DataAlloc Request(DeviceId, Size, HstPtr);
  return Request.Message;
}

std::tuple<int32_t, int64_t, void *>
CustomSerializerTy::DataAlloc(std::string Message) {
  transport::ucx::custom::DataAlloc Request(Message);
  return {Request.DeviceId, Request.AllocSize, (void *)Request.HstPtr};
}

std::string CustomSerializerTy::DataDelete(int32_t DeviceId, void *TgtPtr) {
  transport::ucx::custom::DataDelete Request(DeviceId, TgtPtr);
  return Request.Message;
}

std::tuple<int32_t, void *>
CustomSerializerTy::DataDelete(std::string Message) {
  transport::ucx::custom::DataDelete Request(Message);
  return {Request.DeviceId, (void *)Request.TgtPtr};
}

std::string CustomSerializerTy::DataSubmit(int32_t DeviceId, void *TgtPtr,
                                           void *HstPtr, int64_t Size) {
  transport::ucx::custom::DataSubmit Request(DeviceId, TgtPtr, HstPtr, Size);
  return Request.Message;
}

std::tuple<int32_t, void *, void *, int64_t>
CustomSerializerTy::DataSubmit(std::string Message) {
  transport::ucx::custom::DataSubmit Request(Message);

  return {Request.DeviceId, (void *)Request.TgtPtr, (void *)Request.HstPtr,
          (int64_t)Request.DataSize};
}

std::string CustomSerializerTy::DataRetrieve(int32_t DeviceId, void *TgtPtr,
                                             int64_t Size) {
  transport::ucx::custom::DataRetrieve Request(DeviceId, TgtPtr, Size);
  return Request.Message;
}

std::tuple<int32_t, void *, int64_t>
CustomSerializerTy::DataRetrieve(std::string Message) {
  transport::ucx::custom::DataRetrieve Request(Message);
  return {Request.DeviceId, (void *)Request.TgtPtr, Request.DataSize};
}

std::string CustomSerializerTy::Data(void *DataBuffer, size_t Size,
                                     int32_t Value) {
  transport::ucx::custom::Data Request(Value, (char *)DataBuffer, Size);
  return Request.Message;
}

std::tuple<void *, size_t, int32_t>
CustomSerializerTy::Data(std::string Message) {
  transport::ucx::custom::Data Request(Message);
  return {(void *)Request.DataBuffer, Request.DataSize, Request.Value};
}

std::string CustomSerializerTy::TargetRegion(
    int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs, ptrdiff_t *TgtOffsets,
    int32_t ArgNum, std::unordered_map<void *, void *> RemoteEntries) {
  transport::ucx::custom::TargetRegion Request(DeviceId, TgtEntryPtr, TgtArgs,
                                               TgtOffsets, ArgNum);
  return Request.Message;
}

std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t>
CustomSerializerTy::TargetRegion(std::string Message) {
  transport::ucx::custom::TargetRegion Request(Message);

  return {Request.DeviceId, (void *)Request.TgtEntryPtr,
          (void **)Request.TgtArgs, (ptrdiff_t *)Request.TgtOffsets,
          Request.ArgNum};
}

std::string CustomSerializerTy::TargetTeamRegion(
    int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs, ptrdiff_t *TgtOffsets,
    int32_t ArgNum, int32_t TeamNum, int32_t ThreadLimit,
    uint64_t LoopTripCount, std::unordered_map<void *, void *> RemoteEntries) {
  transport::ucx::custom::TargetTeamRegion Request(
      DeviceId, TgtEntryPtr, TgtArgs, TgtOffsets, ArgNum, TeamNum, ThreadLimit,
      LoopTripCount);
  return Request.Message;
}

std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t, int32_t, int32_t,
           uint64_t>
CustomSerializerTy::TargetTeamRegion(std::string Message) {
  transport::ucx::custom::TargetTeamRegion Request(Message);
  return {Request.DeviceId,    Request.TgtEntryPtr,  Request.TgtArgs,
          Request.TgtOffsets,  Request.ArgNum,       Request.TeamNum,
          Request.ThreadLimit, Request.LoopTripCount};
}

std::string ProtobufSerializerTy::I32(int32_t Value) {
  transport::messages::I32 Message;
  Message.set_number(Value);
  return Message.SerializeAsString();
}

int32_t ProtobufSerializerTy::I32(std::string Message) {
  transport::messages::I32 Response;
  Response.ParseFromString(Message);
  return Response.number();
}

std::string ProtobufSerializerTy::I64(int64_t Value) {
  transport::messages::I64 Message;
  Message.set_number(Value);
  return Message.SerializeAsString();
}

int64_t ProtobufSerializerTy::I64(std::string Message) {
  transport::messages::I64 Response;
  Response.ParseFromString(Message);
  return Response.number();
}

std::string ProtobufSerializerTy::TargetBinaryDescription(__tgt_bin_desc *TBD) {
  transport::messages::TargetBinaryDescription Response;
  loadTargetBinaryDescription(TBD, Response);
  return Response.SerializeAsString();
}

__tgt_bin_desc *ProtobufSerializerTy::TargetBinaryDescription(
    std::string Message,
    std::unordered_map<const void *, __tgt_device_image *> &DeviceImages) {
  transport::messages::TargetBinaryDescription Response;
  auto *TBD = new __tgt_bin_desc();
  Response.ParseFromString(Message);
  unloadTargetBinaryDescription(&Response, TBD, DeviceImages);
  return TBD;
}

std::string ProtobufSerializerTy::Pointer(uintptr_t Pointer) {
  transport::messages::Pointer Message;
  Message.set_number(Pointer);
  return Message.SerializeAsString();
}

void *ProtobufSerializerTy::Pointer(std::string Message) {
  transport::messages::Pointer Response;
  Response.ParseFromString(Message);
  return (void *)Response.number();
}

std::string ProtobufSerializerTy::Binary(int32_t DeviceId,
                                         __tgt_device_image *Image) {
  transport::messages::Binary Response;
  Response.set_device_id(DeviceId);
  Response.set_image_ptr((uintptr_t)Image);
  return Response.SerializeAsString();
}

std::pair<int32_t, __tgt_device_image *>
ProtobufSerializerTy::Binary(std::string Message) {
  transport::messages::Binary Response;
  if (!Response.ParseFromString(Message))
    llvm::report_fatal_error("Could not parse Protobuf Message");
  return {Response.device_id(), (__tgt_device_image *)Response.image_ptr()};
}

std::string ProtobufSerializerTy::TargetTable(__tgt_target_table *TT) {
  transport::messages::TargetTable Table;
  loadTargetTable(TT, Table);
  return Table.SerializeAsString();
}

__tgt_target_table *ProtobufSerializerTy::TargetTable(
    std::string Message,
    std::unordered_map<void *, void *> &HostToRemoteTargetTableMap) {
  transport::messages::TargetTable Table;
  auto *TT = new __tgt_target_table;
  Table.ParseFromString(Message);
  unloadTargetTable(Table, TT, HostToRemoteTargetTableMap);
  return TT;
}

std::string ProtobufSerializerTy::DataAlloc(int32_t DeviceId, int64_t Size,
                                            void *HstPtr) {
  transport::messages::AllocData Request;
  Request.set_device_id(DeviceId);
  Request.set_size(Size);
  Request.set_hst_ptr((uintptr_t)HstPtr);
  return Request.SerializeAsString();
}

std::tuple<int32_t, int64_t, void *>
ProtobufSerializerTy::DataAlloc(std::string Message) {
  transport::messages::AllocData Request;
  Request.ParseFromString(Message);
  return {Request.device_id(), Request.size(), (void *)Request.hst_ptr()};
}

std::string ProtobufSerializerTy::DataDelete(int32_t DeviceId, void *TgtPtr) {
  transport::messages::DeleteData Request;
  Request.set_device_id(DeviceId);
  Request.set_tgt_ptr((uintptr_t)TgtPtr);
  return Request.SerializeAsString();
}

std::tuple<int32_t, void *>
ProtobufSerializerTy::DataDelete(std::string Message) {
  transport::messages::DeleteData Request;
  Request.ParseFromString(Message);
  return {Request.device_id(), (void *)Request.tgt_ptr()};
}

std::string ProtobufSerializerTy::DataSubmit(int32_t DeviceId, void *TgtPtr,
                                             void *HstPtr, int64_t Size) {
  transport::messages::SubmitData Request;
  Request.set_device_id(DeviceId);
  Request.set_data((const char *)HstPtr, Size);
  Request.set_tgt_ptr((uint64_t)TgtPtr);

  return Request.SerializeAsString();
}

std::tuple<int32_t, void *, void *, int64_t>
ProtobufSerializerTy::DataSubmit(std::string Message) {
  transport::messages::SubmitData * Request = new transport::messages::SubmitData();
  Request->ParseFromString(Message);

  return {Request->device_id(),(void *)Request->tgt_ptr(),  (void *)Request->data().data(),
          (int64_t)Request->data().size()};
}

std::string ProtobufSerializerTy::DataRetrieve(int32_t DeviceId, void *TgtPtr,
                                               int64_t Size) {
  transport::messages::RetrieveData Request;
  Request.set_device_id(DeviceId);
  Request.set_tgt_ptr((uintptr_t)TgtPtr);
  Request.set_size(Size);
  return Request.SerializeAsString();
}

std::tuple<int32_t, void *, int64_t>
ProtobufSerializerTy::DataRetrieve(std::string Message) {
  transport::messages::RetrieveData Request;
  Request.ParseFromString(Message);
  return {Request.device_id(), (void *)Request.tgt_ptr(), Request.size()};
}

std::string ProtobufSerializerTy::Data(void *DataBuffer, size_t Size,
                                       int32_t Value) {
  transport::messages::Data Request;
  Request.set_data(DataBuffer, Size);
  Request.set_ret(Value);
  return Request.SerializeAsString();
}

std::tuple<void *, size_t, int32_t>
ProtobufSerializerTy::Data(std::string Message) {
  transport::messages::Data *Request = new transport::messages::Data();
  Request->ParseFromString(Message);
  return {(void *)Request->data().data(), Request->data().size(), Request->ret()};
}

std::string ProtobufSerializerTy::TargetRegion(
    int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs, ptrdiff_t *TgtOffsets,
    int32_t ArgNum, std::unordered_map<void *, void *> RemoteEntries) {
  transport::messages::TargetRegion Request;
  Request.set_device_id(DeviceId);

  Request.set_tgt_entry_ptr((uint64_t)RemoteEntries[TgtEntryPtr]);

  char **ArgPtr = (char **)TgtArgs;
  for (auto I = 0; I < ArgNum; I++, ArgPtr++)
    Request.add_tgt_args((uint64_t)*ArgPtr);

  char *OffsetPtr = (char *)TgtOffsets;
  for (auto I = 0; I < ArgNum; I++, OffsetPtr++)
    Request.add_tgt_offsets(*OffsetPtr);

  return Request.SerializeAsString();
}

std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t>
ProtobufSerializerTy::TargetRegion(std::string Message) {
  transport::messages::TargetRegion Request;
  Request.ParseFromString(Message);

  std::vector<uint64_t> TgtArgs(Request.tgt_args_size());
  for (auto I = 0; I < Request.tgt_args_size(); I++)
    TgtArgs[I] = (uint64_t)Request.tgt_args()[I];

  std::vector<ptrdiff_t> TgtOffsets(Request.tgt_offsets_size());
  const auto *TgtOffsetItr = Request.tgt_offsets().begin();
  for (auto I = 0; I < Request.tgt_offsets_size(); I++, TgtOffsetItr++)
    TgtOffsets[I] = (ptrdiff_t)*TgtOffsetItr;

  void *TgtEntryPtr = ((__tgt_offload_entry *)Request.tgt_entry_ptr())->addr;

  return {Request.device_id(), TgtEntryPtr, (void **)TgtArgs.data(),
          (ptrdiff_t *)TgtOffsets.data(), Request.tgt_args_size()};
}

std::string ProtobufSerializerTy::TargetTeamRegion(
    int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs, ptrdiff_t *TgtOffsets,
    int32_t ArgNum, int32_t TeamNum, int32_t ThreadLimit,
    uint64_t LoopTripCount, std::unordered_map<void *, void *> RemoteEntries) {
  transport::messages::TargetTeamRegion Request;
  Request.set_device_id(DeviceId);
  Request.set_team_num(TeamNum);
  Request.set_thread_limit(ThreadLimit);
  Request.set_loop_tripcount(LoopTripCount);

  Request.set_tgt_entry_ptr((uint64_t)RemoteEntries[TgtEntryPtr]);

  char **ArgPtr = (char **)TgtArgs;
  for (auto I = 0; I < ArgNum; I++, ArgPtr++)
    Request.add_tgt_args((uint64_t)*ArgPtr);

  char *OffsetPtr = (char *)TgtOffsets;
  for (auto I = 0; I < ArgNum; I++, OffsetPtr++)
    Request.add_tgt_offsets(*OffsetPtr);

  return Request.SerializeAsString();
}

std::tuple<int32_t, void *, void **, ptrdiff_t *, int32_t, int32_t, int32_t,
           uint64_t>
ProtobufSerializerTy::TargetTeamRegion(std::string Message) {
  transport::messages::TargetTeamRegion Request;
  Request.ParseFromString(Message);

  std::vector<uint64_t> TgtArgs(Request.tgt_args_size());
  for (auto I = 0; I < Request.tgt_args_size(); I++)
    TgtArgs[I] = (uint64_t)Request.tgt_args()[I];

  std::vector<ptrdiff_t> TgtOffsets(Request.tgt_offsets_size());
  const auto *TgtOffsetItr = Request.tgt_offsets().begin();
  for (auto I = 0; I < Request.tgt_offsets_size(); I++, TgtOffsetItr++)
    TgtOffsets[I] = (ptrdiff_t)*TgtOffsetItr;

  void *TgtEntryPtr = ((__tgt_offload_entry *)Request.tgt_entry_ptr())->addr;

  return {Request.device_id(),     TgtEntryPtr,
          (void **)TgtArgs.data(), (ptrdiff_t *)TgtOffsets.data(),
          Request.tgt_args_size(), Request.team_num(),
          Request.thread_limit(),  Request.loop_tripcount()};
}
