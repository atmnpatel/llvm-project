#include "../../src/BaseClient.h"
#include "Base.h"
#include "Serialization.h"
#include "Utils.h"
#include "omptarget.h"
#include <memory>

namespace transport {
namespace ucx {

class ClientTy : public Base, public BaseClientTy {};

class ProtobufClientTy : public ClientTy {
  Base::InterfaceTy Interface;
  std::map<int32_t, std::unordered_map<void *, void *>> RemoteEntries;
  std::map<int32_t, std::unique_ptr<__tgt_target_table>> DevicesToTables;
  int DebugLevel;

  std::string getHeader(MessageTy Type) {
    HeaderTy Header;
    Header.set_type(Type);
    return Header.SerializeAsString();
  }

public:
  ProtobufClientTy(const ConnectionConfigTy &Config);

  int32_t shutdown() override { return 0; }

  int32_t registerLib(__tgt_bin_desc *Desc) override;
  int32_t unregisterLib(__tgt_bin_desc *Desc) override;

  int32_t isValidBinary(__tgt_device_image *Image) override;
  int32_t getNumberOfDevices() override;

  int32_t initDevice(int32_t DeviceId) override;
  int64_t initRequires(int64_t RequiresFlags) override;

  __tgt_target_table *loadBinary(int32_t DeviceId,
                                 __tgt_device_image *Image) override;

  void *dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) override;
  int32_t dataDelete(int32_t DeviceId, void *TgtPtr) override;

  int32_t dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                     int64_t Size) override;
  int32_t dataRetrieve(int32_t DeviceId, void *HstPtr, void *TgtPtr,
                       int64_t Size) override;

  int32_t isDataExchangeable(int32_t SrcDevId, int32_t DstDevId) override;
  int32_t dataExchange(int32_t SrcDevId, void *SrcPtr, int32_t DstDevId,
                       void *DstPtr, int64_t Size) override;

  int32_t runTargetRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
                          ptrdiff_t *TgtOffsets, int32_t ArgNum) override;

  int32_t runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                              void **TgtArgs, ptrdiff_t *TgtOffsets,
                              int32_t ArgNum, int32_t TeamNum,
                              int32_t ThreadLimit,
                              uint64_t LoopTripCount) override;
};

class ClientManagerTy final : public BaseClientManagerTy {
  std::vector<std::unique_ptr<ClientTy>> Clients;
  std::vector<int> NumDevicesInClient;

  std::pair<int32_t, int32_t> mapDeviceId(int32_t DeviceId) override;

public:
  ClientManagerTy(bool Protobuf);

  int32_t shutdown() override { return 0; }

  int32_t registerLib(__tgt_bin_desc *Desc) override;
  int32_t unregisterLib(__tgt_bin_desc *Desc) override;

  int32_t isValidBinary(__tgt_device_image *Image) override;
  int32_t getNumberOfDevices() override;

  int32_t initDevice(int32_t DeviceId) override;
  int64_t initRequires(int64_t RequiresFlags) override;

  __tgt_target_table *loadBinary(int32_t DeviceId,
                                 __tgt_device_image *Image) override;

  void *dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) override;
  int32_t dataDelete(int32_t DeviceId, void *TgtPtr) override;

  int32_t dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                     int64_t Size) override;
  int32_t dataRetrieve(int32_t DeviceId, void *HstPtr, void *TgtPtr,
                       int64_t Size) override;

  int32_t isDataExchangeable(int32_t SrcDevId, int32_t DstDevId) override;
  int32_t dataExchange(int32_t SrcDevId, void *SrcPtr, int32_t DstDevId,
                       void *DstPtr, int64_t Size) override;

  int32_t runTargetRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
                          ptrdiff_t *TgtOffsets, int32_t ArgNum) override;

  int32_t runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                              void **TgtArgs, ptrdiff_t *TgtOffsets,
                              int32_t ArgNum, int32_t TeamNum,
                              int32_t ThreadLimit,
                              uint64_t LoopTripCount) override;
};

class SelfClientTy : public ClientTy {
  Base::InterfaceTy Interface;
  std::map<int32_t, std::unordered_map<void *, void *>> RemoteEntries;

public:
  SelfClientTy(const ConnectionConfigTy &Config);

  int32_t shutdown() override { return 0; }

  int32_t registerLib(__tgt_bin_desc *Desc) override;
  int32_t unregisterLib(__tgt_bin_desc *Desc) override;

  int32_t isValidBinary(__tgt_device_image *Image) override;
  int32_t getNumberOfDevices() override;

  int32_t initDevice(int32_t DeviceId) override;
  int64_t initRequires(int64_t RequiresFlags) override;

  __tgt_target_table *loadBinary(int32_t DeviceId,
                                 __tgt_device_image *Image) override;

  void *dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) override;
  int32_t dataDelete(int32_t DeviceId, void *TgtPtr) override;

  int32_t dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                     int64_t Size) override;
  int32_t dataRetrieve(int32_t DeviceId, void *HstPtr, void *TgtPtr,
                       int64_t Size) override;

  int32_t isDataExchangeable(int32_t SrcDevId, int32_t DstDevId) override;
  int32_t dataExchange(int32_t SrcDevId, void *SrcPtr, int32_t DstDevId,
                       void *DstPtr, int64_t Size) override;

  int32_t runTargetRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
                          ptrdiff_t *TgtOffsets, int32_t ArgNum) override;

  int32_t runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                              void **TgtArgs, ptrdiff_t *TgtOffsets,
                              int32_t ArgNum, int32_t TeamNum,
                              int32_t ThreadLimit,
                              uint64_t LoopTripCount) override;
};
} // namespace ucx
} // namespace transport