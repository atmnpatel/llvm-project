#pragma once

#include "Debug.h"
#include "omptarget.h"
#include <vector>
#include <memory>

class BaseClientTy {
protected:
  uint32_t DebugLevel = 0;

public:
  BaseClientTy() : DebugLevel(getDebugLevel()) {}
  virtual ~BaseClientTy() = default;

  virtual int32_t registerLib(__tgt_bin_desc *Desc) = 0;
  virtual int32_t unregisterLib(__tgt_bin_desc *Desc) = 0;

  virtual int32_t isValidBinary(__tgt_device_image *Image) = 0;
  virtual int32_t getNumberOfDevices() = 0;

  virtual int32_t initDevice(int32_t DeviceId) = 0;
  virtual int64_t initRequires(int64_t RequiresFlags) = 0;

  virtual __tgt_target_table *loadBinary(int32_t DeviceId,
                                         __tgt_device_image *Image) = 0;

  virtual void *dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) = 0;
  virtual int32_t dataDelete(int32_t DeviceId, void *TgtPtr) = 0;

  virtual int32_t dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                             int64_t Size) = 0;
  virtual int32_t dataRetrieve(int32_t DeviceId, void *HstPtr, void *TgtPtr,
                               int64_t Size) = 0;

  virtual int32_t runTargetRegion(int32_t DeviceId, void *TgtEntryPtr,
                                  void **TgtArgs, ptrdiff_t *TgtOffsets,
                                  int32_t ArgNum) = 0;
  virtual int32_t runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                                      void **TgtArgs, ptrdiff_t *TgtOffsets,
                                      int32_t ArgNum, int32_t TeamNum,
                                      int32_t ThreadLimit,
                                      uint64_t LoopTripCount) = 0;

  virtual void shutdown() = 0;
};

class BaseClientManagerTy {
protected:
  std::vector<int> Devices;
  std::vector<BaseClientTy*> Clients;

  std::pair<int32_t, int32_t> mapDeviceId(int32_t DeviceId);

public:
  virtual ~BaseClientManagerTy() = default;

  int32_t registerLib(__tgt_bin_desc *Desc);
  int32_t unregisterLib(__tgt_bin_desc *Desc);

  int32_t isValidBinary(__tgt_device_image *Image);
  int32_t getNumberOfDevices();

  int32_t initDevice(int32_t DeviceId);
  int64_t initRequires(int64_t RequiresFlags);

  __tgt_target_table *loadBinary(int32_t DeviceId,
                                         __tgt_device_image *Image);

  void *dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr);
  int32_t dataDelete(int32_t DeviceId, void *TgtPtr);

  int32_t dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                             int64_t Size);
  int32_t dataRetrieve(int32_t DeviceId, void *HstPtr, void *TgtPtr,
                               int64_t Size);

  int32_t runTargetRegion(int32_t DeviceId, void *TgtEntryPtr,
                                  void **TgtArgs, ptrdiff_t *TgtOffsets,
                                  int32_t ArgNum);
  int32_t runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                                      void **TgtArgs, ptrdiff_t *TgtOffsets,
                                      int32_t ArgNum, int32_t TeamNum,
                                      int32_t ThreadLimit,
                                      uint64_t LoopTripCount);

  void shutdown();
};
