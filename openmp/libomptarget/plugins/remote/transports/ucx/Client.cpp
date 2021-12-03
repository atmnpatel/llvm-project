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

ClientTy::ClientTy(ConnectionConfigTy Config, SerializerType Type) : Config(Config), Interface(new Base::InterfaceTy(Context, Config)) {
  switch (Type) {
  case SerializerType::Custom:
    Serializer = (SerializerTy *)new CustomSerializerTy();
    break;
  case SerializerType::Protobuf:
    Serializer = (SerializerTy *)new ProtobufSerializerTy();
    break;
  }
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
  CLIENT_DBG("Submitting %ld bytes from %p to %p on device %d", Size, HstPtr,
             TgtPtr, DeviceId)

  if (Size >= 5189760) {

  { // If Sent
    std::lock_guard Guard(SentDataMtx);
    if (SentData.contains(HstPtr)) {
      return 0;
    }
  }

  std::mutex *Mtx;
  std::condition_variable *CV;
  {
    std::lock_guard Guard(InProgressMtx);
    if (!InProgress.contains(HstPtr))
      InProgress[HstPtr] = std::make_unique<std::mutex>();
    if (!InProgressCVs.contains(HstPtr))
      InProgressCVs[HstPtr] = std::make_unique<std::condition_variable>();

    Mtx = InProgress[HstPtr].get();
    CV = InProgressCVs[HstPtr].get();
  }

  bool IsInProgress = false;
  { // Check if its being sent
    std::lock_guard Guard(SendingDataMtx);
    IsInProgress = SendingData.contains(HstPtr);
  }

  if (IsInProgress) {
    std::unique_lock Latch(*Mtx);
    CV->wait(Latch, [&] () {
      std::lock_guard Guard(SentDataMtx);
      return SentData.contains(HstPtr);
    });

    return 0;
  } else {
    MessageBufferTy Response;
    {
      std::lock_guard Guard(*Mtx);
      {
        std::lock_guard InnerGuard(SendingDataMtx);
        SendingData.insert(HstPtr);
      }
      Response =
          send(MessageKind::DataSubmit,
               Serializer->DataSubmit(DeviceId, TgtPtr, HstPtr, Size));
      {
        std::lock_guard Guard(SentDataMtx);
        SentData.insert(HstPtr);
      }
    }
    CV->notify_all();
    auto Value = Serializer->I32(Response);
    if (!Value)
      CLIENT_DBG("Submitted %ld bytes from %p to %p on device %d", Size, HstPtr,
                 TgtPtr, DeviceId)
    return Value;
  }
  } else {
    auto Response =
          send(MessageKind::DataSubmit,
               Serializer->DataSubmit(DeviceId, TgtPtr, HstPtr, Size));
    auto Value = Serializer->I32(Response);
    if (!Value)
      CLIENT_DBG("Submitted %ld bytes from %p to %p on device %d", Size, HstPtr,
                 TgtPtr, DeviceId)
    return Value;
  }
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

} // namespace transport::ucx
