#include "Client.h"
#include "Utils.h"
#include "omptarget.h"
#include "ucx.pb.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstddef>
#include <cstdint>
#include <numeric>

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

int32_t ClientManagerTy::registerLib(__tgt_bin_desc *Desc) {
  int32_t Ret = 0;
  for (auto &Client : Clients)
    Ret &= Client->registerLib(Desc);
  return Ret;
}

int32_t ClientManagerTy::unregisterLib(__tgt_bin_desc *Desc) {
  int32_t Ret = 0;
  for (auto &Client : Clients)
    Ret &= Client->unregisterLib(Desc);
  return Ret;
}

int32_t ClientManagerTy::isValidBinary(__tgt_device_image *Image) {
  for (auto &Client : Clients) {
    if (auto Ret = Client->isValidBinary(Image))
      return Ret;
  }
  return 0;
}

int32_t ClientManagerTy::getNumberOfDevices() {
  for (auto &Client : Clients) {
    if (auto NumDevices = Client->getNumberOfDevices()) {
      Devices.push_back(NumDevices);
    }
  }
  return std::accumulate(Devices.begin(), Devices.end(), 0);
}

std::pair<int32_t, int32_t> ClientManagerTy::mapDeviceId(int32_t DeviceId) {
  for (size_t ClientIdx = 0; ClientIdx < Devices.size(); ClientIdx++) {
    if (DeviceId < Devices[ClientIdx])
      return {ClientIdx, DeviceId};
    DeviceId -= Devices[ClientIdx];
  }
  llvm::llvm_unreachable_internal("Invalid Device Id");
}

int32_t ClientManagerTy::initDevice(int32_t DeviceId) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->initDevice(DeviceIdx);
}

int64_t ClientManagerTy::initRequires(int64_t RequiresFlags) {
  for (auto &Client : Clients)
    Client->initRequires(RequiresFlags);
  return RequiresFlags;
}

__tgt_target_table *ClientManagerTy::loadBinary(int32_t DeviceId,
                                                __tgt_device_image *Image) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->loadBinary(DeviceIdx, Image);
}

int32_t ClientManagerTy::isDataExchangeable(int32_t SrcDevId,
                                            int32_t DstDevId) {
  int32_t SrcClientIdx, SrcDeviceIdx, DstClientIdx, DstDeviceIdx;
  std::tie(SrcClientIdx, SrcDeviceIdx) = mapDeviceId(SrcDevId);
  std::tie(DstClientIdx, DstDeviceIdx) = mapDeviceId(DstDevId);
  return Clients[SrcClientIdx]->isDataExchangeable(SrcDeviceIdx, DstDeviceIdx);
}

void *ClientManagerTy::dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->dataAlloc(DeviceIdx, Size, HstPtr);
}

int32_t ClientManagerTy::dataDelete(int32_t DeviceId, void *TgtPtr) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->dataDelete(DeviceIdx, TgtPtr);
}

int32_t ClientManagerTy::dataSubmit(int32_t DeviceId, void *TgtPtr,
                                    void *HstPtr, int64_t Size) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->dataSubmit(DeviceIdx, TgtPtr, HstPtr, Size);
}

int32_t ClientManagerTy::dataRetrieve(int32_t DeviceId, void *HstPtr,
                                      void *TgtPtr, int64_t Size) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->dataRetrieve(DeviceIdx, HstPtr, TgtPtr, Size);
}

int32_t ClientManagerTy::dataExchange(int32_t SrcDevId, void *SrcPtr,
                                      int32_t DstDevId, void *DstPtr,
                                      int64_t Size) {
  int32_t SrcClientIdx, SrcDeviceIdx, DstClientIdx, DstDeviceIdx;
  std::tie(SrcClientIdx, SrcDeviceIdx) = mapDeviceId(SrcDevId);
  std::tie(DstClientIdx, DstDeviceIdx) = mapDeviceId(DstDevId);
  return Clients[SrcClientIdx]->dataExchange(SrcDeviceIdx, SrcPtr, DstDeviceIdx,
                                             DstPtr, Size);
}

int32_t ClientManagerTy::runTargetRegion(int32_t DeviceId, void *TgtEntryPtr,
                                         void **TgtArgs, ptrdiff_t *TgtOffsets,
                                         int32_t ArgNum) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->runTargetRegion(DeviceIdx, TgtEntryPtr, TgtArgs,
                                             TgtOffsets, ArgNum);
}

int32_t ClientManagerTy::runTargetTeamRegion(int32_t DeviceId,
                                             void *TgtEntryPtr, void **TgtArgs,
                                             ptrdiff_t *TgtOffsets,
                                             int32_t ArgNum, int32_t TeamNum,
                                             int32_t ThreadLimit,
                                             uint64_t LoopTripCount) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->runTargetTeamRegion(
      DeviceIdx, TgtEntryPtr, TgtArgs, TgtOffsets, ArgNum, TeamNum, ThreadLimit,
      LoopTripCount);
}

ProtobufClientTy::ProtobufClientTy(const ConnectionConfigTy &Config)
    : Interface(Context, Config) {}

int32_t ProtobufClientTy::getNumberOfDevices() {
  CLIENT_DBG("Getting number of devices")
  Interface.send(GetNumberOfDevices, std::string("0"));

  if (!Interface.EP.Connected) {
    CLIENT_DBG("Could not get the number of devices")
    return 0;
  }

  I32 Reply;
  Reply.ParseFromString(Interface.receive().second);

  CLIENT_DBG("Found %d devices", Reply.number())

  return Reply.number();
}

int32_t ProtobufClientTy::registerLib(__tgt_bin_desc *Description) {
  TargetBinaryDescription Request;
  loadTargetBinaryDescription(Description, Request);

  CLIENT_DBG("Registering library")
  Interface.send(RegisterLib, Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interface.receive().second);

  if (Reply.number() == 0)
    CLIENT_DBG("Registered library")

  return Reply.number();
}

int32_t ProtobufClientTy::unregisterLib(__tgt_bin_desc *Description) {
  Pointer Request;
  Request.set_number((uintptr_t)Description);

  CLIENT_DBG("Unregistering library")
  Interface.send(UnregisterLib, Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interface.receive().second);

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
  Interface.send(IsValidBinary, Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interface.receive().second);

  if (Reply.number())
    CLIENT_DBG("Validated binary")
  else
    CLIENT_DBG("Could not validate binary")

  return Reply.number();
}

int32_t ProtobufClientTy::initDevice(int32_t DeviceId) {
  I32 DevId;
  DevId.set_number(DeviceId);

  CLIENT_DBG("Initializing device %d", DeviceId)
  Interface.send(InitDevice, DevId.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interface.receive().second);

  if (!Reply.number())
    CLIENT_DBG("Initialized device %d", DeviceId)
  else
    CLIENT_DBG("Could not initialize device %d", DeviceId)

  return Reply.number();
}

int64_t ProtobufClientTy::initRequires(int64_t RequiresFlags) {
  I64 Request;
  Request.set_number(RequiresFlags);

  CLIENT_DBG("Initializing requires")
  Interface.send(InitRequires, Request.SerializeAsString());

  I64 Reply;
  Reply.ParseFromString(Interface.receive().second);

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

  CLIENT_DBG("Loading Image %p to device %d", Image, DeviceId)
  Interface.send(LoadBinary, Request.SerializeAsString());

  TargetTable Reply;
  Reply.ParseFromString(Interface.receive().second);

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

  CLIENT_DBG("Allocating %ld bytes on device %d", Size, DeviceId)
  Interface.send(DataAlloc, Request.SerializeAsString());

  Pointer Reply;
  Reply.ParseFromString(Interface.receive().second);

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

  CLIENT_DBG("Deleting data at %p on device %d", TgtPtr, DeviceId)
  Interface.send(DataDelete, Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interface.receive().second);

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

  CLIENT_DBG("Submitting %ld bytes async on device %d at %p", Size, DeviceId,
             TgtPtr)
  Interface.send(DataSubmit, Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interface.receive().second);

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
  Request.set_size((uint64_t)Size);

  CLIENT_DBG(" retrieving %ld bytes on device %d at %p for %p", Size, DeviceId,
             TgtPtr, HstPtr)
  Interface.send(DataRetrieve, Request.SerializeAsString());

  Data Reply;
  Reply.ParseFromString(Interface.receive().second);

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

  char **ArgPtr = (char **)TgtArgs;
  for (auto I = 0; I < ArgNum; I++, ArgPtr++)
    Request.add_tgt_args((uint64_t)*ArgPtr);

  char *OffsetPtr = (char *)TgtOffsets;
  for (auto I = 0; I < ArgNum; I++, OffsetPtr++)
    Request.add_tgt_offsets(*OffsetPtr);

  CLIENT_DBG("Running target region async on device %d", DeviceId)
  Interface.send(RunTargetRegion, Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interface.receive().second);

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

  char **ArgPtr = (char **)TgtArgs;
  for (auto I = 0; I < ArgNum; I++, ArgPtr++)
    Request.add_tgt_args((uint64_t)*ArgPtr);

  char *OffsetPtr = (char *)TgtOffsets;
  for (auto I = 0; I < ArgNum; I++, OffsetPtr++)
    Request.add_tgt_offsets(*OffsetPtr);

  CLIENT_DBG("Running target team region async on device %d", DeviceId)
  Interface.send(RunTargetTeamRegion, Request.SerializeAsString());

  I32 Reply;
  Reply.ParseFromString(Interface.receive().second);

  if (!Reply.number())
    CLIENT_DBG("Ran target team region async on device %d", DeviceId)
  else
    CLIENT_DBG("Could not run target team region async on device %d", DeviceId)

  return Reply.number();
}

CustomClientTy::CustomClientTy(const ConnectionConfigTy &Config)
    : Interface(Context, Config) {}

int32_t CustomClientTy::getNumberOfDevices() {
  custom::Message Request(true);
  Interface.send(MessageKind::GetNumberOfDevices, Request.getBuffer());

  if (!Interface.EP.Connected)
    return 0;

  custom::I32 NumDevices(Interface.receive().second);
  return NumDevices.Value;
}

int32_t CustomClientTy::registerLib(__tgt_bin_desc *Description) {
  custom::TargetBinaryDescription Request(Description);
  Interface.send(MessageKind::RegisterLib, Request.getBuffer());

  auto Response = Interface.receive();

  return 0;
}

int32_t CustomClientTy::unregisterLib(__tgt_bin_desc *Description) {
  custom::Pointer Request((uintptr_t)Description);

  Interface.synchronize();
  Interface.send(MessageKind::UnregisterLib, Request.getBuffer());

  auto Response = Interface.receive();

  return 0;
}

int32_t CustomClientTy::isValidBinary(__tgt_device_image *Image) {
  custom::Pointer Request((uintptr_t)Image);
  Interface.send(MessageKind::IsValidBinary, Request.getBuffer());

  custom::I32 Response(Interface.receive().second);

  return Response.Value;
}

int32_t CustomClientTy::initDevice(int32_t DeviceId) {
  custom::I32 Request(DeviceId);

  Interface.send(MessageKind::InitDevice, Request.getBuffer());

  custom::I32 Response(Interface.receive().second);

  return Response.Value;
}

int64_t CustomClientTy::initRequires(int64_t RequiresFlags) {
  custom::I64 Request(RequiresFlags);

  Interface.send(MessageKind::InitRequires, Request.getBuffer());

  custom::I64 Response(Interface.receive().second);

  return Response.Value;
}

__tgt_target_table *CustomClientTy::loadBinary(int32_t DeviceId,
                                               __tgt_device_image *Image) {
  custom::Binary Request(DeviceId, Image);

  Interface.send(MessageKind::LoadBinary, Request.getBuffer());

  custom::TargetTable Response(Interface.receive().second);

  return Response.Table;
}

int32_t CustomClientTy::isDataExchangeable(int32_t SrcDevId, int32_t DstDevId) {
  llvm_unreachable("Unimplemented");
}

void *CustomClientTy::dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) {
  custom::DataAlloc Request(DeviceId, Size, HstPtr);

  Interface.send(MessageKind::DataAlloc, Request.getBuffer());

  custom::Pointer Response(Interface.receive().second);

  return Response.Value;
}

int32_t CustomClientTy::dataDelete(int32_t DeviceId, void *TgtPtr) {
  custom::DataDelete Request(DeviceId, TgtPtr);

  Interface.send(MessageKind::DataDelete, Request.getBuffer());

  custom::I32 Response(Interface.receive().second);

  return Response.Value;
}

int32_t CustomClientTy::dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                                   int64_t Size) {
  custom::DataSubmit Request(DeviceId, TgtPtr, HstPtr, Size);

  Interface.send(MessageKind::DataSubmit, Request.getBuffer());

  custom::I32 Response(Interface.receive().second);

  return Response.Value;
}

int32_t CustomClientTy::dataRetrieve(int32_t DeviceId, void *HstPtr,
                                     void *TgtPtr, int64_t Size) {
  custom::DataRetrieve Request(DeviceId, HstPtr, TgtPtr, Size);

  Interface.send(MessageKind::DataRetrieve, Request.getBuffer());

  custom::Data Response(Interface.receive().second);

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

  Interface.synchronize();
  Interface.send(RunTargetRegion, Request.getBuffer());

  custom::I32 Response(Interface.receive().second);

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

  Interface.synchronize();
  Interface.send(RunTargetTeamRegion, Request.getBuffer());

  custom::I32 Response(Interface.receive().second);

  return Response.Value;
}
} // namespace transport::ucx
