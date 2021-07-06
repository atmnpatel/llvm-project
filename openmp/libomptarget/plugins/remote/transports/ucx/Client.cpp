#include "Client.h"
#include "Utils.h"
#include "omptarget.h"
#include "ucx.pb.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstddef>
#include <cstdint>
#include <numeric>

namespace transport {
namespace ucx {

ProtobufClientTy::ProtobufClientTy(const ConnectionConfigTy &Config)
    : Interface(Context, &Allocator, Config), DebugLevel(getDebugLevel()) {}

int32_t ProtobufClientTy::getNumberOfDevices() {
  std::vector<std::string> Messages = {getHeader(GetNumberOfDevices)};

  CLIENT_DBG("Getting number of devices")
  Interface.send(Messages);

  auto Message = Interface.receiveMessage();

  I32 Reply;
  Reply.ParseFromString(Message);

  if (Reply.number())
    CLIENT_DBG("Found %d devices", Reply.number())
  else
    CLIENT_DBG("Could not get the number of devices")

  return Reply.number();
}

int32_t ProtobufClientTy::registerLib(__tgt_bin_desc *Desc) {
  TargetBinaryDescription desc;
  loadTargetBinaryDescription(Desc, desc);

  std::vector<std::string> Messages = {getHeader(RegisterLib),
                                       desc.SerializeAsString()};

  CLIENT_DBG("Registering library")
  Interface.send(Messages);

  auto Message = Interface.receiveMessage();

  I32 Reply;
  Reply.ParseFromString(Message);

  if (Reply.number() == 0)
    CLIENT_DBG("Registered library")

  return Reply.number();
}

int32_t ProtobufClientTy::unregisterLib(__tgt_bin_desc *Desc) {
  Pointer desc;
  desc.set_number((uintptr_t)Desc);

  std::vector<std::string> Messages = {getHeader(UnregisterLib),
                                       desc.SerializeAsString()};

  CLIENT_DBG("Unregistering library")
  Interface.send(Messages);

  auto Message = Interface.receiveMessage();

  I32 Reply;
  Reply.ParseFromString(Message);

  if (Reply.number() == 0)
    CLIENT_DBG("Unregistered library")
  else
    CLIENT_DBG("Failed to unregister library")

  return Reply.number();
}

int32_t ProtobufClientTy::isValidBinary(__tgt_device_image *Image) {
  Pointer desc;
  desc.set_number((uintptr_t)Image);

  std::vector<std::string> Messages = {getHeader(IsValidBinary),
                                       desc.SerializeAsString()};

  CLIENT_DBG("Validating binary")
  Interface.send(Messages);

  auto Message = Interface.receiveMessage();

  I32 Reply;
  Reply.ParseFromString(Message);

  if (Reply.number())
    CLIENT_DBG("Validated binary")
  else
    CLIENT_DBG("Could not validate binary")

  return Reply.number();
}

int32_t ProtobufClientTy::initDevice(int32_t DeviceId) {
  I32 DevId;
  DevId.set_number(DeviceId);

  std::vector<std::string> Messages = {getHeader(InitDevice),
                                       DevId.SerializeAsString()};

  CLIENT_DBG("Initializing device %d", DeviceId)
  Interface.send(Messages);

  auto Message = Interface.receiveMessage();

  I32 Reply;
  Reply.ParseFromString(Message);

  if (!Reply.number())
    CLIENT_DBG("Initialized device %d", DeviceId)
  else
    CLIENT_DBG("Could not initialize device %d", DeviceId)

  return Reply.number();
}

int64_t ProtobufClientTy::initRequires(int64_t RequiresFlags) {
  I64 DevId;
  DevId.set_number(RequiresFlags);

  std::vector<std::string> Messages = {getHeader(InitRequires),
                                       DevId.SerializeAsString()};

  CLIENT_DBG("Initializing requires")
  Interface.send(Messages);

  auto Message = Interface.receiveMessage();

  I64 Reply;
  Reply.ParseFromString(Message);

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

  std::vector<std::string> Messages = {getHeader(LoadBinary),
                                       Request.SerializeAsString()};

  CLIENT_DBG("Loading Image %p to device %d", Image, DeviceId)
  Interface.send(Messages);

  auto Message = Interface.receiveMessage();
  TargetTable Reply;
  Reply.ParseFromString(Message);

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

  std::vector<std::string> Messages = {getHeader(DataAlloc),
                                       Request.SerializeAsString()};

  CLIENT_DBG("Allocating %ld bytes on device %d", Size, DeviceId)
  Interface.send(Messages);

  auto Message = Interface.receiveMessage();

  Pointer Reply;
  Reply.ParseFromString(Message);

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

  std::vector<std::string> Messages = {getHeader(DataDelete),
                                       Request.SerializeAsString()};

  CLIENT_DBG("Deleting data at %p on device %d", TgtPtr, DeviceId)
  Interface.send(Messages);

  auto Message = Interface.receiveMessage();

  I32 Reply;
  Reply.ParseFromString(Message);

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

  std::vector<std::string> Messages = {getHeader(DataSubmit),
                                       Request.SerializeAsString()};

  CLIENT_DBG("Submitting %ld bytes async on device %d at %p", Size, DeviceId,
             TgtPtr)
  Interface.send(Messages);

  auto Message = Interface.receiveMessage();

  I32 Reply;
  Reply.ParseFromString(Message);

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

  std::vector<std::string> Messages = {getHeader(DataRetrieve),
                                       Request.SerializeAsString()};

  CLIENT_DBG(" retrieving %ld bytes on device %d at %p for %p", Size, DeviceId,
             TgtPtr, HstPtr)
  Interface.send(Messages);

  auto Message = Interface.receiveMessage();

  Data Reply;
  Reply.ParseFromString(Message);

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
    Request.add_tgt_offsets((uint64_t)*OffsetPtr);

  std::vector<std::string> Messages = {getHeader(RunTargetRegion),
                                       Request.SerializeAsString()};

  CLIENT_DBG("Running target region async on device %d", DeviceId)
  Interface.send(Messages);

  auto Message = Interface.receiveMessage();

  I32 Reply;
  Reply.ParseFromString(Message);

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
    Request.add_tgt_offsets((uint64_t)*OffsetPtr);

  std::vector<std::string> Messages = {getHeader(RunTargetTeamRegion),
                                       Request.SerializeAsString()};

  CLIENT_DBG("Running target team region async on device %d", DeviceId)
  Interface.send(Messages);

  auto Message = Interface.receiveMessage();

  I32 Reply;
  Reply.ParseFromString(Message);

  if (!Reply.number())
    CLIENT_DBG("Ran target team region async on device %d", DeviceId)
  else
    CLIENT_DBG("Could not run target team region async on device %d", DeviceId)

  return Reply.number();
}

SelfClientTy::SelfClientTy(const ConnectionConfigTy &Config)
    : Interface(Context, &Allocator, Config) {}

int32_t SelfClientTy::getNumberOfDevices() {
  DFN("")
  Interface.sendMessage(MessageTy::GetNumberOfDevices);

  if (!Interface.EP.Connected)
    return 0;

  Decoder D(&Allocator);
  Interface.receive(D);

  int32_t NumDevices;
  D.deserializeValue(NumDevices);

  return NumDevices;
}

int32_t SelfClientTy::registerLib(__tgt_bin_desc *Desc) {
  DFN("")
  Interface.sendMessage(MessageTy::RegisterLib, *Desc);

  Decoder D(&Allocator);
  Interface.receive(D);
  return 0;
}

int32_t SelfClientTy::unregisterLib(__tgt_bin_desc *Desc) {
  DFN("")
  Interface.sendMessage(MessageTy::UnregisterLib, (uintptr_t)Desc);

  Decoder D(&Allocator);
  Interface.receive(D);
  return 0;
}

int32_t SelfClientTy::isValidBinary(__tgt_device_image *Image) {
  DFN("")
  Interface.sendMessage(MessageTy::IsValidBinary, (uintptr_t)Image);

  Decoder D(&Allocator);
  Interface.receive(D);

  int32_t IsValid;
  D.deserializeValue(IsValid);

  return IsValid;
}

int32_t SelfClientTy::initDevice(int32_t DeviceId) {
  DFN("")
  Interface.sendMessage(MessageTy::InitDevice, DeviceId);

  Decoder D(&Allocator);
  Interface.receive(D);

  int32_t Response;
  D.deserializeValue(Response);

  return Response;
}

int64_t SelfClientTy::initRequires(int64_t RequiresFlags) {
  DFN("")
  Interface.sendMessage(MessageTy::InitRequires, RequiresFlags);

  Decoder D(&Allocator);

  Interface.receive(D);

  int64_t Response;
  D.deserializeValue(Response);

  return Response;
}

__tgt_target_table *SelfClientTy::loadBinary(int32_t DeviceId,
                                             __tgt_device_image *Image) {
  DFN("")
  Interface.sendMessage(MessageTy::LoadBinary, DeviceId, (uintptr_t)Image);

  Decoder D(&Allocator);
  Interface.receive(D);

  __tgt_target_table *TT =
      (__tgt_target_table *)malloc(sizeof(__tgt_target_table));

  D.deserializeList(TT->EntriesBegin, TT->EntriesEnd, false);

  return TT;
}

int32_t SelfClientTy::isDataExchangeable(int32_t SrcDevId, int32_t DstDevId) {
  DFN("")
  llvm_unreachable("Unimplemented");
  Interface.sendMessage(MessageTy::IsDataExchangeable, SrcDevId, DstDevId);
}

void *SelfClientTy::dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) {
  DFN("")
  Interface.sendMessage(MessageTy::DataAlloc, DeviceId, Size, (uintptr_t)HstPtr);

  Decoder D(&Allocator);
  Interface.receive(D);

  uintptr_t Response;
  D.deserializeValue(Response);

  return (void *)Response;
}

int32_t SelfClientTy::dataDelete(int32_t DeviceId, void *TgtPtr) {
  DFN("")
  Interface.sendMessage(MessageTy::DataDelete, DeviceId, (uintptr_t)TgtPtr);

  Decoder D(&Allocator);
  Interface.receive(D);

  int32_t Response;
  D.deserializeValue(Response);

  return Response;
}

int32_t SelfClientTy::dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                                 int64_t Size) {
  DFN("")
  Interface.sendMessage(MessageTy::DataSubmit,
              DataSubmitTy{DeviceId, HstPtr, TgtPtr, Size});

  Decoder D(&Allocator);
  Interface.receive(D);

  int32_t Response;
  D.deserializeValue(Response);

  return Response;
}
int32_t SelfClientTy::dataRetrieve(int32_t DeviceId, void *HstPtr, void *TgtPtr,
                                   int64_t Size) {
  DFN("")
  Interface.sendMessage(MessageTy::DataRetrieve, DeviceId, (uintptr_t)TgtPtr, Size);

  Decoder D(&Allocator);
  Interface.receive(D);

  int32_t Response;
  char *Start, *End;

  D.deserializeValue(Response);
  std::tie(Start, End) = D.deserializeMemory();
  copyMemoryFromBuffer(Start, HstPtr, End - Start, false);

  return Response;
}

int32_t SelfClientTy::dataExchange(int32_t SrcDevId, void *SrcPtr,
                                   int32_t DstDevId, void *DstPtr,
                                   int64_t Size) {
  DFN("")
  llvm_unreachable("");
}

int32_t SelfClientTy::runTargetRegion(int32_t DeviceId, void *TgtEntryPtr,
                                      void **TgtArgs, ptrdiff_t *TgtOffsets,
                                      int32_t ArgNum) {
  DFN("")

  EncoderTy E(&Allocator, MessageTy::RunTargetRegion, false);
  E.serializeValue(DeviceId);
  E.serializeValue((uintptr_t)TgtEntryPtr);
  E.serializeValue(ArgNum);
  for (auto I = 0; I < ArgNum; I++) {
    E.serializeValue((uintptr_t) * (TgtArgs + I));
  }

  for (auto I = 0; I < ArgNum; I++) {
    E.serializeValue((uintptr_t)(TgtOffsets[I]));
  }

  E.initializeIndirectMemorySlabs();
  E.finalize();

  Interface.send(E.MessageSlabs);

  Decoder D(&Allocator);
  Interface.receive(D);

  int32_t Response;
  D.deserializeValue(Response);

  return Response;
}

int32_t SelfClientTy::runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                                          void **TgtArgs, ptrdiff_t *TgtOffsets,
                                          int32_t ArgNum, int32_t TeamNum,
                                          int32_t ThreadLimit,
                                          uint64_t LoopTripCount) {
  DFN("")

  EncoderTy E(&Allocator, MessageTy::RunTargetTeamRegion, false);
  E.serializeValue(DeviceId);
  E.serializeValue((uintptr_t)TgtEntryPtr);
  E.serializeValue(ArgNum);
  for (auto I = 0; I < ArgNum; I++) {
    E.serializeValue((uintptr_t) * (TgtArgs + I));
  }

  for (auto I = 0; I < ArgNum; I++) {
    E.serializeValue((uintptr_t)(TgtOffsets[I]));
  }

  E.serializeValue(TeamNum);
  E.serializeValue(ThreadLimit);
  E.serializeValue(LoopTripCount);
  E.initializeIndirectMemorySlabs();
  E.finalize();

  Interface.send(E.MessageSlabs);

  Decoder D(&Allocator);
  Interface.receive(D);

  int32_t Response;
  D.deserializeValue(Response);

  return Response;
}

ClientManagerTy::ClientManagerTy(bool Protobuf) {
  ManagerConfigTy Config;
  for (auto &ConnectionConfig : Config.ConnectionConfigs) {
    if (Protobuf)
      Clients.push_back(std::make_unique<ProtobufClientTy>(ConnectionConfig));
    else
      Clients.push_back(std::make_unique<SelfClientTy>(ConnectionConfig));
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
      NumDevicesInClient.push_back(NumDevices);
    }
  }
  return std::accumulate(NumDevicesInClient.begin(), NumDevicesInClient.end(),
                         0);
}

std::pair<int32_t, int32_t> ClientManagerTy::mapDeviceId(int32_t DeviceId) {
  for (size_t ClientIdx = 0; ClientIdx < NumDevicesInClient.size();
       ClientIdx++) {
    if (DeviceId < NumDevicesInClient[ClientIdx])
      return {ClientIdx, DeviceId};
    DeviceId -= NumDevicesInClient[ClientIdx];
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
} // namespace ucx
} // namespace transport
