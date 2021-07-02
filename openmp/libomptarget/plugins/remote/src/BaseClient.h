#pragma once

#include "omptarget.h"
#include <vector>

class BaseClientTy {
protected:
  int DebugLevel;

public:
  virtual ~BaseClientTy() {};

  virtual int32_t shutdown(void) = 0;

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

  virtual int32_t isDataExchangeable(int32_t SrcDevId, int32_t DstDevId) = 0;
  virtual int32_t dataExchange(int32_t SrcDevId, void *SrcPtr, int32_t DstDevId,
                               void *DstPtr, int64_t Size) = 0;

  virtual int32_t runTargetRegion(int32_t DeviceId, void *TgtEntryPtr,
                                  void **TgtArgs, ptrdiff_t *TgtOffsets,
                                  int32_t ArgNum) = 0;
  virtual int32_t runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                                      void **TgtArgs, ptrdiff_t *TgtOffsets,
                                      int32_t ArgNum, int32_t TeamNum,
                                      int32_t ThreadLimit,
                                      uint64_t LoopTripCount) = 0;
};

class BaseClientManagerTy {
protected:
  int DebugLevel;
  std::vector<int> Devices;

  virtual std::pair<int32_t, int32_t> mapDeviceId(int32_t DeviceId) = 0;

public:
  virtual ~BaseClientManagerTy() {}

  virtual int32_t shutdown(void) = 0;

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

  virtual int32_t isDataExchangeable(int32_t SrcDevId, int32_t DstDevId) = 0;
  virtual int32_t dataExchange(int32_t SrcDevId, void *SrcPtr, int32_t DstDevId,
                               void *DstPtr, int64_t Size) = 0;

  virtual int32_t runTargetRegion(int32_t DeviceId, void *TgtEntryPtr,
                                  void **TgtArgs, ptrdiff_t *TgtOffsets,
                                  int32_t ArgNum) = 0;
  virtual int32_t runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                                      void **TgtArgs, ptrdiff_t *TgtOffsets,
                                      int32_t ArgNum, int32_t TeamNum,
                                      int32_t ThreadLimit,
                                      uint64_t LoopTripCount) = 0;
};
