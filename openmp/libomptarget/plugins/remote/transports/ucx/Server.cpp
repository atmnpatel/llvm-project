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

/*
ProtobufServerTy::ProtobufServerTy(SerializerType Type) : ServerTy(Type) {}
CustomServerTy::CustomServerTy(SerializerType Type) : ServerTy(Type) {}
 */

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

/*
void ProtobufServerTy::getNumberOfDevices(size_t InterfaceIdx) {
  I32 Response;

  PM->RTLsMtx.lock();
  for (auto &RTL : PM->RTLs.AllRTLs)
    Devices += RTL.NumberOfDevices;
  PM->RTLsMtx.unlock();

  Response.set_number(Devices);

  SERVER_DBG("Found %d devices", Devices)
  Interfaces[InterfaceIdx]->send(Count, Response.SerializeAsString());
}

void ProtobufServerTy::registerLib(size_t InterfaceIdx, std::string &Message) {
  auto Description = deserialize<TargetBinaryDescription>(Message);

  unloadTargetBinaryDescription(&Description, TBD, HostToRemoteDeviceImage);

  // dump(TBD);

  PM->RTLs.RegisterLib(TBD);

  I32 Response;
  Response.set_number(0);

  Interfaces[InterfaceIdx]->send(Count, Response.SerializeAsString());
  SERVER_DBG("Registered library")
}

void ProtobufServerTy::isValidBinary(size_t InterfaceIdx,
                                     std::string &Message) {
  auto DeviceImageHostPtr = deserialize<Pointer>(Message);

  SERVER_DBG("Checking if binary (%p) is valid",
             (void *)(DeviceImageHostPtr.number()))

  __tgt_device_image *DeviceImage =
      HostToRemoteDeviceImage[(void *)DeviceImageHostPtr.number()];

  int32_t IsValid = false;
  for (auto &RTL : PM->RTLs.AllRTLs)
    if (RTL.is_valid_binary(DeviceImage)) {
      IsValid = true;
      break;
    }

  I32 Response;
  Response.set_number(IsValid);

  Interfaces[InterfaceIdx]->send(Count, Response.SerializeAsString());

  SERVER_DBG("Checked if binary (%p) is valid",
             (void *)(DeviceImageHostPtr.number()))
}

void ProtobufServerTy::initRequires(size_t InterfaceIdx, std::string &Message) {
  SERVER_DBG("Initializing requires for devices")
  auto RequiresFlag = deserialize<I64>(Message);

  for (auto &Device : PM->Devices)
    if (Device.RTL->init_requires)
      Device.RTL->init_requires(RequiresFlag.number());

  Interfaces[InterfaceIdx]->send(Count, RequiresFlag.SerializeAsString());

  SERVER_DBG("Initialized requires for devices")
}

void ProtobufServerTy::initDevice(size_t InterfaceIdx, std::string &Message) {
  auto DeviceId = deserialize<I32>(Message);

  SERVER_DBG("Initializing device %d", DeviceId.number())

  int32_t Err = PM->Devices[DeviceId.number()].RTL->init_device(
      mapHostRTLDeviceId(DeviceId.number()));

  I32 Response;
  Response.set_number(Err);

  Interfaces[InterfaceIdx]->send(Count, Response.SerializeAsString());

  SERVER_DBG("Initialized device %d, Err: %d", DeviceId.number(),
             Response.number())
}



void ProtobufServerTy::loadBinary(size_t InterfaceIdx, std::string &Message) {
  auto Request = deserialize<Binary>(Message);

  __tgt_device_image *Image =
      HostToRemoteDeviceImage[(void *)Request.image_ptr()];

  SERVER_DBG("Loading binary (%p) to device %d", (void *)Request.image_ptr(),
             Request.device_id())

  auto *TT = PM->Devices[Request.device_id()].RTL->load_binary(
      mapHostRTLDeviceId(Request.device_id()), Image);

  TargetTable Response;
  if (TT) {
    SERVER_DBG("Loaded binary (%p) to device %d", (void *)Request.image_ptr(),
               Request.device_id())
    loadTargetTable(TT, Response);
  } else
    ERR("Could not load binary");

  Interfaces[InterfaceIdx]->send(Count, Response.SerializeAsString());
}

void ProtobufServerTy::dataAlloc(size_t InterfaceIdx, std::string &Message) {
  auto Request = deserialize<AllocData>(Message);

  SERVER_DBG("Allocating %ld bytes on device %d", Request.size(),
             Request.device_id())

  auto TgtPtr = (uint64_t)PM->Devices[Request.device_id()].RTL->data_alloc(
      mapHostRTLDeviceId(Request.device_id()), Request.size(),
      (void *)Request.hst_ptr(), TARGET_ALLOC_DEFAULT);

  Pointer Response;
  Response.set_number(TgtPtr);

  Interfaces[InterfaceIdx]->send(Count, Response.SerializeAsString());

  SERVER_DBG("Allocated at " DPxMOD "", DPxPTR((void *)TgtPtr))
}

void ProtobufServerTy::dataDelete(size_t InterfaceIdx, std::string &Message) {
  auto Request = deserialize<DeleteData>(Message);

  SERVER_DBG("Deleting data from (%p) on device %d", (void *)Request.tgt_ptr(),
             mapHostRTLDeviceId(Request.device_id()))

  int32_t Err = PM->Devices[Request.device_id()].RTL->data_delete(
      mapHostRTLDeviceId(Request.device_id()), (void *)Request.tgt_ptr());

  I32 Response;
  Response.set_number(Err);

  Interfaces[InterfaceIdx]->send(Count, Response.SerializeAsString());

  SERVER_DBG("Deleted data from (%p) on device %d", (void *)Request.tgt_ptr(),
             mapHostRTLDeviceId(Request.device_id()))
}

void ProtobufServerTy::dataSubmit(size_t InterfaceIdx, std::string &Message) {
  auto Request = deserialize<SubmitData>(Message);

  SERVER_DBG("Submitting %lu bytes async to (%p) on device %d",
             Request.data().size(), (void *)Request.tgt_ptr(),
             Request.device_id())

  int32_t Err = PM->Devices[Request.device_id()].RTL->data_submit(
      mapHostRTLDeviceId(Request.device_id()), (void *)Request.tgt_ptr(),
      (void *)Request.data().data(), Request.data().size());

  I32 Response;
  Response.set_number(Err);

  Interfaces[InterfaceIdx]->send(Count, Response.SerializeAsString());

  SERVER_DBG("Submitted %lu bytes async to (%p) on device %d",
             Request.data().size(), (void *)Request.tgt_ptr(),
             Request.device_id())
}

void ProtobufServerTy::runTargetRegion(size_t InterfaceIdx,
                                       std::string &Message) {
  auto Request = deserialize<TargetRegion>(Message);

  std::vector<uint64_t> TgtArgs(Request.tgt_args_size());
  for (auto I = 0; I < Request.tgt_args_size(); I++)
    TgtArgs[I] = (uint64_t)Request.tgt_args()[I];

  std::vector<ptrdiff_t> TgtOffsets(Request.tgt_offsets_size());
  const auto *TgtOffsetItr = Request.tgt_offsets().begin();
  for (auto I = 0; I < Request.tgt_offsets_size(); I++, TgtOffsetItr++)
    TgtOffsets[I] = (ptrdiff_t)*TgtOffsetItr;

  void *TgtEntryPtr = ((__tgt_offload_entry *)Request.tgt_entry_ptr())->addr;

  int32_t Ret = PM->Devices[Request.device_id()].RTL->run_region(
      mapHostRTLDeviceId(Request.device_id()), TgtEntryPtr,
      (void **)TgtArgs.data(), TgtOffsets.data(), Request.tgt_args_size());

  I32 Response;
  Response.set_number(Ret);

  Interfaces[InterfaceIdx]->send(Count, Response.SerializeAsString());
}

void ProtobufServerTy::dataRetrieve(size_t InterfaceIdx, std::string &Message) {
  auto Request = deserialize<RetrieveData>(Message);

  SERVER_DBG("Retrieve %lu bytes from (%p) on device %d", Request.size(),
             (void *)Request.tgt_ptr(), mapHostRTLDeviceId(Request.device_id()))

  auto HstPtr = std::make_unique<char[]>(Request.size());
  int32_t Ret = PM->Devices[Request.device_id()].RTL->data_retrieve(
      mapHostRTLDeviceId(Request.device_id()), (void *)HstPtr.get(),
      (void *)Request.tgt_ptr(), Request.size());

  Data Response;
  Response.set_data(HstPtr.get(), Request.size());
  Response.set_ret(Ret);

  Interfaces[InterfaceIdx]->send(Count, Response.SerializeAsString());

  SERVER_DBG("Retrieved %lu bytes from (%p) on device %d", Request.size(),
             (void *)Request.tgt_ptr(), mapHostRTLDeviceId(Request.device_id()))
}

void ProtobufServerTy::runTargetTeamRegion(size_t InterfaceIdx,
                                           std::string &Message) {
  auto Request = deserialize<TargetTeamRegion>(Message);

  std::vector<uint64_t> TgtArgs(Request.tgt_args_size());
  for (auto I = 0; I < Request.tgt_args_size(); I++)
    TgtArgs[I] = (uint64_t)Request.tgt_args()[I];

  std::vector<ptrdiff_t> TgtOffsets(Request.tgt_offsets_size());
  const auto *TgtOffsetItr = Request.tgt_offsets().begin();
  for (auto I = 0; I < Request.tgt_offsets_size(); I++, TgtOffsetItr++)
    TgtOffsets[I] = (ptrdiff_t)*TgtOffsetItr;

  void *TgtEntryPtr = ((__tgt_offload_entry *)Request.tgt_entry_ptr())->addr;

  int32_t Ret = PM->Devices[Request.device_id()].RTL->run_team_region(
      mapHostRTLDeviceId(Request.device_id()), TgtEntryPtr,
      (void **)TgtArgs.data(), TgtOffsets.data(), Request.tgt_args_size(),
      Request.team_num(), Request.thread_limit(), Request.loop_tripcount());

  I32 Response;
  Response.set_number(Ret);

  Interfaces[InterfaceIdx]->send(Count, Response.SerializeAsString());
}

void ProtobufServerTy::unregisterLib(size_t InterfaceIdx,
                                     std::string &Message) {
  SERVER_DBG("Unregistering library")
  auto Request = deserialize<Pointer>(Message);

  PM->RTLs.UnregisterLib(TBD);

  I32 Response;
  Response.set_number(0);

  Interfaces[InterfaceIdx]->send(Count, Response.SerializeAsString());

  SERVER_DBG("Unregistered library")
}
*/

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
      auto Interface = std::make_unique<InterfaceTy>(Context, ConnRequest);
      Interfaces.push_back(std::move(Interface));
      run();
    }

    Ctx.ConnRequests.clear();
  }
}

void ServerTy::run() {
  auto InterfaceIdx = 0;
  while (Running) {
    auto [Type, Message] = Interfaces[InterfaceIdx]->receive();
    SERVER_DBG("Interface: %ld - Type: %s", InterfaceIdx,
               MessageKindToString[Type].c_str());

    if (!Running)
      return;

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
        Interface->Running = false;
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

/*
void CustomServerTy::getNumberOfDevices(size_t InterfaceIdx) {
  PM->RTLsMtx.lock();
  for (auto &RTL : PM->RTLs.AllRTLs)
    Devices += RTL.NumberOfDevices;
  PM->RTLsMtx.unlock();

  custom::I32 Response(Devices);

  SERVER_DBG("Found %d devices", Devices)
  Interfaces[InterfaceIdx]->send(GetNumberOfDevices, Response.Message);
}

void CustomServerTy::registerLib(size_t InterfaceIdx, std::string &Message) {
  dump(Message.data(), Message.data() + Message.size());
  custom::TargetBinaryDescription Request(Message, TBD,
                                          HostToRemoteDeviceImage);

  PM->RTLs.RegisterLib(TBD);

  Interfaces[InterfaceIdx]->send(RegisterLib, std::string("0"));
}

void CustomServerTy::isValidBinary(size_t InterfaceIdx, std::string &Message) {
  custom::Pointer Request(Message);

  __tgt_device_image *DeviceImage =
      HostToRemoteDeviceImage[(void *)Request.Value];

  int32_t IsValid = false;
  for (auto &RTL : PM->RTLs.AllRTLs)
    if (RTL.is_valid_binary(DeviceImage)) {
      IsValid = true;
      break;
    }

  custom::I32 Response(IsValid);

  Interfaces[InterfaceIdx]->send(IsValidBinary, Response.Message);
}

void CustomServerTy::initRequires(size_t InterfaceIdx, std::string &Message) {
  custom::I64 Request(Message);

  for (auto &Device : PM->Devices)
    if (Device.RTL->init_requires)
      Device.RTL->init_requires(Request.Value);

  Interfaces[InterfaceIdx]->send(InitRequires, Request.Message);
}

void CustomServerTy::initDevice(size_t InterfaceIdx, std::string &Message) {
  custom::I32 Request(Message);

  custom::I32 Response(PM->Devices[Request.Value].RTL->init_device(
      mapHostRTLDeviceId(Request.Value)));

  Interfaces[InterfaceIdx]->send(InitDevice, Response.Message);
  SERVER_DBG("Initialized device %d, Err: %d", Request.Value, Response.Value);
}

void CustomServerTy::loadBinary(size_t InterfaceIdx, std::string &Message) {
  custom::Binary Request(Message);

  __tgt_device_image *Image = HostToRemoteDeviceImage[(void *)Request.Image];

  auto *TT = PM->Devices[Request.DeviceId].RTL->load_binary(
      mapHostRTLDeviceId(Request.DeviceId), Image);

  if (TT) {
    custom::TargetTable Response(TT);
    Interfaces[InterfaceIdx]->send(LoadBinary, Response.Message);
  } else {
    ERR("Could not load binary");
  }
}

void CustomServerTy::dataAlloc(size_t InterfaceIdx, std::string &Message) {
  custom::DataAlloc Request(Message);

  auto TgtPtr = (uint64_t)PM->Devices[Request.DeviceId].RTL->data_alloc(
      mapHostRTLDeviceId(Request.DeviceId), Request.AllocSize,
      (void *)Request.HstPtr, TARGET_ALLOC_DEFAULT);

  custom::Pointer Response((uintptr_t)TgtPtr);
  Interfaces[InterfaceIdx]->send(DataAlloc, Response.Message);
}

void CustomServerTy::dataSubmit(size_t InterfaceIdx, std::string &Message) {
  custom::DataSubmit Request(Message);

  custom::I32 Response(PM->Devices[Request.DeviceId].RTL->data_submit(
      mapHostRTLDeviceId(Request.DeviceId), Request.TgtPtr, Request.HstPtr,
      Request.DataSize));

  Interfaces[InterfaceIdx]->send(DataSubmit, Response.Message);
}

void CustomServerTy::dataRetrieve(size_t InterfaceIdx, std::string &Message) {
  custom::DataRetrieve Request(Message);

  auto HstPtr = std::make_unique<char[]>(Request.DataSize);

  int32_t Value = PM->Devices[Request.DeviceId].RTL->data_retrieve(
      mapHostRTLDeviceId(Request.DeviceId), (void *)HstPtr.get(),
      (void *)Request.TgtPtr, Request.DataSize);

  custom::Data Response(Value, HstPtr.get(), Request.DataSize);

  Interfaces[InterfaceIdx]->send(DataRetrieve, Response.Message);
}

void CustomServerTy::runTargetRegion(size_t InterfaceIdx,
                                     std::string &Message) {
  custom::TargetRegion Request(Message);

  custom::I32 Response(PM->Devices[Request.DeviceId].RTL->run_region(
      mapHostRTLDeviceId(Request.DeviceId), (void *)Request.TgtEntryPtr,
      (void **)Request.TgtArgs, (ptrdiff_t *)Request.TgtOffsets,
      Request.ArgNum));

  Interfaces[InterfaceIdx]->send(RunTargetRegion, Response.Message);
}

void CustomServerTy::runTargetTeamRegion(size_t InterfaceIdx,
                                         std::string &Message) {
  custom::TargetTeamRegion Request(Message);

  custom::I32 Response(PM->Devices[Request.DeviceId].RTL->run_team_region(
      mapHostRTLDeviceId(Request.DeviceId), (void *)Request.TgtEntryPtr,
      (void **)Request.TgtArgs, (ptrdiff_t *)Request.TgtOffsets, Request.ArgNum,
      Request.TeamNum, Request.ThreadLimit, Request.LoopTripCount));

  Interfaces[InterfaceIdx]->send(RunTargetTeamRegion, Response.Message);
}

void CustomServerTy::dataDelete(size_t InterfaceIdx, std::string &Message) {
  custom::DataDelete Request(Message);

  custom::I32 Response(PM->Devices[Request.DeviceId].RTL->data_delete(
      mapHostRTLDeviceId(Request.DeviceId), (void *)Request.TgtPtr));

  Interfaces[InterfaceIdx]->send(DataDelete, Response.Message);
}

void CustomServerTy::unregisterLib(size_t InterfaceIdx, std::string &Message) {
  PM->RTLs.UnregisterLib(TBD);

  Interfaces[InterfaceIdx]->send(UnregisterLib, std::string("0"));
}
*/

////////////////////////////////////////////////////////////////////////////////

void ServerTy::getNumberOfDevices(size_t InterfaceIdx) {
  PM->RTLsMtx.lock();
  for (auto &RTL : PM->RTLs.AllRTLs)
    Devices += RTL.NumberOfDevices;
  PM->RTLsMtx.unlock();

  SERVER_DBG("Found %d devices", Devices)
  Interfaces[InterfaceIdx]->send(GetNumberOfDevices, Serializer->I32(Devices));
}

void ServerTy::registerLib(size_t InterfaceIdx, std::string &Message) {
  TBD = Serializer->TargetBinaryDescription(Message, HostToRemoteDeviceImage);

  PM->RTLs.RegisterLib(TBD);

  Interfaces[InterfaceIdx]->send(RegisterLib, Serializer->EmptyMessage());
}

void ServerTy::isValidBinary(size_t InterfaceIdx, std::string &Message) {
  auto Value = Serializer->Pointer(Message);

  __tgt_device_image *DeviceImage =
      HostToRemoteDeviceImage[(void *)Value];

  int32_t IsValid = false;
  for (auto &RTL : PM->RTLs.AllRTLs)
    if (RTL.is_valid_binary(DeviceImage)) {
      IsValid = true;
      break;
    }

  Interfaces[InterfaceIdx]->send(IsValidBinary, Serializer->I32(IsValid));
}

void ServerTy::initRequires(size_t InterfaceIdx, std::string &Message) {
  auto Value = Serializer->I64(Message);

  for (auto &Device : PM->Devices)
    if (Device->RTL->init_requires)
      Device->RTL->init_requires(Value);

  Interfaces[InterfaceIdx]->send(InitRequires, Serializer->I64(Value));
}

void ServerTy::initDevice(size_t InterfaceIdx, std::string &Message) {
  auto DeviceId = Serializer->I32(Message);

  auto Value = PM->Devices[DeviceId]->RTL->init_device(
      mapHostRTLDeviceId(DeviceId));

  SERVER_DBG("Initialized device %d, Err: %d", DeviceId, Value);

  Interfaces[InterfaceIdx]->send(InitDevice, Serializer->I32(Value));
}

void ServerTy::loadBinary(size_t InterfaceIdx, std::string &Message) {
  auto Request = Serializer->Binary(Message);

  __tgt_device_image *Image = HostToRemoteDeviceImage[(void *)Request.second];

  auto *TT = PM->Devices[Request.first]->RTL->load_binary(
      mapHostRTLDeviceId(Request.first), Image);

  if (TT) {
    Interfaces[InterfaceIdx]->send(LoadBinary, Serializer->TargetTable(TT));
  } else {
    ERR("Could not load binary");
  }
}

void ServerTy::dataAlloc(size_t InterfaceIdx, std::string &Message) {
  auto [DeviceId, AllocSize, HstPtr] = Serializer->DataAlloc(Message);

  auto TgtPtr = (uint64_t)PM->Devices[DeviceId]->RTL->data_alloc(
      mapHostRTLDeviceId(DeviceId), AllocSize,
      (void *)HstPtr, TARGET_ALLOC_DEFAULT);

  Interfaces[InterfaceIdx]->send(DataAlloc, Serializer->Pointer(TgtPtr));
}

void ServerTy::dataSubmit(size_t InterfaceIdx, std::string &Message) {
  auto [DeviceId, TgtPtr, HstPtr, DataSize] = Serializer->DataSubmit(Message);

  printf("%d, %p, %p, %ld\n", mapHostRTLDeviceId(DeviceId), TgtPtr, HstPtr, DataSize);

  auto Ret = PM->Devices[DeviceId]->RTL->data_submit(
      mapHostRTLDeviceId(DeviceId), TgtPtr, HstPtr,
      DataSize);

  Interfaces[InterfaceIdx]->send(DataSubmit, Serializer->I32(Ret));
}

void ServerTy::dataRetrieve(size_t InterfaceIdx, std::string &Message) {
  auto [DeviceId, TgtPtr, DataSize] = Serializer->DataRetrieve(Message);

  auto HstPtr = std::make_unique<char[]>(DataSize);

  int32_t Value = PM->Devices[DeviceId]->RTL->data_retrieve(
      mapHostRTLDeviceId(DeviceId), (void *)HstPtr.get(),
      (void *)TgtPtr, DataSize);

  Interfaces[InterfaceIdx]->send(DataRetrieve, Serializer->Data(HstPtr.get(), DataSize, Value));
}

void ServerTy::runTargetRegion(size_t InterfaceIdx, std::string &Message) {
  auto Request = Serializer->TargetRegion(Message);

  // custom::I32 Response(PM->Devices[Request.DeviceId]->RTL->run_region(
  //     mapHostRTLDeviceId(Request.DeviceId), (void *)Request.TgtEntryPtr,
  //     (void **)Request.TgtArgs, (ptrdiff_t *)Request.TgtOffsets,
  //     Request.ArgNum));

  auto Ret = 0;

  Interfaces[InterfaceIdx]->send(RunTargetRegion, Serializer->I32(Ret));
}

void ServerTy::runTargetTeamRegion(size_t InterfaceIdx, std::string &Message) {
  auto Request = Serializer->TargetTeamRegion(Message);

  // custom::I32 Response(PM->Devices[Request.DeviceId]->RTL->run_team_region(
  //     mapHostRTLDeviceId(Request.DeviceId), (void *)Request.TgtEntryPtr,
  //     (void **)Request.TgtArgs, (ptrdiff_t *)Request.TgtOffsets, Request.ArgNum,
  //     Request.TeamNum, Request.ThreadLimit, Request.LoopTripCount));

  auto Ret = 0;

  Interfaces[InterfaceIdx]->send(RunTargetTeamRegion, Serializer->I32(Ret));
}

void ServerTy::dataDelete(size_t InterfaceIdx, std::string &Message) {
  auto [DeviceId, TgtPtr] = Serializer->DataDelete(Message);

  auto Ret = PM->Devices[DeviceId]->RTL->data_delete(
      mapHostRTLDeviceId(DeviceId), (void *)TgtPtr);

  Interfaces[InterfaceIdx]->send(DataDelete, Serializer->I32(Ret));
}

void ServerTy::unregisterLib(size_t InterfaceIdx, std::string &Message) {
  PM->RTLs.UnregisterLib(TBD);

  Interfaces[InterfaceIdx]->send(UnregisterLib, Serializer->EmptyMessage());
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
