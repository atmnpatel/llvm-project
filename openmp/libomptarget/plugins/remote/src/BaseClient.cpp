#include "BaseClient.h"

BaseClientTy::BaseClientTy() : DebugLevel(getDebugLevel()) {}

int32_t BaseClientManagerTy::registerLib(__tgt_bin_desc *Desc) {
  for (auto &Client : Clients)
    if (auto RC = Client->registerLib(Desc))
      return RC;

  return 0;
}

int32_t BaseClientManagerTy::unregisterLib(__tgt_bin_desc *Desc) {
  for (auto &Client : Clients)
    if (auto RC = Client->unregisterLib(Desc))
      return RC;

  return 0;
}

int32_t BaseClientManagerTy::isValidBinary(__tgt_device_image *Image) {
  for (auto &Client : Clients)
    if (auto RC = Client->isValidBinary(Image))
      return RC;

  return 0;
}

int32_t BaseClientManagerTy::getNumberOfDevices() {
  auto ClientIdx = 0;
  auto TotalNumDevices = 0;

  for (auto &Client : Clients) {
    if (auto NumDevices = Client->getNumberOfDevices()) {
      for (auto Idx = 0; Idx < NumDevices; Idx++)
        DeviceLocs.emplace_back(ClientIdx, Idx);
      TotalNumDevices += NumDevices;
    }

    ClientIdx++;
  }

  return TotalNumDevices;
}

int32_t BaseClientManagerTy::initDevice(int32_t DeviceId) {
  const auto Loc = DeviceLocs[DeviceId];
  return Clients[Loc.RemoteHostIdx]->initDevice(Loc.DeviceIdx);
}

int64_t BaseClientManagerTy::initRequires(int64_t RequiresFlags) {
  for (auto &Client : Clients)
    Client->initRequires(RequiresFlags);

  return RequiresFlags;
}

__tgt_target_table *BaseClientManagerTy::loadBinary(int32_t DeviceId,
                                                __tgt_device_image *Image) {
  const auto Loc = DeviceLocs[DeviceId];
  return Clients[Loc.RemoteHostIdx]->loadBinary(Loc.DeviceIdx, Image);
}

void *BaseClientManagerTy::dataAlloc(int32_t DeviceId, int64_t Size,
                                     void *HstPtr) {
  const auto Loc = DeviceLocs[DeviceId];
  return Clients[Loc.RemoteHostIdx]->dataAlloc(Loc.DeviceIdx, Size, HstPtr);
}

int32_t BaseClientManagerTy::dataDelete(int32_t DeviceId, void *TgtPtr) {
  const auto Loc = DeviceLocs[DeviceId];
  return Clients[Loc.RemoteHostIdx]->dataDelete(Loc.DeviceIdx, TgtPtr);
}

int32_t BaseClientManagerTy::dataSubmit(int32_t DeviceId, void *TgtPtr,
                                    void *HstPtr, int64_t Size) {
  const auto Loc = DeviceLocs[DeviceId];
  return Clients[Loc.RemoteHostIdx]->dataSubmit(Loc.DeviceIdx, TgtPtr, HstPtr, Size);
}

int32_t BaseClientManagerTy::dataRetrieve(int32_t DeviceId, void *HstPtr,
                                      void *TgtPtr, int64_t Size) {
  const auto Loc = DeviceLocs[DeviceId];
  return Clients[Loc.RemoteHostIdx]->dataRetrieve(Loc.DeviceIdx, HstPtr, TgtPtr, Size);
}

int32_t BaseClientManagerTy::runTargetRegion(int32_t DeviceId,
                                             void *TgtEntryPtr, void **TgtArgs,
                                             ptrdiff_t *TgtOffsets,
                                             int32_t ArgNum) {
  const auto Loc = DeviceLocs[DeviceId];
  return Clients[Loc.RemoteHostIdx]->runTargetRegion(Loc.DeviceIdx, TgtEntryPtr, TgtArgs,
                                            TgtOffsets, ArgNum);
}

int32_t BaseClientManagerTy::runTargetTeamRegion(int32_t DeviceId,
                                             void *TgtEntryPtr, void **TgtArgs,
                                             ptrdiff_t *TgtOffsets,
                                             int32_t ArgNum, int32_t TeamNum,
                                             int32_t ThreadLimit,
                                             uint64_t LoopTripCount) {
  const auto Loc = DeviceLocs[DeviceId];
  return Clients[Loc.RemoteHostIdx]->runTargetTeamRegion(Loc.DeviceIdx, TgtEntryPtr, TgtArgs,
                                                TgtOffsets, ArgNum, TeamNum,
                                                ThreadLimit, LoopTripCount);
}
