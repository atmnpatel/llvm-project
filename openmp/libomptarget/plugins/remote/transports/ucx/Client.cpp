#include "Client.h"
#include "Utils.h"
#include "omptarget.h"
#include "ucx.pb.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstddef>
#include <cstdint>
#include <utility>

namespace transport::ucx {

ClientManagerTy::ClientManagerTy(bool Protobuf) {
  ManagerConfigTy Config;
  for (auto &ConnectionConfig : Config.ConnectionConfigs) {
    if (Protobuf)
      Clients.emplace_back(new ProtobufClientTy(ConnectionConfig));
    else
      Clients.emplace_back(new CustomClientTy(ConnectionConfig));
  }
}

ClientTy::ClientTy(ConnectionConfigTy Config) : Config(std::move(Config)) {}

ProtobufClientTy::ProtobufClientTy(const ConnectionConfigTy &Config)
    : ClientTy(Config) {}

int32_t ProtobufClientTy::getNumberOfDevices() {
  CLIENT_DBG("Getting number of devices")
  auto InterfaceIdx = getInterfaceIdx();
  Interfaces[InterfaceIdx]->send(GetNumberOfDevices, std::string("0"));

  if (!Interfaces[InterfaceIdx]->EP.Connected) {
    CLIENT_DBG("Could not get the number of devices")
    return 0;
  }

  I32 Reply;
  Reply.ParseFromString(Interfaces[InterfaceIdx]->receive().second);

  CLIENT_DBG("Found %d devices", Reply.number())

  return Reply.number();
}

int32_t ProtobufClientTy::registerLib(__tgt_bin_desc *Description) {
  TargetBinaryDescription Request;
  loadTargetBinaryDescription(Description, Request);
  auto InterfaceIdx = getInterfaceIdx();

  CLIENT_DBG("Registering library")
  Interfaces[InterfaceIdx]->send(RegisterLib, Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interfaces[InterfaceIdx]->receive().second);

  if (Reply.number() == 0)
    CLIENT_DBG("Registered library")

  return Reply.number();
}

int32_t ProtobufClientTy::unregisterLib(__tgt_bin_desc *Description) {
  Pointer Request;
  Request.set_number((uintptr_t)Description);

  auto InterfaceIdx = getInterfaceIdx();
  CLIENT_DBG("Unregistering library")
  Interfaces[InterfaceIdx]->send(UnregisterLib, Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interfaces[InterfaceIdx]->receive().second);

  if (Reply.number() == 0)
    CLIENT_DBG("Unregistered library")
  else
    CLIENT_DBG("Failed to unregister library")

  return Reply.number();
}

int32_t ProtobufClientTy::isValidBinary(__tgt_device_image *Image) {
  Pointer Request;
  Request.set_number((uintptr_t)Image);

  CLIENT_DBG("Validating binary")
  auto InterfaceIdx = getInterfaceIdx();
  Interfaces[InterfaceIdx]->send(IsValidBinary, Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interfaces[InterfaceIdx]->receive().second);

  if (Reply.number())
    CLIENT_DBG("Validated binary")
  else
    CLIENT_DBG("Could not validate binary")

  return Reply.number();
}

int32_t ProtobufClientTy::initDevice(int32_t DeviceId) {
  I32 DevId;
  DevId.set_number(DeviceId);

  auto InterfaceIdx = getInterfaceIdx();
  CLIENT_DBG("Initializing device %d", DeviceId)
  Interfaces[InterfaceIdx]->send(InitDevice, DevId.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interfaces[InterfaceIdx]->receive().second);

  if (!Reply.number())
    CLIENT_DBG("Initialized device %d", DeviceId)
  else
    CLIENT_DBG("Could not initialize device %d", DeviceId)

  return Reply.number();
}

int64_t ProtobufClientTy::initRequires(int64_t RequiresFlags) {
  I64 Request;
  Request.set_number(RequiresFlags);

  auto InterfaceIdx = getInterfaceIdx();
  CLIENT_DBG("Initializing requires")
  Interfaces[InterfaceIdx]->send(InitRequires, Request.SerializeAsString());

  I64 Reply;
  Reply.ParseFromString(Interfaces[InterfaceIdx]->receive().second);

  if (Reply.number()) {
    CLIENT_DBG("Initialized requires")
  } else {
    CLIENT_DBG("Could not initialize requires")
  }

  return Reply.number();
}

__tgt_target_table *ProtobufClientTy::loadBinary(int32_t DeviceId,
                                                 __tgt_device_image *Image) {
  Binary Request;
  Request.set_device_id(DeviceId);
  Request.set_image_ptr((uintptr_t)Image);

  auto InterfaceIdx = getInterfaceIdx();
  CLIENT_DBG("Loading Image %p to device %d", Image, DeviceId)
  Interfaces[InterfaceIdx]->send(LoadBinary, Request.SerializeAsString());

  TargetTable Reply;
  Reply.ParseFromString(Interfaces[InterfaceIdx]->receive().second);

  if (Reply.entries_size() == 0) {
    CLIENT_DBG("Could not load image %p onto device %d", Image, DeviceId)
    return (__tgt_target_table *)nullptr;
  }
  DevicesToTables[DeviceId] = std::make_unique<__tgt_target_table>();
  unloadTargetTable(Reply, DevicesToTables[DeviceId].get(),
                    RemoteEntries[DeviceId]);

  CLIENT_DBG("Loaded Image %p to device %d with %d entries", Image, DeviceId,
             Reply.entries_size())

  return DevicesToTables[DeviceId].get();
}

int32_t ProtobufClientTy::isDataExchangeable(int32_t SrcDevId,
                                             int32_t DstDevId) {
  llvm_unreachable("Unimplemented");
}

void *ProtobufClientTy::dataAlloc(int32_t DeviceId, int64_t Size,
                                  void *HstPtr) {
  AllocData Request;
  Request.set_device_id(DeviceId);
  Request.set_hst_ptr((uintptr_t)HstPtr);
  Request.set_size(Size);

  auto InterfaceIdx = getInterfaceIdx();
  CLIENT_DBG("Allocating %ld bytes on device %d", Size, DeviceId)
  Interfaces[InterfaceIdx]->send(DataAlloc, Request.SerializeAsString());

  Pointer Reply;
  Reply.ParseFromString(Interfaces[InterfaceIdx]->receive().second);

  if (Reply.number())
    CLIENT_DBG("Allocated %ld bytes on device %d at %p", Size, DeviceId,
               (void *)Reply.number())
  else
    CLIENT_DBG("Could not allocate %ld bytes on device %d at %p", Size,
               DeviceId, (void *)Reply.number())

  return (void *)Reply.number();
}

int32_t ProtobufClientTy::dataDelete(int32_t DeviceId, void *TgtPtr) {
  DeleteData Request;
  Request.set_device_id(DeviceId);
  Request.set_tgt_ptr((uintptr_t)TgtPtr);
  auto InterfaceIdx = getInterfaceIdx();

  CLIENT_DBG("Deleting data at %p on device %d", TgtPtr, DeviceId)
  Interfaces[InterfaceIdx]->send(DataDelete, Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interfaces[InterfaceIdx]->receive().second);

  if (!Reply.number())
    CLIENT_DBG("Deleted data at %p on device %d", TgtPtr, DeviceId)
  else
    CLIENT_DBG("Could not delete data at %p on device %d", TgtPtr, DeviceId)

  return Reply.number();
}

int32_t ProtobufClientTy::dataSubmit(int32_t DeviceId, void *TgtPtr,
                                     void *HstPtr, int64_t Size) {
  SubmitData Request;
  Request.set_device_id(DeviceId);
  Request.set_data((char *)HstPtr, Size);
  Request.set_hst_ptr((uint64_t)HstPtr);
  Request.set_tgt_ptr((uint64_t)TgtPtr);
  auto InterfaceIdx = getInterfaceIdx();

  CLIENT_DBG("Submitting %ld bytes async on device %d at %p", Size, DeviceId,
             TgtPtr)
  Interfaces[InterfaceIdx]->send(DataSubmit, Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interfaces[InterfaceIdx]->receive().second);

  if (!Reply.number())
    CLIENT_DBG(" submitted %ld bytes on device %d at %p", Size, DeviceId,
               TgtPtr)
  else
    CLIENT_DBG("Could not async submit %ld bytes on device %d at %p", Size,
               DeviceId, TgtPtr)

  return Reply.number();
}

int32_t ProtobufClientTy::dataRetrieve(int32_t DeviceId, void *HstPtr,
                                       void *TgtPtr, int64_t Size) {
  RetrieveData Request;
  Request.set_device_id(DeviceId);
  Request.set_tgt_ptr((uint64_t)TgtPtr);
  Request.set_size(Size);
  auto InterfaceIdx = getInterfaceIdx();

  CLIENT_DBG(" retrieving %ld bytes on device %d at %p for %p", Size, DeviceId,
             TgtPtr, HstPtr)
  Interfaces[InterfaceIdx]->send(DataRetrieve, Request.SerializeAsString());

  Data Reply;
  Reply.ParseFromString(Interfaces[InterfaceIdx]->receive().second);

  memcpy(HstPtr, Reply.data().data(), Reply.data().size());

  if (!Reply.ret()) {
    CLIENT_DBG(" retrieved %ld bytes on Device %d", Size, DeviceId)
  } else {
    CLIENT_DBG("Could not async retrieve %ld bytes on Device %d", Size,
               DeviceId)
  }

  return Reply.ret();
}

int32_t ProtobufClientTy::dataExchange(int32_t SrcDevId, void *SrcPtr,
                                       int32_t DstDevId, void *DstPtr,
                                       int64_t Size) {
  llvm_unreachable("");
}

int32_t ProtobufClientTy::runTargetRegion(int32_t DeviceId, void *TgtEntryPtr,
                                          void **TgtArgs, ptrdiff_t *TgtOffsets,
                                          int32_t ArgNum) {
  TargetRegion Request;
  Request.set_device_id(DeviceId);

  Request.set_tgt_entry_ptr((uint64_t)RemoteEntries[DeviceId][TgtEntryPtr]);
  auto InterfaceIdx = getInterfaceIdx();

  char **ArgPtr = (char **)TgtArgs;
  for (auto I = 0; I < ArgNum; I++, ArgPtr++)
    Request.add_tgt_args((uint64_t)*ArgPtr);

  char *OffsetPtr = (char *)TgtOffsets;
  for (auto I = 0; I < ArgNum; I++, OffsetPtr++)
    Request.add_tgt_offsets(*OffsetPtr);

  CLIENT_DBG("Running target region async on device %d", DeviceId)
  Interfaces[InterfaceIdx]->send(RunTargetRegion, Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interfaces[InterfaceIdx]->receive().second);

  if (!Reply.number())
    CLIENT_DBG("Ran target region async on device %d", DeviceId)
  else
    CLIENT_DBG("Could not run target region async on device %d", DeviceId)

  return Reply.number();
}

int32_t ProtobufClientTy::runTargetTeamRegion(int32_t DeviceId,
                                              void *TgtEntryPtr, void **TgtArgs,
                                              ptrdiff_t *TgtOffsets,
                                              int32_t ArgNum, int32_t TeamNum,
                                              int32_t ThreadLimit,
                                              uint64_t LoopTripCount) {
  TargetTeamRegion Request;
  Request.set_device_id(DeviceId);
  Request.set_team_num(TeamNum);
  Request.set_thread_limit(ThreadLimit);
  Request.set_loop_tripcount(LoopTripCount);

  Request.set_tgt_entry_ptr((uint64_t)RemoteEntries[DeviceId][TgtEntryPtr]);
  auto InterfaceIdx = getInterfaceIdx();

  char **ArgPtr = (char **)TgtArgs;
  for (auto I = 0; I < ArgNum; I++, ArgPtr++)
    Request.add_tgt_args((uint64_t)*ArgPtr);

  char *OffsetPtr = (char *)TgtOffsets;
  for (auto I = 0; I < ArgNum; I++, OffsetPtr++)
    Request.add_tgt_offsets(*OffsetPtr);

  CLIENT_DBG("Running target team region async on device %d", DeviceId)
  Interfaces[InterfaceIdx]->send(RunTargetTeamRegion,
                                 Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interfaces[InterfaceIdx]->receive().second);

  if (!Reply.number())
    CLIENT_DBG("Ran target team region async on device %d", DeviceId)
  else
    CLIENT_DBG("Could not run target team region async on device %d", DeviceId)

  return Reply.number();
}

CustomClientTy::CustomClientTy(const ConnectionConfigTy &Config)
    : ClientTy(Config) {}

int32_t CustomClientTy::getNumberOfDevices() {
  custom::MessageTy Request(true);
  auto InterfaceIdx = getInterfaceIdx();
  Interfaces[InterfaceIdx]->send(MessageKind::GetNumberOfDevices,
                                 Request.getBuffer());

  if (!Interfaces[InterfaceIdx]->EP.Connected)
    return 0;

  custom::I32 NumDevices(Interfaces[InterfaceIdx]->receive().second);
  return NumDevices.Value;
}

int32_t CustomClientTy::registerLib(__tgt_bin_desc *Description) {
  auto InterfaceIdx = getInterfaceIdx();
  custom::TargetBinaryDescription Request(Description);
  Interfaces[InterfaceIdx]->send(MessageKind::RegisterLib, Request.getBuffer());

  auto Response = Interfaces[InterfaceIdx]->receive();

  return 0;
}

int32_t CustomClientTy::unregisterLib(__tgt_bin_desc *Description) {
  custom::Pointer Request((uintptr_t)Description);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::UnregisterLib,
                                 Request.getBuffer());

  auto Response = Interfaces[InterfaceIdx]->receive();

  return 0;
}

int32_t CustomClientTy::isValidBinary(__tgt_device_image *Image) {
  custom::Pointer Request((uintptr_t)Image);
  auto InterfaceIdx = getInterfaceIdx();
  Interfaces[InterfaceIdx]->send(MessageKind::IsValidBinary,
                                 Request.getBuffer());

  custom::I32 Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Value;
}

int32_t CustomClientTy::initDevice(int32_t DeviceId) {
  custom::I32 Request(DeviceId);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::InitDevice, Request.getBuffer());

  custom::I32 Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Value;
}

int64_t CustomClientTy::initRequires(int64_t RequiresFlags) {
  custom::I64 Request(RequiresFlags);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::InitRequires,
                                 Request.getBuffer());

  custom::I64 Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Value;
}

__tgt_target_table *CustomClientTy::loadBinary(int32_t DeviceId,
                                               __tgt_device_image *Image) {
  custom::Binary Request(DeviceId, Image);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::LoadBinary, Request.getBuffer());

  custom::TargetTable Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Table;
}

int32_t CustomClientTy::isDataExchangeable(int32_t SrcDevId, int32_t DstDevId) {
  llvm_unreachable("Unimplemented");
}

void *CustomClientTy::dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) {
  custom::DataAlloc Request(DeviceId, Size, HstPtr);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::DataAlloc, Request.getBuffer());

  custom::Pointer Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Value;
}

int32_t CustomClientTy::dataDelete(int32_t DeviceId, void *TgtPtr) {
  custom::DataDelete Request(DeviceId, TgtPtr);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::DataDelete, Request.getBuffer());

  custom::I32 Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Value;
}

int32_t CustomClientTy::dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                                   int64_t Size) {
  custom::DataSubmit Request(DeviceId, TgtPtr, HstPtr, Size);

  auto InterfaceIdx = getInterfaceIdx();
  Interfaces[InterfaceIdx]->send(MessageKind::DataSubmit, Request.getBuffer());

  custom::I32 Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Value;
}

int32_t CustomClientTy::dataRetrieve(int32_t DeviceId, void *HstPtr,
                                     void *TgtPtr, int64_t Size) {
  custom::DataRetrieve Request(DeviceId, HstPtr, TgtPtr, Size);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::DataRetrieve,
                                 Request.getBuffer());

  custom::Data Response(Interfaces[InterfaceIdx]->receive().second);

  std::memcpy(HstPtr, Response.DataBuffer, Response.DataSize);

  return Response.Value;
}

int32_t CustomClientTy::dataExchange(int32_t SrcDevId, void *SrcPtr,
                                     int32_t DstDevId, void *DstPtr,
                                     int64_t Size) {
  llvm_unreachable("");
}

int32_t CustomClientTy::runTargetRegion(int32_t DeviceId, void *TgtEntryPtr,
                                        void **TgtArgs, ptrdiff_t *TgtOffsets,
                                        int32_t ArgNum) {
  custom::TargetRegion Request(DeviceId, TgtEntryPtr, TgtArgs, TgtOffsets,
                               ArgNum);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(RunTargetRegion, Request.getBuffer());

  custom::I32 Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Value;
}

int32_t CustomClientTy::runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                                            void **TgtArgs,
                                            ptrdiff_t *TgtOffsets,
                                            int32_t ArgNum, int32_t TeamNum,
                                            int32_t ThreadLimit,
                                            uint64_t LoopTripCount) {
  custom::TargetTeamRegion Request(DeviceId, TgtEntryPtr, TgtArgs, TgtOffsets,
                                   ArgNum, TeamNum, ThreadLimit, LoopTripCount);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(RunTargetTeamRegion, Request.getBuffer());

  custom::I32 Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Value;
}
} // namespace transport::ucx
