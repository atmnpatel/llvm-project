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

  uint64_t Tag = 0;

  std::map<int32_t, std::unordered_map<void *, void *>> RemoteEntries{};
  std::map<int32_t, std::unique_ptr<__tgt_target_table>> DevicesToTables{};

  std::string send(MessageKind Kind, std::string Message) {
    /*
    if (MultiThreaded) {
      auto SendFuture = asyncSend(Kind, Message);
      auto RecvFuture = asyncRecv(SendFuture.Tag);

      WorkAvailable.notify_all();

      std::unique_lock<std::mutex> UniqueLock((WorkDoneMtx));
      WorkDone.wait(UniqueLock, [&]() {
        return ((bool)*SendFuture.IsCompleted) &&
               ((bool)*RecvFuture.IsCompleted);
      });

      return {*RecvFuture.Buffer};
    } else {
     */

    Interfaces[0]->send(Kind, Message);
    return Interfaces[0]->receive().second;
  }

public:
  ClientTy(ConnectionConfigTy Config, SerializerType Type);
  ConnectionConfigTy Config;
  std::map<std::thread::id, size_t> InterfaceId;
  std::vector<std::unique_ptr<Base::InterfaceTy>> Interfaces;

  size_t getInterfaceIdx() {
    std::stringstream SS;
    SS << std::this_thread::get_id();

    if (!InterfaceId.contains(std::this_thread::get_id())) {
      auto Interface = std::make_unique<InterfaceTy>(Context, Config);
      Interfaces.push_back(std::move(Interface));
      InterfaceId[std::this_thread::get_id()] = Interfaces.size() - 1;
    }

    auto NextInterfaceIdx = InterfaceId[std::this_thread::get_id()];
    return NextInterfaceIdx;
  }


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
