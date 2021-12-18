#pragma once

#include "Debug.h"
#include "omptarget.h"
#include <vector>

class BaseClientTy {
protected:
  /// Cached Debug Level
  uint32_t DebugLevel = 0;

public:
  BaseClientTy();
  virtual ~BaseClientTy() = default;

  /// Register target binary description with remote host.
  virtual int32_t registerLib(__tgt_bin_desc *Desc) = 0;

  /// Unregister target binary description with remote host.
  virtual int32_t unregisterLib(__tgt_bin_desc *Desc) = 0;

  /// Check if binary is valid on remote host.
  virtual int32_t isValidBinary(__tgt_device_image *Image) = 0;

  /// Get number of devices on remote host.
  virtual int32_t getNumberOfDevices() = 0;

  /// Initialize remote device.
  virtual int32_t initDevice(int32_t DeviceId) = 0;

  /// Initialize requires flag on remote host.
  virtual int64_t initRequires(int64_t RequiresFlags) = 0;

  /// Load binary on remote device.
  virtual __tgt_target_table *loadBinary(int32_t DeviceId,
                                         __tgt_device_image *Image) = 0;

  /// Allocate memory on remote device.
  virtual void *dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) = 0;

  /// Free memory on remote device.
  virtual int32_t dataDelete(int32_t DeviceId, void *TgtPtr) = 0;

  /// Move memory to remote device.
  virtual int32_t dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                             int64_t Size) = 0;

  /// Move memory from remote device.
  virtual int32_t dataRetrieve(int32_t DeviceId, void *HstPtr, void *TgtPtr,
                               int64_t Size) = 0;

  /// Execute target region on remote device.
  virtual int32_t runTargetRegion(int32_t DeviceId, void *TgtEntryPtr,
                                  void **TgtArgs, ptrdiff_t *TgtOffsets,
                                  int32_t ArgNum) = 0;

  /// Execute target teams region on remote device.
  virtual int32_t runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                                      void **TgtArgs, ptrdiff_t *TgtOffsets,
                                      int32_t ArgNum, int32_t TeamNum,
                                      int32_t ThreadLimit,
                                      uint64_t LoopTripCount) = 0;
};

class BaseClientManagerTy {
protected:
  /// Struct for storing device location.
  struct DeviceLocTy {
    int RemoteHostIdx, DeviceIdx;
    DeviceLocTy(int RemoteHostIdx, int DeviceIdx) : RemoteHostIdx(RemoteHostIdx), DeviceIdx(DeviceIdx) {}
  };

  /// Vector of remote client objects.
  std::vector<BaseClientTy*> Clients;

  /// Map host device id to remote device location.
  std::vector<DeviceLocTy> DeviceLocs;

public:
  virtual ~BaseClientManagerTy() = default;

  /// Register target binary description with remote host.
  int32_t registerLib(__tgt_bin_desc *Desc);

  /// Unregister target binary description with remote host.
  int32_t unregisterLib(__tgt_bin_desc *Desc);

  /// Check if binary is valid on remote host.
  int32_t isValidBinary(__tgt_device_image *Image);

  /// Get number of devices on remote host.
  int32_t getNumberOfDevices();

  /// Initialize remote device.
  int32_t initDevice(int32_t DeviceId);

  /// Initialize requires flag on remote host.
  int64_t initRequires(int64_t RequiresFlags);

  /// Load binary on remote device.
  __tgt_target_table *loadBinary(int32_t DeviceId,
                                         __tgt_device_image *Image);
  /// Allocate memory on remote device.
  void *dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr);

  /// Free memory on remote device.
  int32_t dataDelete(int32_t DeviceId, void *TgtPtr);

  /// Move memory to remote device.
  int32_t dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                             int64_t Size);

  /// Move memory from remote device.
  int32_t dataRetrieve(int32_t DeviceId, void *HstPtr, void *TgtPtr,
                               int64_t Size);

  /// Execute target region on remote device.
  int32_t runTargetRegion(int32_t DeviceId, void *TgtEntryPtr,
                                  void **TgtArgs, ptrdiff_t *TgtOffsets,
                                  int32_t ArgNum);

  /// Execute target teams region on remote device.
  int32_t runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                                      void **TgtArgs, ptrdiff_t *TgtOffsets,
                                      int32_t ArgNum, int32_t TeamNum,
                                      int32_t ThreadLimit,
                                      uint64_t LoopTripCount);
};
