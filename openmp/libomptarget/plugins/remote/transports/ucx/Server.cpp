#include "Server.h"
#include "Base.h"
#include "Debug.h"
#include "Utils.h"
#include "omptarget.h"
#include "ucp/api/ucp.h"
#include "ucx.pb.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>

namespace transport::ucx {

Server::Server()
    : Base(), ConnectionWorker((ucp_context_h *)Context),
      DebugLevel(getDebugLevel()), TBD(new __tgt_bin_desc) {
  PM->RTLs.BlocklistedRTLs = {"libomptarget.rtl.rpc.so"};

  std::call_once(PM->RTLs.initFlag, &RTLsTy::LoadRTLs, &PM->RTLs);

  Devices = 0;
  PM->RTLsMtx.lock();
  for (auto &RTL : PM->RTLs.AllRTLs)
    Devices += RTL.NumberOfDevices;
  PM->RTLsMtx.unlock();
}

Server::~Server() {
  if (Thread->joinable())
    Thread->join();
}

Server::ListenerTy::ListenerTy(ucp_worker_h Worker, ServerContextTy *Context,
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

void Server::ListenerTy::query() {
  ucp_listener_attr_t Attr = {.field_mask = UCP_LISTENER_ATTR_FIELD_SOCKADDR};
  auto Status = ucp_listener_query(*Listener, &Attr);
  if (Status != UCS_OK)
    ERR("failed to query the listener ({0})\n", ucs_status_string(Status))

  DP("Server is listening on IP %s port %s\n", getIP(&Attr.sockaddr).c_str(),
     getPort(&Attr.sockaddr).c_str());
}

void ProtobufServer::listenForConnections(const ConnectionConfigTy &Config) {
  ServerContextTy Ctx;
  ListenerTy Listener((ucp_worker_h)ConnectionWorker, &Ctx, Config);

  Listener.query();

  while (true) {
    while (Ctx.ConnRequests.empty()) {
      ucp_worker_progress((ucp_worker_h)ConnectionWorker);
    }

    for (auto *ConnRequest : Ctx.ConnRequests) {
      Interface = std::make_unique<InterfaceTy>(Context, ConnRequest);
      Thread = new std::thread([&]() { run(); });
    }

    Ctx.ConnRequests.clear();
  }
}

void ProtobufServer::run() {
  while (true) {
    auto [Type, Message] = Interface->receive();

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
    case DataDelete: {
      dataDelete(Message);
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
    case UnregisterLib: {
      unregisterLib(Message);
      break;
    }
    default: {
      llvm_unreachable("Unimplemented Message Type");
    }
    }
  }
}

void ProtobufServer::getNumberOfDevices() {
  I32 Response;
  Response.set_number(Devices);

  SERVER_DBG("Found %d devices", Devices)
  Interface->send(Count, Response.SerializeAsString());
}

void ProtobufServer::registerLib(std::string &Message) {
  auto Description = deserialize<TargetBinaryDescription>(Message);

  if (TBD)
    delete TBD.get();
  TBD = std::make_unique<__tgt_bin_desc>();
  unloadTargetBinaryDescription(&Description, TBD.get(),
                                HostToRemoteDeviceImage);

  PM->RTLs.RegisterLib(TBD.get());

  I32 Response;
  Response.set_number(0);

  Interface->send(Count, Response.SerializeAsString());
  SERVER_DBG("Registered library")
}

void ProtobufServer::isValidBinary(std::string &Message) {
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

  Interface->send(Count, Response.SerializeAsString());

  SERVER_DBG("Checked if binary (%p) is valid",
             (void *)(DeviceImageHostPtr.number()))
}

void ProtobufServer::initRequires(std::string &Message) {
  SERVER_DBG("Initializing requires for devices")
  auto RequiresFlag = deserialize<I64>(Message);

  for (auto &Device : PM->Devices)
    if (Device.RTL->init_requires)
      Device.RTL->init_requires(RequiresFlag.number());

  Interface->send(Count, RequiresFlag.SerializeAsString());

  SERVER_DBG("Initialized requires for devices")
}

void ProtobufServer::initDevice(std::string &Message) {
  auto DeviceId = deserialize<I32>(Message);

  SERVER_DBG("Initializing device %d", DeviceId.number())

  int32_t Err = PM->Devices[DeviceId.number()].RTL->init_device(
      mapHostRTLDeviceId(DeviceId.number()));

  I32 Response;
  Response.set_number(Err);

  Interface->send(Count, Response.SerializeAsString());

  SERVER_DBG("Initialized device %d", DeviceId.number())
}

int32_t Server::mapHostRTLDeviceId(int32_t RTLDeviceID) {
  for (auto &RTL : PM->RTLs.UsedRTLs) {
    if (RTLDeviceID - RTL->NumberOfDevices >= 0)
      RTLDeviceID -= RTL->NumberOfDevices;
    else
      break;
  }
  return RTLDeviceID;
}

void ProtobufServer::loadBinary(std::string &Message) {
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
    loadTargetTable(TT, Response, Image);
  } else
    ERR("Could not load binary");

  Interface->send(Count, Response.SerializeAsString());
}

void ProtobufServer::dataAlloc(std::string &Message) {
  auto Request = deserialize<AllocData>(Message);

  SERVER_DBG("Allocating %ld bytes on device %d", Request.size(),
             Request.device_id())

  uint64_t TgtPtr = (uint64_t)PM->Devices[Request.device_id()].RTL->data_alloc(
      mapHostRTLDeviceId(Request.device_id()), Request.size(),
      (void *)Request.hst_ptr(), TARGET_ALLOC_DEFAULT);

  Pointer Response;
  Response.set_number(TgtPtr);

  Interface->send(Count, Response.SerializeAsString());

  SERVER_DBG("Allocated at " DPxMOD "", DPxPTR((void *)TgtPtr))
}

void ProtobufServer::dataDelete(std::string &Message) {
  auto Request = deserialize<DeleteData>(Message);

  SERVER_DBG("Deleting data from (%p) on device %d", (void *)Request.tgt_ptr(),
             mapHostRTLDeviceId(Request.device_id()))

  int32_t Err = PM->Devices[Request.device_id()].RTL->data_delete(
      mapHostRTLDeviceId(Request.device_id()), (void *)Request.tgt_ptr());

  I32 Response;
  Response.set_number(Err);

  Interface->send(Count, Response.SerializeAsString());

  SERVER_DBG("Deleted data from (%p) on device %d", (void *)Request.tgt_ptr(),
             mapHostRTLDeviceId(Request.device_id()))
}

void ProtobufServer::dataSubmit(std::string &Message) {
  auto Request = deserialize<SubmitData>(Message);

  SERVER_DBG("Submitting %lu bytes async to (%p) on device %d",
             Request.data().size(), (void *)Request.tgt_ptr(),
             Request.device_id())

  int32_t Err = PM->Devices[Request.device_id()].RTL->data_submit(
      mapHostRTLDeviceId(Request.device_id()), (void *)Request.tgt_ptr(),
      (void *)Request.data().data(), Request.data().size());

  I32 Response;
  Response.set_number(Err);

  Interface->send(Count, Response.SerializeAsString());

  SERVER_DBG("Submitted %lu bytes async to (%p) on device %d",
             Request.data().size(), (void *)Request.tgt_ptr(),
             Request.device_id())
}

void ProtobufServer::runTargetRegion(std::string &Message) {
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

  Interface->send(Count, Response.SerializeAsString());
}

void ProtobufServer::dataRetrieve(std::string &Message) {
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

  Interface->send(Count, Response.SerializeAsString());

  SERVER_DBG("Retrieved %lu bytes from (%p) on device %d", Request.size(),
             (void *)Request.tgt_ptr(), mapHostRTLDeviceId(Request.device_id()))
}

void ProtobufServer::runTargetTeamRegion(std::string &Message) {
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

  Interface->send(Count, Response.SerializeAsString());
}

void ProtobufServer::unregisterLib(std::string &Message) {
  SERVER_DBG("Unregistering library")
  auto Request = deserialize<Pointer>(Message);

  // TODO: handle multiple tgt_bin_descs / application if necessary
  PM->RTLs.UnregisterLib(TBD.get());

  I32 Response;
  Response.set_number(0);

  Interface->send(Count, Response.SerializeAsString());

  SERVER_DBG("Unregistered library")
}

void CustomServer::listenForConnections(const ConnectionConfigTy &Config) {
  ServerContextTy Ctx;
  ListenerTy Listener((ucp_worker_h)ConnectionWorker, &Ctx, Config);

  Listener.query(); // For Info only

  while (true) {
    while (Ctx.ConnRequests.size() == 0) {
      ucp_worker_progress((ucp_worker_h)ConnectionWorker);
    }

    for (auto *ConnRequest : Ctx.ConnRequests) {
      Interface = std::make_unique<InterfaceTy>(Context, ConnRequest);
      Thread = new std::thread([&]() { run(); });
    }

    Ctx.ConnRequests.clear();
  }
}

std::vector<std::string> MessageKindToString = {
    "RegisterLib",         "UnregisterLib", "IsValidBinary",
    "GetNumberOfDevices",  "InitDevice",    "InitRequires",
    "LoadBinary",          "DataAlloc",     "DataDelete",
    "DataSubmit",          "DataRetrieve",  "RunTargetRegion",
    "RunTargetTeamRegion", "Count"};

void CustomServer::run() {
  while (true) {
    auto [Type, Message] = Interface->receive();
    printf("Type: %s\n", MessageKindToString[Type].c_str());

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
      break;
    }
    default: {
      llvm_unreachable("Unimplemented Message Type");
    }
    }
  }
}

void CustomServer::getNumberOfDevices() {
  custom::I32 Response(Devices);

  SERVER_DBG("Found %d devices", Devices)
  Interface->send(GetNumberOfDevices, Response.getBuffer());
}

void CustomServer::registerLib(std::string &Message) {
  custom::TargetBinaryDescription Request(Message, TBD.get(),
                                          HostToRemoteDeviceImage);

  PM->RTLs.RegisterLib(TBD.get());

  Interface->send(RegisterLib, std::string("0"));
}

void CustomServer::isValidBinary(std::string &Message) {
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

  Interface->send(IsValidBinary, Response.getBuffer());
}

void CustomServer::initRequires(std::string &Message) {
  custom::I64 Request(Message);

  for (auto &Device : PM->Devices)
    if (Device.RTL->init_requires)
      Device.RTL->init_requires(Request.Value);

  Interface->send(InitRequires, Request.getBuffer());
}

void CustomServer::initDevice(std::string &Message) {
  custom::I32 Request(Message);

  custom::I32 Response(PM->Devices[Request.Value].RTL->init_device(
      mapHostRTLDeviceId(Request.Value)));

  Interface->send(InitDevice, Response.getBuffer());
}

void CustomServer::loadBinary(std::string &Message) {
  custom::Binary Request(Message);

  __tgt_device_image *Image = HostToRemoteDeviceImage[(void *)Request.Image];

  auto *TT = PM->Devices[Request.DeviceId].RTL->load_binary(
      mapHostRTLDeviceId(Request.DeviceId), Image);

  if (TT) {
    custom::TargetTable Response(TT);
    Interface->send(LoadBinary, Response.getBuffer());
  } else {
    ERR("Could not load binary");
  }
}

void CustomServer::dataAlloc(std::string &Message) {
  custom::DataAlloc Request(Message);

  auto TgtPtr = (uint64_t)PM->Devices[Request.DeviceId].RTL->data_alloc(
      mapHostRTLDeviceId(Request.DeviceId), Request.AllocSize,
      (void *)Request.HstPtr, TARGET_ALLOC_DEFAULT);

  custom::Pointer Response((uintptr_t)TgtPtr);
  Interface->send(DataAlloc, Response.getBuffer());
}

void CustomServer::dataSubmit(std::string &Message) {
  custom::DataSubmit Request(Message);

  custom::I32 Response(PM->Devices[Request.DeviceId].RTL->data_submit(
      mapHostRTLDeviceId(Request.DeviceId), Request.TgtPtr, Request.HstPtr,
      Request.DataSize));

  Interface->send(DataSubmit, Response.getBuffer());
}

void CustomServer::dataRetrieve(std::string &Message) {
  custom::DataRetrieve Request(Message);

  auto HstPtr = std::make_unique<char[]>(Request.DataSize);

  int32_t Value = PM->Devices[Request.DeviceId].RTL->data_retrieve(
      mapHostRTLDeviceId(Request.DeviceId), (void *)HstPtr.get(),
      (void *)Request.TgtPtr, Request.DataSize);

  custom::Data Response(Value, HstPtr.get(), Request.DataSize);

  Interface->send(DataRetrieve, Response.getBuffer());
}

void CustomServer::runTargetRegion(std::string &Message) {
  custom::TargetRegion Request(Message);

  custom::I32 Response(PM->Devices[Request.DeviceId].RTL->run_region(
      mapHostRTLDeviceId(Request.DeviceId), (void *)Request.TgtEntryPtr,
      (void **)Request.TgtArgs, (ptrdiff_t *)Request.TgtOffsets,
      Request.ArgNum));

  Interface->send(RunTargetRegion, Response.getBuffer());
}

void CustomServer::runTargetTeamRegion(std::string &Message) {
  custom::TargetTeamRegion Request(Message);

  custom::I32 Response(PM->Devices[Request.DeviceId].RTL->run_team_region(
      mapHostRTLDeviceId(Request.DeviceId), (void *)Request.TgtEntryPtr,
      (void **)Request.TgtArgs, (ptrdiff_t *)Request.TgtOffsets, Request.ArgNum,
      Request.TeamNum, Request.ThreadLimit, Request.LoopTripCount));

  Interface->send(RunTargetTeamRegion, Response.getBuffer());
}

void CustomServer::dataDelete(std::string &Message) {
  custom::DataDelete Request(Message);

  custom::I32 Response(PM->Devices[Request.DeviceId].RTL->data_delete(
      mapHostRTLDeviceId(Request.DeviceId), (void *)Request.TgtPtr));

  Interface->send(DataDelete, Response.getBuffer());
}

void CustomServer::unregisterLib(std::string &Message) {
  custom::Pointer Request(Message);

  PM->RTLs.UnregisterLib(TBD.get());

  // TODO: handle multiple tgt_bin_descs / application if necessary
  Interface->send(UnregisterLib, std::string("0"));
}
} // namespace transport::ucx
