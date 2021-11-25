#include "Base.h"
#include "BaseClient.h"
#include "Serialization.h"
#include "Utils.h"
#include "omptarget.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <type_traits>
#include "Serializer.h"

namespace transport::ucx {

class ClientTy : public Base, public BaseClientTy {
protected:
  SerializerTy *Serializer;

  std::map<int32_t, std::unordered_map<void *, void *>> RemoteEntries{};
  std::map<int32_t, std::unique_ptr<__tgt_target_table>> DevicesToTables{};

  MessageBufferTy send(MessageKind Kind, std::string Message) {
    auto Tag = Interface->GetTag(Kind);
    Interface->send(Tag, Message);
    return Interface->receive(Tag).Buffer;
  }

public:
  ClientTy(ConnectionConfigTy Config, SerializerType Type);
  ConnectionConfigTy Config;
  std::unique_ptr<Base::InterfaceTy> Interface;

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

  int32_t runTargetRegion(int32_t DeviceId, void *TgtEntryPtr, void **TgtArgs,
                          ptrdiff_t *TgtOffsets, int32_t ArgNum) override;

  int32_t runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                              void **TgtArgs, ptrdiff_t *TgtOffsets,
                              int32_t ArgNum, int32_t TeamNum,
                              int32_t ThreadLimit,
                              uint64_t LoopTripCount) override;

    void shutdown() override;
};

struct ClientManagerTy final : public BaseClientManagerTy {
  explicit ClientManagerTy(SerializerType Type);
};

} // namespace transport::ucx
