#include "BaseClient.h"
#include "llvm/Support/ErrorHandling.h"
#include <numeric>

std::pair<int32_t, int32_t> BaseClientManagerTy::mapDeviceId(int32_t DeviceId) {
  for (size_t ClientIdx = 0; ClientIdx < Devices.size(); ClientIdx++) {
    if (DeviceId < Devices[ClientIdx])
      return {ClientIdx, DeviceId};
    DeviceId -= Devices[ClientIdx];
  }
  llvm::llvm_unreachable_internal("Invalid Device Id");
}

int32_t BaseClientManagerTy::registerLib(__tgt_bin_desc *Desc) {
  int32_t Ret = 0;
  for (auto &Client : Clients)
    Ret &= Client->registerLib(Desc);
  return Ret;
}

int32_t BaseClientManagerTy::unregisterLib(__tgt_bin_desc *Desc) {
  int32_t Ret = 0;
  for (auto &Client : Clients)
    Ret &= Client->unregisterLib(Desc);
  return Ret;
}

int32_t BaseClientManagerTy::isValidBinary(__tgt_device_image *Image) {
  int32_t ClientIdx = 0;
  for (auto &Client : Clients) {
    if (auto Ret = Client->isValidBinary(Image))
      return Ret;
    ClientIdx++;
  }
  return 0;
}

int32_t BaseClientManagerTy::getNumberOfDevices() {
  auto ClientIdx = 0;
  for (auto &Client : Clients) {
    if (auto NumDevices = Client->getNumberOfDevices()) {
      Devices.push_back(NumDevices);
    }
    ClientIdx++;
  }

  return std::accumulate(Devices.begin(), Devices.end(), 0);
}

int32_t BaseClientManagerTy::initDevice(int32_t DeviceId) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->initDevice(DeviceIdx);
}

int64_t BaseClientManagerTy::initRequires(int64_t RequiresFlags) {
  for (auto &Client : Clients)
    Client->initRequires(RequiresFlags);

  return RequiresFlags;
}

__tgt_target_table *BaseClientManagerTy::loadBinary(int32_t DeviceId,
                                                __tgt_device_image *Image) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->loadBinary(DeviceIdx, Image);
}

void *BaseClientManagerTy::dataAlloc(int32_t DeviceId, int64_t Size,
                                     void *HstPtr) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->dataAlloc(DeviceIdx, Size, HstPtr);
}

int32_t BaseClientManagerTy::dataDelete(int32_t DeviceId, void *TgtPtr) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->dataDelete(DeviceIdx, TgtPtr);
}

int32_t BaseClientManagerTy::dataSubmit(int32_t DeviceId, void *TgtPtr,
                                    void *HstPtr, int64_t Size) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->dataSubmit(DeviceIdx, TgtPtr, HstPtr, Size);
}

int32_t BaseClientManagerTy::dataRetrieve(int32_t DeviceId, void *HstPtr,
                                      void *TgtPtr, int64_t Size) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->dataRetrieve(DeviceIdx, HstPtr, TgtPtr, Size);
}

int32_t BaseClientManagerTy::runTargetRegion(int32_t DeviceId,
                                             void *TgtEntryPtr, void **TgtArgs,
                                             ptrdiff_t *TgtOffsets,
                                             int32_t ArgNum) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->runTargetRegion(DeviceIdx, TgtEntryPtr, TgtArgs,
                                            TgtOffsets, ArgNum);
}

int32_t BaseClientManagerTy::runTargetTeamRegion(int32_t DeviceId,
                                             void *TgtEntryPtr, void **TgtArgs,
                                             ptrdiff_t *TgtOffsets,
                                             int32_t ArgNum, int32_t TeamNum,
                                             int32_t ThreadLimit,
                                             uint64_t LoopTripCount) {
  int32_t ClientIdx, DeviceIdx;
  std::tie(ClientIdx, DeviceIdx) = mapDeviceId(DeviceId);
  return Clients[ClientIdx]->runTargetTeamRegion(DeviceIdx, TgtEntryPtr, TgtArgs,
                                                TgtOffsets, ArgNum, TeamNum,
                                                ThreadLimit, LoopTripCount);
}

void BaseClientManagerTy::shutdown() {
  for (auto &Client : Clients)
    Client->shutdown();
}
