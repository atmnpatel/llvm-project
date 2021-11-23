#include "Server.h"
#include "Base.h"
#include "Debug.h"
#include "Serializer.h"
#include "Utils.h"
#include "messages.pb.h"
#include "omptarget.h"
#include "ucp/api/ucp.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>

namespace transport::ucx {

ServerTy::ServerTy(SerializerType Type)
    : Base(), ConnectionWorker((ucp_context_h *)Context),
      DebugLevel(getDebugLevel()), TBD(new __tgt_bin_desc) {
  switch (Type) {
  case SerializerType::Custom:
    Serializer = (SerializerTy *)new CustomSerializerTy();
    break;
  case SerializerType::Protobuf:
    Serializer = (SerializerTy *)new ProtobufSerializerTy();
    break;
  }

  if (NumThreads > 1) {

    for (auto I = 0; I < NumThreads; I++) {
      ThreadPool.emplace_back([this]() {
        while (true) {
          std::unique_lock Latch(QueueMtx);

          TaskAvailable.wait(Latch,
                             [this]() { return Running || !Tasks.empty(); });

          if (!Tasks.empty()) {
            BusyThreads++;

            auto [Kind, Message] = Tasks.front();
            Tasks.pop();

            Latch.unlock();

            process(Kind, Message);

            Latch.lock();
            --BusyThreads;
            TaskFinished.notify_one();
          } else if (!Running) {
            break;
          }
        }
      });
    }
  }
}

ServerTy::~ServerTy() {
  std::unique_lock<std::mutex> lock(QueueMtx);
  TaskFinished.wait(lock, [this](){ return Tasks.empty() && (BusyThreads == 0); });
}

ServerTy::ListenerTy::ListenerTy(ucp_worker_h Worker, ServerContextTy *Context,
                                 const ConnectionConfigTy &Config)
    : Listener(&Context->Listener) {
  sockaddr_in ListenAddress = {
      .sin_family = AF_INET,
      .sin_port = htons(Config.Port),
      .sin_addr{.s_addr = (Config.Address.size())
                              ? inet_addr(Config.Address.c_str())
                              : INADDR_ANY}};

  ucp_listener_params_t Params = {
      .field_mask = UCP_LISTENER_PARAM_FIELD_SOCK_ADDR |
                    UCP_LISTENER_PARAM_FIELD_CONN_HANDLER,
      .sockaddr = {.addr = (const struct sockaddr *)&ListenAddress,
                   .addrlen = sizeof(ListenAddress)},
      .conn_handler = {.cb = handleConnectionCallback, .arg = Context}};

  /* Create a listener to listen on the given socket address.*/
  if (auto Status = ucp_listener_create(Worker, &Params, Listener))
    ERR("Failed to listen: {0}", ucs_status_string(Status))
}

void ServerTy::ListenerTy::query() {
  ucp_listener_attr_t Attr = {.field_mask = UCP_LISTENER_ATTR_FIELD_SOCKADDR};
  auto Status = ucp_listener_query(*Listener, &Attr);
  if (Status != UCS_OK)
    ERR("failed to query the listener ({0})\n", ucs_status_string(Status))

  DP("Server is listening on IP %s port %s\n", getIP(&Attr.sockaddr).c_str(),
     getPort(&Attr.sockaddr).c_str());
}


void ServerTy::listenForConnections(const ConnectionConfigTy &Config) {
  Running = true;
  ServerContextTy Ctx;
  ListenerTy Listener((ucp_worker_h)ConnectionWorker, &Ctx, Config);

  Listener.query(); // For Info only
  while (Running) {
    while (Ctx.ConnRequests.empty() && Running) {
      if (ucp_worker_progress((ucp_worker_h)ConnectionWorker))
        continue;
      auto Status = ucp_worker_wait(ConnectionWorker);
      if (Status != UCS_OK) {
        ERR("Failed to recv message {0}", ucs_status_string(Status))
      }
    }

    for (auto *ConnRequest : Ctx.ConnRequests) {
      Interface = std::make_unique<InterfaceTy>(Context, ConnRequest);
      run();
    }

    Ctx.ConnRequests.clear();
  }
}

void ServerTy::run() {
  if (NumThreads > 1) {
    std::unique_lock Latch(QueueMtx);
    Latch.unlock();
    do {
      auto [Type, Message] = Interface->receive();
      Latch.lock();
      Tasks.emplace(Type, std::move(Message));
      Latch.unlock();
    } while (Running);
  } else {
    do {
      auto [Type, Message] = Interface->receive();
      process(Type, Message);
    } while (Running);
  }
}

void ServerTy::process(MessageKind Type, std::string_view Message) {
   switch (Type) {
   case GetNumberOfDevices: {
     getNumberOfDevices();
     break;
   }
   case RegisterLib: {
     registerLib(Message);
     break;
   }
   case IsValidBinary: {
     isValidBinary(Message);
     break;
   }
   case InitRequires: {
     initRequires(Message);
     break;
   }
   case InitDevice: {
     initDevice(Message);
     break;
   }
   case LoadBinary: {
     loadBinary(Message);
     break;
   }
   case DataAlloc: {
     dataAlloc(Message);
     break;
   }
   case DataSubmit: {
     dataSubmit(Message);
     break;
   }
   case DataRetrieve: {
     dataRetrieve(Message);
     break;
   }
   case RunTargetRegion: {
     runTargetRegion(Message);
     break;
   }
   case RunTargetTeamRegion: {
     runTargetTeamRegion(Message);
     break;
   }
   case DataDelete: {
     dataDelete(Message);
     break;
   }
   case UnregisterLib: {
     unregisterLib(Message);
     Interface->Running = false;
     Running = false;
     break;
   }
   default: {
     llvm_unreachable("Unimplemented Message Type");
   }
   }
}

////////////////////////////////////////////////////////////////////////////////

void ServerTy::getNumberOfDevices() {
  PM->RTLsMtx.lock();
  for (auto &RTL : PM->RTLs.AllRTLs)
    Devices += RTL.NumberOfDevices;
  PM->RTLsMtx.unlock();

  SERVER_DBG("Found %d devices", Devices)
  Interface->send(GetNumberOfDevices, Serializer->I32(Devices));
}

void ServerTy::registerLib(std::string_view Message) {
  TBD = Serializer->TargetBinaryDescription(Message, HostToRemoteDeviceImage);

  PM->RTLs.RegisterLib(TBD);

  Interface->send(RegisterLib, Serializer->EmptyMessage());
}

void ServerTy::isValidBinary(std::string_view Message) {
  auto Value = Serializer->Pointer(Message);

  __tgt_device_image *DeviceImage =
      HostToRemoteDeviceImage[(void *)Value];

  int32_t IsValid = false;
  for (auto &RTL : PM->RTLs.AllRTLs)
    if (RTL.is_valid_binary(DeviceImage)) {
      IsValid = true;
      break;
    }

  Interface->send(IsValidBinary, Serializer->I32(IsValid));
}

void ServerTy::initRequires(std::string_view Message) {
  auto Value = Serializer->I64(Message);

  for (auto &Device : PM->Devices)
    if (Device->RTL->init_requires)
      Device->RTL->init_requires(Value);

  Interface->send(InitRequires, Serializer->I64(Value));
}

void ServerTy::initDevice(std::string_view Message) {
  auto DeviceId = Serializer->I32(Message);

  auto Value = PM->Devices[DeviceId]->RTL->init_device(
      mapHostRTLDeviceId(DeviceId));

  SERVER_DBG("Initialized device %d, Err: %d", DeviceId, Value);

  Interface->send(InitDevice, Serializer->I32(Value));
}

void ServerTy::loadBinary(std::string_view Message) {
  auto Request = Serializer->Binary(Message);

  __tgt_device_image *Image = HostToRemoteDeviceImage[(void *)Request.second];

  auto *TT = PM->Devices[Request.first]->RTL->load_binary(
      mapHostRTLDeviceId(Request.first), Image);

  if (TT) {
    Interface->send(LoadBinary, Serializer->TargetTable(TT));
  } else {
    ERR("Could not load binary");
  }
}

void ServerTy::dataAlloc(std::string_view Message) {
  auto [DeviceId, AllocSize, HstPtr] = Serializer->DataAlloc(Message);

  auto TgtPtr = (uint64_t)PM->Devices[DeviceId]->RTL->data_alloc(
      mapHostRTLDeviceId(DeviceId), AllocSize,
      (void *)HstPtr, TARGET_ALLOC_DEFAULT);

  Interface->send(DataAlloc, Serializer->Pointer(TgtPtr));
}

void ServerTy::dataSubmit(std::string_view Message) {
  auto [DeviceId, TgtPtr, HstPtr, DataSize] = Serializer->DataSubmit(Message);

  auto Ret = PM->Devices[DeviceId]->RTL->data_submit(
      mapHostRTLDeviceId(DeviceId), TgtPtr, HstPtr,
      DataSize);

  Interface->send(DataSubmit, Serializer->I32(Ret));
}

void ServerTy::dataRetrieve(std::string_view Message) {
  auto [DeviceId, TgtPtr, DataSize] = Serializer->DataRetrieve(Message);

  auto *HstPtr = new char[DataSize];

  int32_t Value = PM->Devices[DeviceId]->RTL->data_retrieve(
      mapHostRTLDeviceId(DeviceId), (void *)HstPtr,
      (void *)TgtPtr, DataSize);

  Interface->send(DataRetrieve, Serializer->Data(HstPtr, DataSize, Value));

  delete[] HstPtr;
}

void ServerTy::runTargetRegion(std::string_view Message) {
  auto [DeviceId, TgtEntryPtr, TgtArgs, TgtOffsets, ArgNum] = Serializer->TargetRegion(Message);

  auto Ret = 0;

  Ret = PM->Devices[DeviceId]->RTL->run_region(
      mapHostRTLDeviceId(DeviceId), (void *)TgtEntryPtr,
      (void **)TgtArgs, (ptrdiff_t *)TgtOffsets,
      ArgNum);

  Interface->send(RunTargetRegion, Serializer->I32(Ret));
}

void ServerTy::runTargetTeamRegion(std::string_view Message) {
  auto [DeviceId, TgtEntryPtr, TgtArgs, TgtOffsets, ArgNum, TeamNum, ThreadLimit, LoopTripCount] = Serializer->TargetTeamRegion(Message);
  auto Ret = 0;

  Ret = PM->Devices[DeviceId]->RTL->run_team_region(
      mapHostRTLDeviceId(DeviceId), (void *)TgtEntryPtr,
      (void **)TgtArgs, (ptrdiff_t *)TgtOffsets, ArgNum,
      TeamNum, ThreadLimit, LoopTripCount);

  Interface->send(RunTargetTeamRegion, Serializer->I32(Ret));
}

void ServerTy::dataDelete(std::string_view Message) {
  auto [DeviceId, TgtPtr] = Serializer->DataDelete(Message);

  auto Ret = PM->Devices[DeviceId]->RTL->data_delete(
      mapHostRTLDeviceId(DeviceId), (void *)TgtPtr);

  Interface->send(DataDelete, Serializer->I32(Ret));
}

void ServerTy::unregisterLib(std::string_view Message) {
  PM->RTLs.UnregisterLib(TBD);

  Interface->send(UnregisterLib, Serializer->EmptyMessage());
}

int32_t ServerTy::mapHostRTLDeviceId(int32_t RTLDeviceID) {
  for (auto &RTL : PM->RTLs.UsedRTLs) {
    if (RTLDeviceID - RTL->NumberOfDevices >= 0)
      RTLDeviceID -= RTL->NumberOfDevices;
    else
      break;
  }
  return RTLDeviceID;
}

} // namespace transport::ucx
