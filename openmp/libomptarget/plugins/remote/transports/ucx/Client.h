#include "Base.h"
#include "BaseClient.h"
#include "Utils.h"
#include "omptarget.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include "Serializer.h"

namespace transport::ucx {

class ClientTy : public Base, public BaseClientTy {
  /// Call init requires once flag.
  std::once_flag RequiresOnceFlag;

  /// UCX interface for remote host.
  InterfaceTy Interface;

  /// Serializer for objects to transmit.
  SerializerTy Serializer;

  std::map<int32_t, std::unordered_map<void *, void *>> RemoteEntries{};

  MessageBufferTy send(MessageKind Kind, std::string Message) {
    const auto Tag = Interface.getTag(Kind);
    Interface.send(Tag, Message);
    return Interface.receive(Tag).Buffer;
  }

public:
  ClientTy(ConnectionConfigTy Config);

  /// Register library on remote host.
  int32_t registerLib(__tgt_bin_desc *Desc) override;

  /// Unregister library on remote host.
  int32_t unregisterLib(__tgt_bin_desc *Desc) override;

  /// Check if binary is valid on remote host.
  int32_t isValidBinary(__tgt_device_image *Image) override;

  /// Get number of devices on remote host.
  int32_t getNumberOfDevices() override;

  /// Initialize remote device.
  int32_t initDevice(int32_t DeviceId) override;

  /// Initialize requires on remote host.
  int64_t initRequires(int64_t RequiresFlags) override;

  /// Load binary on remote device.
  __tgt_target_table *loadBinary(int32_t DeviceId,
                                 __tgt_device_image *Image) override;

  /// Allocate memory on remote device.
  void *dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) override;

  /// Free memory on remote device.
  int32_t dataDelete(int32_t DeviceId, void *TgtPtr) override;

  /// Move data to remote device.
  int32_t dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                     int64_t Size) override;

  /// Move data from remote device.
  int32_t dataRetrieve(int32_t DeviceId, void *HstPtr, void *TgtPtr,
                       int64_t Size) override;

  /// Run target region on remote device.
  int32_t runTargetRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
                          ptrdiff_t *TgtOffsets, int32_t ArgNum) override;

  /// Run target teams region on remote device.
  int32_t runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                              void **TgtArgs, ptrdiff_t *TgtOffsets,
                              int32_t ArgNum, int32_t TeamNum,
                              int32_t ThreadLimit,
                              uint64_t LoopTripCount) override;
};

struct ClientManagerTy final : public BaseClientManagerTy {
  explicit ClientManagerTy();
};

} // namespace transport::ucx
