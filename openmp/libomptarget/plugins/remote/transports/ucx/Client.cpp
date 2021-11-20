#include "Client.h"
#include "Serializer.h"
#include "Utils.h"
#include "messages.pb.h"
#include "omptarget.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstddef>
#include <cstdint>
#include <utility>

namespace transport::ucx {

ClientManagerTy::ClientManagerTy(SerializerType Type) {
  ManagerConfigTy Config;
  for (auto &ConnectionConfig : Config.ConnectionConfigs)
    Clients.emplace_back((BaseClientTy *)new ClientTy(ConnectionConfig, Type));
}

ClientTy::ClientTy(ConnectionConfigTy Config, SerializerType Type) : Config(Config) {
  switch (Type) {
  case SerializerType::Custom:
    Serializer = (SerializerTy *)new CustomSerializerTy();
    break;
  case SerializerType::Protobuf:
    Serializer = (SerializerTy *)new ProtobufSerializerTy();
    break;
  }

  auto InterfaceIdx = getInterfaceIdx();
}

int32_t ClientTy::getNumberOfDevices() {
  CLIENT_DBG("Attempting to get number of devices")
  auto Response =
      send(MessageKind::GetNumberOfDevices, Serializer->EmptyMessage());

  auto NumDevices = Serializer->I32(Response);
  CLIENT_DBG("Found %d devices!", NumDevices)
  return NumDevices;
}

int32_t ClientTy::registerLib(__tgt_bin_desc *Description) {
  CLIENT_DBG("Registering library %p", Description)
  auto Response = send(MessageKind::RegisterLib,
                       Serializer->TargetBinaryDescription(Description));
  CLIENT_DBG("Registered library %p", Description)
  return Serializer->I32(Response);
}

int32_t ClientTy::unregisterLib(__tgt_bin_desc *Description) {
  auto Response = send(MessageKind::UnregisterLib, Serializer->EmptyMessage());
  return 0;
}

int32_t ClientTy::isValidBinary(__tgt_device_image *Image) {
  CLIENT_DBG("Checking validity of binary %p", Image)
  auto Response =
      send(MessageKind::IsValidBinary, Serializer->Pointer((uintptr_t)Image));
  auto IsValid = Serializer->I32(Response);
  if (IsValid)
    CLIENT_DBG("Binary %p is valid", Image)
  return IsValid;
}

int32_t ClientTy::initDevice(int32_t DeviceId) {
  CLIENT_DBG("Initializing device %d", DeviceId)
  auto Response = send(MessageKind::InitDevice, Serializer->I32(DeviceId));
  auto IsInitialized = Serializer->I32(Response);
  if (IsInitialized)
    CLIENT_DBG("Device %d is initialized", DeviceId)
  return IsInitialized;
}

int64_t ClientTy::initRequires(int64_t RequiresFlags) {
  CLIENT_DBG("Initializing requires")
  auto Response =
      send(MessageKind::InitRequires, Serializer->I64(RequiresFlags));
  CLIENT_DBG("Initialized requires")
  return Serializer->I64(Response);
}

__tgt_target_table *ClientTy::loadBinary(int32_t DeviceId,
                                         __tgt_device_image *Image) {
  CLIENT_DBG("Loading binary %p on device %d", Image, DeviceId)
  auto Response =
      send(MessageKind::LoadBinary, Serializer->Binary(DeviceId, Image));
  auto Value = Serializer->TargetTable(Response, RemoteEntries[DeviceId]);
  if (Value)
    CLIENT_DBG("Loaded binary %p on device %d", Image, DeviceId)
  return Value;
}

void *ClientTy::dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) {
  CLIENT_DBG("Allocating %ld bytes for %p on device %d", Size, HstPtr, DeviceId)
  auto Response = send(MessageKind::DataAlloc,
                       Serializer->DataAlloc(DeviceId, Size, HstPtr));
  auto *Value = Serializer->Pointer(Response);
  if (Value)
    CLIENT_DBG("Allocated %ld bytes for %p on device %d", Size, HstPtr,
               DeviceId)
  return Value;
}

int32_t ClientTy::dataDelete(int32_t DeviceId, void *TgtPtr) {
  auto Response =
      send(MessageKind::DataDelete, Serializer->DataDelete(DeviceId, TgtPtr));
  return Serializer->I32(Response);
}

int32_t ClientTy::dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                             int64_t Size) {
  printf("%d, %p, %p, %ld\n", DeviceId, TgtPtr, HstPtr, Size);
  CLIENT_DBG("Submitting %ld bytes from %p to %p on device %d", Size, HstPtr,
             TgtPtr, DeviceId)
  auto Response = send(MessageKind::DataSubmit,
                       Serializer->DataSubmit(DeviceId, TgtPtr, HstPtr, Size));
  auto Value = Serializer->I32(Response);
  if (Value)
    CLIENT_DBG("Submitted %ld bytes from %p to %p on device %d", Size, HstPtr,
               TgtPtr, DeviceId)
  return Value;
}

int32_t ClientTy::dataRetrieve(int32_t DeviceId, void *HstPtr, void *TgtPtr,
                               int64_t Size) {
  CLIENT_DBG("Retrieving %ld bytes from %p to %p on device %d", Size, HstPtr,
             TgtPtr, DeviceId)
  auto Response = send(MessageKind::DataRetrieve,
                       Serializer->DataRetrieve(DeviceId, TgtPtr, Size));

  auto [Buffer, BufferSize, Value] = Serializer->Data(Response);
  std::memcpy(HstPtr, Buffer, BufferSize);

  if (Value)
    CLIENT_DBG("Retrieved %ld bytes from %p to %p on device %d", Size, HstPtr,
               TgtPtr, DeviceId)
  return Value;
}

int32_t ClientTy::runTargetRegion(int32_t DeviceId, void *TgtEntryPtr,
                                  void **TgtArgs, ptrdiff_t *TgtOffsets,
                                  int32_t ArgNum) {
  auto Response =
      send(RunTargetRegion,
           Serializer->TargetRegion(DeviceId, TgtEntryPtr, TgtArgs, TgtOffsets,
                                    ArgNum, RemoteEntries[DeviceId]));

  return Serializer->I32(Response);
}

int32_t ClientTy::runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                                      void **TgtArgs, ptrdiff_t *TgtOffsets,
                                      int32_t ArgNum, int32_t TeamNum,
                                      int32_t ThreadLimit,
                                      uint64_t LoopTripCount) {
  auto Response =
      send(RunTargetTeamRegion,
           Serializer->TargetTeamRegion(
               DeviceId, TgtEntryPtr, TgtArgs, TgtOffsets, ArgNum, TeamNum,
               ThreadLimit, LoopTripCount, RemoteEntries[DeviceId]));

  return Serializer->I32(Response);
}

void ClientTy::shutdown() {}

/*
ProtobufClientTy::ProtobufClientTy(const ConnectionConfigTy &Config)
    : ClientTy(Config, SerializerType::Protobuf) {}

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
  // dump((char *) HstPtr, (char *) HstPtr + Size);
  SubmitData Request;
  Request.set_device_id(DeviceId);
  Request.set_data((char *)HstPtr, Size);
  Request.set_tgt_ptr((uint64_t)TgtPtr);
  auto InterfaceIdx = getInterfaceIdx();

  // dump((char *) HstPtr, (char *) HstPtr + Size);

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

void ProtobufClientTy::shutdown() {}

////////////////////////////////////////////////////////////////////////////////

CustomClientTy::CustomClientTy(const ConnectionConfigTy &Config)
    : ClientTy(Config, SerializerType::Custom) {}

int32_t CustomClientTy::getNumberOfDevices() {
  auto InterfaceIdx = getInterfaceIdx();
  Interfaces[InterfaceIdx]->send(MessageKind::GetNumberOfDevices,
                                 Serializer->EmptyMessage());

  if (!Interfaces[InterfaceIdx]->EP.Connected)
    return 0;

  return Serializer->I32(Interfaces[InterfaceIdx]->receive().second);
}

int32_t CustomClientTy::registerLib(__tgt_bin_desc *Description) {
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::RegisterLib,
                                 Serializer->TargetBinaryDescription(Description));
  Interfaces[InterfaceIdx]->receive();
  return 0;
}

int32_t CustomClientTy::unregisterLib(__tgt_bin_desc *Description) {
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::UnregisterLib,
                                 Serializer->EmptyMessage());
  Interfaces[InterfaceIdx]->receive();

  return 0;
}

int32_t CustomClientTy::isValidBinary(__tgt_device_image *Image) {
  auto InterfaceIdx = getInterfaceIdx();
  Interfaces[InterfaceIdx]->send(MessageKind::IsValidBinary,
                                 Serializer->Pointer((uintptr_t) Image));


  return Serializer->I32(Interfaces[InterfaceIdx]->receive().second);
}

int32_t CustomClientTy::initDevice(int32_t DeviceId) {
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::InitDevice, Serializer->I32(DeviceId));

  return Serializer->I32(Interfaces[InterfaceIdx]->receive().second);
}

int64_t CustomClientTy::initRequires(int64_t RequiresFlags) {
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::InitRequires,
                                 Serializer->I64(RequiresFlags));

  return Serializer->I64(Interfaces[InterfaceIdx]->receive().second);
}

__tgt_target_table *CustomClientTy::loadBinary(int32_t DeviceId,
                                               __tgt_device_image *Image) {
  custom::Binary Request(DeviceId, Image);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::LoadBinary, Request.Message);

  custom::TargetTable Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Table;
}

void *CustomClientTy::dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) {
  custom::DataAlloc Request(DeviceId, Size, HstPtr);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::DataAlloc, Request.Message);

  custom::Pointer Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Value;
}

int32_t CustomClientTy::dataDelete(int32_t DeviceId, void *TgtPtr) {
  custom::DataDelete Request(DeviceId, TgtPtr);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::DataDelete, Request.Message);

  custom::I32 Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Value;
}

int32_t CustomClientTy::dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                                   int64_t Size) {
  custom::DataSubmit Request(DeviceId, TgtPtr, HstPtr, Size);

  auto InterfaceIdx = getInterfaceIdx();
  Interfaces[InterfaceIdx]->send(MessageKind::DataSubmit, Request.Message);

  custom::I32 Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Value;
}

int32_t CustomClientTy::dataRetrieve(int32_t DeviceId, void *HstPtr,
                                     void *TgtPtr, int64_t Size) {
  custom::DataRetrieve Request(DeviceId, HstPtr, TgtPtr, Size);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(MessageKind::DataRetrieve,
                                 Request.Message);

  custom::Data Response(Interfaces[InterfaceIdx]->receive().second);

  std::memcpy(HstPtr, Response.DataBuffer, Response.DataSize);

  return Response.Value;
}

int32_t CustomClientTy::runTargetRegion(int32_t DeviceId, void *TgtEntryPtr,
                                        void **TgtArgs, ptrdiff_t *TgtOffsets,
                                        int32_t ArgNum) {
  custom::TargetRegion Request(DeviceId, TgtEntryPtr, TgtArgs, TgtOffsets,
                               ArgNum);
  auto InterfaceIdx = getInterfaceIdx();

  Interfaces[InterfaceIdx]->send(RunTargetRegion, Request.Message);

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

  Interfaces[InterfaceIdx]->send(RunTargetTeamRegion, Request.Message);

  custom::I32 Response(Interfaces[InterfaceIdx]->receive().second);

  return Response.Value;
}

void CustomClientTy::shutdown() {}
*/

} // namespace transport::ucx
