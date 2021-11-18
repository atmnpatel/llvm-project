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
}

ServerTy::~ServerTy() {
  if (Thread.joinable())
    Thread.join();
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
      ucp_worker_progress((ucp_worker_h)ConnectionWorker);
    }

    for (auto *ConnRequest : Ctx.ConnRequests) {
      Interface = new InterfaceTy(Context, ConnRequest);
      run();
//      Thread = std::thread([&]() {
//        initializeWorkerThread();
//        run();
//      });
    }

    Ctx.ConnRequests.clear();
  }
}

void ServerTy::run() {
  while (Running) {
    auto [Type, Message] = recv(Interface->LastRecvTag);
    SERVER_DBG("Type: %s", MessageKindToString[Type].c_str());

    Interface->LastRecvTag++;

    if (!Running)
      return;

    size_t InterfaceIdx = 0;

    switch (Type) {
    case GetNumberOfDevices: {
      getNumberOfDevices(InterfaceIdx);
      break;
    }
    case RegisterLib: {
      registerLib(InterfaceIdx, Message);
      break;
    }
    case IsValidBinary: {
      isValidBinary(InterfaceIdx, Message);
      break;
    }
    case InitRequires: {
      initRequires(InterfaceIdx, Message);
      break;
    }
    case InitDevice: {
      initDevice(InterfaceIdx, Message);
      break;
    }
    case LoadBinary: {
      loadBinary(InterfaceIdx, Message);
      break;
    }
    case DataAlloc: {
      dataAlloc(InterfaceIdx, Message);
      break;
    }
    case DataSubmit: {
      dataSubmit(InterfaceIdx, Message);
      break;
    }
    case DataRetrieve: {
      dataRetrieve(InterfaceIdx, Message);
      break;
    }
    case RunTargetRegion: {
      runTargetRegion(InterfaceIdx, Message);
      break;
    }
    case RunTargetTeamRegion: {
      runTargetTeamRegion(InterfaceIdx, Message);
      break;
    }
    case DataDelete: {
      dataDelete(InterfaceIdx, Message);
      break;
    }
    case UnregisterLib: {
      unregisterLib(InterfaceIdx, Message);
      for (auto &Interface : Interfaces) {
        Interface->Worker.Running = false;
        Running = false;
      }
      break;
    }
    default: {
      llvm_unreachable("Unimplemented Message Type");
    }
    }
  }
}

void ServerTy::getNumberOfDevices(size_t InterfaceIdx) {
  PM->RTLsMtx.lock();
  for (auto &RTL : PM->RTLs.AllRTLs)
    Devices += RTL.NumberOfDevices;
  PM->RTLsMtx.unlock();

  SERVER_DBG("Found %d devices", Devices)
  send(GetNumberOfDevices, Serializer->I32(Devices));
}

void ServerTy::registerLib(size_t InterfaceIdx, std::string &Message) {
  TBD = Serializer->TargetBinaryDescription(Message, HostToRemoteDeviceImage);

  PM->RTLs.RegisterLib(TBD);

  SERVER_DBG("Registered target binary description %p", TBD);

  send(RegisterLib, Serializer->EmptyMessage());
}

void ServerTy::isValidBinary(size_t InterfaceIdx, std::string &Message) {
  auto *Value = Serializer->Pointer(Message);

  __tgt_device_image *DeviceImage = HostToRemoteDeviceImage[(void *)Value];

  int32_t IsValid = false;
  for (auto &RTL : PM->RTLs.AllRTLs)
    if (RTL.is_valid_binary(DeviceImage)) {
      IsValid = true;
      break;
    }

  send(IsValidBinary, Serializer->I32(IsValid));
}

void ServerTy::initRequires(size_t InterfaceIdx, std::string &Message) {
  auto Value = Serializer->I64(Message);

  for (auto &Device : PM->Devices)
    if (Device->RTL->init_requires)
      Device->RTL->init_requires(Value);

  send(InitRequires, Serializer->I64(Value));
}

void ServerTy::initDevice(size_t InterfaceIdx, std::string &Message) {
  auto DeviceId = Serializer->I32(Message);

  auto Value =
      PM->Devices[DeviceId]->RTL->init_device(mapHostRTLDeviceId(DeviceId));

  SERVER_DBG("Initialized device %d, Err: %d", DeviceId, Value);

  send(InitDevice, Serializer->I32(Value));
}

void ServerTy::loadBinary(size_t InterfaceIdx, std::string &Message) {
  auto [DeviceId, ImagePtr] = Serializer->Binary(Message);

  __tgt_device_image *Image = HostToRemoteDeviceImage[(void *)ImagePtr];

  auto *TT = PM->Devices[DeviceId]->RTL->load_binary(
      mapHostRTLDeviceId(DeviceId), Image);

  if (TT) {
    send(LoadBinary, Serializer->TargetTable(TT));
  } else {
    ERR("Could not load binary");
  }
}

void ServerTy::dataAlloc(size_t InterfaceIdx, std::string &Message) {
  auto [DeviceId, Size, HstPtr] = Serializer->DataAlloc(Message);

  auto TgtPtr = (uint64_t)PM->Devices[DeviceId]->RTL->data_alloc(
      mapHostRTLDeviceId(DeviceId), Size, (void *)HstPtr, TARGET_ALLOC_DEFAULT);

  send(DataAlloc, Serializer->Pointer(TgtPtr));
}

void ServerTy::dataSubmit(size_t InterfaceIdx, std::string &Message) {
  auto [DeviceId, HstPtr, TgtPtr, Size] = Serializer->DataSubmit(Message);

  auto Value = PM->Devices[DeviceId]->RTL->data_submit(
      mapHostRTLDeviceId(DeviceId), TgtPtr, HstPtr, Size);

  send(DataSubmit, Serializer->I32(Value));
}

void ServerTy::dataRetrieve(size_t InterfaceIdx, std::string &Message) {
  auto [DeviceId, TgtPtr, Size] = Serializer->DataRetrieve(Message);
  auto HstPtr = std::make_unique<char[]>(Size);

  int32_t Value = PM->Devices[DeviceId]->RTL->data_retrieve(
      mapHostRTLDeviceId(DeviceId), (void *)HstPtr.get(), (void *)TgtPtr, Size);

  send(DataRetrieve, Serializer->Data(HstPtr.get(), Size, Value));
}

void ServerTy::runTargetRegion(size_t InterfaceIdx, std::string &Message) {
  auto [DeviceId, TgtEntryPtr, TgtArgs, TgtOffsets, ArgNum] =
      Serializer->TargetRegion(Message);

  auto Value = PM->Devices[DeviceId]->RTL->run_region(
      mapHostRTLDeviceId(DeviceId), (void *)TgtEntryPtr, (void **)TgtArgs,
      (ptrdiff_t *)TgtOffsets, ArgNum);

  send(RunTargetRegion, Serializer->I32(Value));
}

void ServerTy::runTargetTeamRegion(size_t InterfaceIdx, std::string &Message) {
  auto [DeviceId, TgtEntryPtr, TgtArgs, TgtOffsets, ArgNum, TeamNum,
        ThreadLimit, LoopTripCount] = Serializer->TargetTeamRegion(Message);

  auto Value = PM->Devices[DeviceId]->RTL->run_team_region(
      mapHostRTLDeviceId(DeviceId), (void *)TgtEntryPtr, (void **)TgtArgs,
      (ptrdiff_t *)TgtOffsets, ArgNum, TeamNum, ThreadLimit, LoopTripCount);

  send(RunTargetTeamRegion, Serializer->I32(Value));
}

void ServerTy::dataDelete(size_t InterfaceIdx, std::string &Message) {
  auto [DeviceId, TgtPtr] = Serializer->DataDelete(Message);

  auto Value = PM->Devices[DeviceId]->RTL->data_delete(
      mapHostRTLDeviceId(DeviceId), (void *)TgtPtr);

  send(DataDelete, Serializer->I32(Value));
}

void ServerTy::unregisterLib(size_t InterfaceIdx, std::string &Message) {
  PM->RTLs.UnregisterLib(TBD);

  send(UnregisterLib, Serializer->EmptyMessage());
}

int32_t ServerTy::mapHostRTLDeviceId(int32_t RTLDeviceID) { return 0; }
} // namespace transport::ucx
