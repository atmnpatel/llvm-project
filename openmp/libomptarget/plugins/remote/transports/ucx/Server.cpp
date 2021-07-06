#include "Server.h"
#include "Base.h"
#include "Debug.h"
#include "Utils.h"
#include "omptarget.h"
#include "ucp/api/ucp.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>

namespace transport {
namespace ucx {

Server::Server()
    : Base(), ConnectionWorker(Context), DebugLevel(getDebugLevel()),
      TBD(new __tgt_bin_desc) {
  PM->RTLs.BlocklistedRTLs = {
      "libomptarget.rtl.rpc.so"};
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
    llvm::report_fatal_error(
        llvm::formatv("Failed to listen: {0}", ucs_status_string(Status)).str());
}

void Server::ListenerTy::query() {
  ucp_listener_attr_t Attr = {.field_mask = UCP_LISTENER_ATTR_FIELD_SOCKADDR};
  auto Status = ucp_listener_query(*Listener, &Attr);
  if (Status != UCS_OK)
    llvm::report_fatal_error(
        llvm::formatv("failed to query the listener ({0})\n",
                      ucs_status_string(Status))
            .str());

  DP("Server is listening on IP %s port %s\n", getIP(&Attr.sockaddr).c_str(),
     getPort(&Attr.sockaddr).c_str());
}

void ProtobufServer::listenForConnections(const ConnectionConfigTy &Config) {
  ServerContextTy Ctx;
  ListenerTy Listener(ConnectionWorker, &Ctx, Config);

  Listener.query();

  while (1) {
    while (Ctx.ConnRequests.size() == 0) {
      ucp_worker_progress(ConnectionWorker);
    }

    for (auto *ConnRequest : Ctx.ConnRequests) {
      Interface = std::make_unique<InterfaceTy>(Context, &Allocator,
                                                         ConnRequest);
      Thread = new std::thread([&]() { run(); });
    }

    Ctx.ConnRequests.clear();
  }
}

void ProtobufServer::run() {
  while (1) {
    auto Header = Interface->receive();

    switch (Header.type()) {
    case GetNumberOfDevices: {
      getNumberOfDevices();
      break;
    }
    case RegisterLib: {
      registerLib();
      break;
    }
    case IsValidBinary: {
      isValidBinary();
      break;
    }
    case InitRequires: {
      initRequires();
      break;
    }
    case InitDevice: {
      initDevice();
      break;
    }
    case LoadBinary: {
      loadBinary();
      break;
    }
    case DataAlloc: {
      dataAlloc();
      break;
    }
    case DataDelete: {
      dataDelete();
      break;
    }
    case DataSubmit: {
      dataSubmit();
      break;
    }
    case DataRetrieve: {
      dataRetrieve();
      break;
    }
    case RunTargetRegion: {
      runTargetRegion();
      break;
    }
    case RunTargetTeamRegion: {
      runTargetTeamRegion();
      break;
    }
    case UnregisterLib: {
      unregisterLib();
      break;
    }
    default: {
      llvm_unreachable("Unimplemented Message Type");
    }
    }
  }
}

void ProtobufServer::getNumberOfDevices() {
  std::call_once(PM->RTLs.initFlag, &RTLsTy::LoadRTLs, &PM->RTLs);

  int32_t Devices = 0;
  PM->RTLsMtx.lock();
  for (auto &RTL : PM->RTLs.AllRTLs)
    Devices += RTL.NumberOfDevices;
  PM->RTLsMtx.unlock();

  I32 NumDevices;
  NumDevices.set_number(Devices);

  std::vector<std::string> Messages = {NumDevices.SerializeAsString()};
  Interface->send(Messages);
  SERVER_DBG("Found %d devices", Devices)
}

void ProtobufServer::registerLib() {
  auto Description = deserialize<TargetBinaryDescription>();

  TBD = std::make_unique<__tgt_bin_desc>();
  unloadTargetBinaryDescription(&Description, TBD.get(),
                                HostToRemoteDeviceImage);

  PM->RTLs.RegisterLib(TBD.get());

  I32 Response;
  Response.set_number(0);

  std::vector<std::string> Messages = {Response.SerializeAsString()};
  Interface->send(Messages);
  SERVER_DBG("Registered library")
}

void ProtobufServer::isValidBinary() {
  auto Message = Interface->receiveMessage(false);
  Pointer DeviceImageHostPtr;
  DeviceImageHostPtr.ParseFromString(Message);

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

  std::vector<std::string> Messages = {Response.SerializeAsString()};
  Interface->send(Messages);

  SERVER_DBG("Checked if binary (%p) is valid",
             (void *)(DeviceImageHostPtr.number()))
}

void ProtobufServer::initRequires() {
  SERVER_DBG("Initializing requires for devices")
  auto Message = Interface->receiveMessage(false);
  I64 RequiresFlag;
  RequiresFlag.ParseFromString(Message);

  for (auto &Device : PM->Devices)
    if (Device.RTL->init_requires)
      Device.RTL->init_requires(RequiresFlag.number());

  std::vector<std::string> Messages = {Message};
  Interface->send(Messages);

  SERVER_DBG("Initialized requires for devices")
}

void ProtobufServer::initDevice() {
  auto Message = Interface->receiveMessage(false);
  I32 DeviceId;
  DeviceId.ParseFromString(Message);

  SERVER_DBG("Initializing device %d", DeviceId.number())

  int32_t Err = PM->Devices[DeviceId.number()].RTL->init_device(
      mapHostRTLDeviceId(DeviceId.number()));

  I32 Response;
  Response.set_number(Err);

  std::vector<std::string> Messages = {Response.SerializeAsString()};
  Interface->send(Messages);

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

void ProtobufServer::loadBinary() {
  auto Message = Interface->receiveMessage(false);
  Binary Request;
  Request.ParseFromString(Message);

  __tgt_device_image *Image =
      HostToRemoteDeviceImage[(void *)Request.image_ptr()];

  SERVER_DBG("Loading binary (%p) to device %d", (void *)Request.image_ptr(),
             Request.device_id())

  auto *TT = PM->Devices[Request.device_id()].RTL->load_binary(
      mapHostRTLDeviceId(Request.device_id()), Image);

  TargetTable Table;
  if (TT) {
    SERVER_DBG("Loaded binary (%p) to device %d", (void *)Request.image_ptr(),
               Request.device_id())
    loadTargetTable(TT, Table, Image);
  } else
    llvm::report_fatal_error("Could not load binary");

  std::vector<std::string> Messages = {Table.SerializeAsString()};
  Interface->send(Messages);
}

void ProtobufServer::dataAlloc() {
  auto Message = Interface->receiveMessage(false);
  AllocData Request;
  Request.ParseFromString(Message);

  SERVER_DBG("Allocating %ld bytes on sevice %d", Request.size(),
             Request.device_id())

  uint64_t TgtPtr = (uint64_t)PM->Devices[Request.device_id()].RTL->data_alloc(
      mapHostRTLDeviceId(Request.device_id()), Request.size(),
      (void *)Request.hst_ptr(), TARGET_ALLOC_DEFAULT);

  Pointer Response;
  Response.set_number(TgtPtr);

  std::vector<std::string> Messages = {Response.SerializeAsString()};
  Interface->send(Messages);

  SERVER_DBG("Allocated at " DPxMOD "", DPxPTR((void *)TgtPtr))
}

void ProtobufServer::dataDelete() {
  auto Message = Interface->receiveMessage(false);
  DeleteData Request;
  Request.ParseFromString(Message);

  SERVER_DBG("Deleting data from (%p) on device %d", (void *)Request.tgt_ptr(),
             mapHostRTLDeviceId(Request.device_id()))

  int32_t Err = PM->Devices[Request.device_id()].RTL->data_delete(
      mapHostRTLDeviceId(Request.device_id()), (void *)Request.tgt_ptr());

  I32 Response;
  Response.set_number(Err);

  std::vector<std::string> Messages = {Response.SerializeAsString()};
  Interface->send(Messages);

  SERVER_DBG("Deleted data from (%p) on device %d", (void *)Request.tgt_ptr(),
             mapHostRTLDeviceId(Request.device_id()))
}

void ProtobufServer::dataSubmit() {
  auto Message = Interface->receiveMessage(false);
  SubmitData Request;
  Request.ParseFromString(Message);

  SERVER_DBG("Submitting %lu bytes async to (%p) on device %d",
             Request.data().size(), (void *)Request.tgt_ptr(),
             Request.device_id())

  int32_t Err = PM->Devices[Request.device_id()].RTL->data_submit(
      mapHostRTLDeviceId(Request.device_id()), (void *)Request.tgt_ptr(),
      (void *)Request.data().data(), Request.data().size());

  I32 Response;
  Response.set_number(Err);

  std::vector<std::string> Messages = {Response.SerializeAsString()};
  Interface->send(Messages);

  SERVER_DBG("Submitted %lu bytes async to (%p) on device %d",
             Request.data().size(), (void *)Request.tgt_ptr(),
             Request.device_id())
}

void ProtobufServer::runTargetRegion() {
  auto Message = Interface->receiveMessage(false);
  TargetRegion Request;
  Request.ParseFromString(Message);

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

  std::vector<std::string> Messages = {Response.SerializeAsString()};
  Interface->send(Messages);
}

void ProtobufServer::dataRetrieve() {
  auto Message = Interface->receiveMessage(false);
  RetrieveData Request;
  Request.ParseFromString(Message);

  SERVER_DBG("Retrieve %lu bytes from (%p) on device %d", Request.size(),
             (void *)Request.tgt_ptr(), mapHostRTLDeviceId(Request.device_id()))

  auto HstPtr = std::make_unique<char[]>(Request.size());
  int32_t Ret = PM->Devices[Request.device_id()].RTL->data_retrieve(
      mapHostRTLDeviceId(Request.device_id()), (void *)HstPtr.get(),
      (void *)Request.tgt_ptr(), Request.size());

  Data DataReply;
  DataReply.set_data(HstPtr.get(), Request.size());
  DataReply.set_ret(Ret);

  std::vector<std::string> Messages = {DataReply.SerializeAsString()};
  Interface->send(Messages);

  SERVER_DBG("Retrieved %lu bytes from (%p) on device %d", Request.size(),
             (void *)Request.tgt_ptr(), mapHostRTLDeviceId(Request.device_id()))
}

void ProtobufServer::runTargetTeamRegion() {
  auto Message = Interface->receiveMessage(false);
  TargetTeamRegion Request;
  Request.ParseFromString(Message);

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

  std::vector<std::string> Messages = {Response.SerializeAsString()};
  Interface->send(Messages);
}

void ProtobufServer::unregisterLib() {
  SERVER_DBG("Unregistering library")
  auto Message = Interface->receiveMessage(false);
  Pointer Request;
  Request.ParseFromString(Message);

  // TODO: handle multiple tgt_bin_descs / application if necessary
  PM->RTLs.UnregisterLib(TBD.get());

  I32 Response;
  Response.set_number(0);

  std::vector<std::string> Messages = {Response.SerializeAsString()};
  Interface->send(Messages);

  SERVER_DBG("Unregistered library")
}

void SelfSerializationServer::listenForConnections(
    const ConnectionConfigTy &Config) {
  ServerContextTy Ctx;
  ListenerTy Listener(ConnectionWorker, &Ctx, Config);

  Listener.query(); // For Info only

  while (1) {
    while (Ctx.ConnRequests.size() == 0) {
      ucp_worker_progress(ConnectionWorker);
    }

    for (auto *ConnRequest : Ctx.ConnRequests) {
      Interface = std::make_unique<InterfaceTy>(Context, &Allocator,
                                                         ConnRequest);
      Thread = new std::thread([&]() { run(); });
    }

    Ctx.ConnRequests.clear();
  }
}

void SelfSerializationServer::run() {
  while (1) {
    Decoder D(&Allocator);

    Interface->receive(D);

    switch (D.Type) {
    case GetNumberOfDevices: {
      getNumberOfDevices();
      break;
    }
    case RegisterLib: {
      registerLib(D);
      break;
    }
    case IsValidBinary: {
      isValidBinary(D);
      break;
    }
    case InitRequires: {
      initRequires(D);
      break;
    }
    case InitDevice: {
      initDevice(D);
      break;
    }
    case LoadBinary: {
      loadBinary(D);
      break;
    }
    case DataAlloc: {
      dataAlloc(D);
      break;
    }
    case DataSubmit: {
      dataSubmit(D);
      break;
    }
    case DataRetrieve: {
      dataRetrieve(D);
      break;
    }
    case RunTargetRegion: {
      runTargetRegion(D);
      break;
    }
    case RunTargetTeamRegion: {
      runTargetTeamRegion(D);
      break;
    }
    case DataDelete: {
      dataDelete(D);
      break;
    }
    case UnregisterLib: {
      unregisterLib(D);
      break;
    }
    default: {
      llvm_unreachable("Unimplemented Message Type");
    }
    }
  }
}

void SelfSerializationServer::getNumberOfDevices() {
  std::call_once(PM->RTLs.initFlag, &RTLsTy::LoadRTLs, &PM->RTLs);

  int32_t Devices = 0;
  PM->RTLsMtx.lock();
  for (auto &RTL : PM->RTLs.AllRTLs)
    Devices += RTL.NumberOfDevices;
  PM->RTLsMtx.unlock();

  EncoderTy E(&Allocator, MessageTy::GetNumberOfDevices, Devices);
  Interface->send(E.MessageSlabs);
}

void SelfSerializationServer::registerLib(Decoder &D) {
  deserialize(D, TBD.get());
  D.initializeIndirect();

  PM->RTLs.RegisterLib(TBD.get());

  // TODO: handle multiple tgt_bin_descs / application if necessary
  EncoderTy E(&Allocator, MessageTy::RegisterLib);
  Interface->send(E.MessageSlabs);
}

void SelfSerializationServer::isValidBinary(Decoder &D) {

  uintptr_t DeviceImageHostPtr;
  D.deserializeValue(DeviceImageHostPtr);
  __tgt_device_image *DeviceImage =
      HostToRemoteDeviceImage[(void *)DeviceImageHostPtr];

  int32_t IsValid = false;
  for (auto &RTL : PM->RTLs.AllRTLs)
    if (RTL.is_valid_binary(DeviceImage)) {
      IsValid = true;
      break;
    }

  EncoderTy E(&Allocator, MessageTy::IsValidBinary, IsValid);

  Interface->send(E.MessageSlabs);
}

void SelfSerializationServer::deserialize(Decoder &D, __tgt_bin_desc *TBD) {
  D.deserializeList(TBD->HostEntriesBegin, TBD->HostEntriesEnd);
  D.deserializeValue(TBD->NumDeviceImages);
  auto Ptrs = D.deserializeList(TBD->DeviceImages, TBD->NumDeviceImages, true);
  auto I = 0;
  for (const auto &Ptr : Ptrs) {
    HostToRemoteDeviceImage.insert({Ptr, TBD->DeviceImages + I});
    I++;
  }
}
void SelfSerializationServer::initRequires(Decoder &D) {

  int64_t Flag;
  D.deserializeValue(Flag);

  for (auto &Device : PM->Devices)
    if (Device.RTL->init_requires)
      Device.RTL->init_requires(Flag);

  EncoderTy E(&Allocator, MessageTy::InitRequires, Flag);

  Interface->send(E.MessageSlabs);
}

void SelfSerializationServer::initDevice(Decoder &D) {

  int32_t DeviceNum;
  D.deserializeValue(DeviceNum);

  int32_t Response =
      PM->Devices[DeviceNum].RTL->init_device(mapHostRTLDeviceId(DeviceNum));

  EncoderTy E(&Allocator, MessageTy::InitDevice, Response);
  Interface->send(E.MessageSlabs);
}

void SelfSerializationServer::loadBinary(Decoder &D) {

  int32_t DeviceNum;
  D.deserializeValue(DeviceNum);

  uintptr_t DeviceImageHostPtr;
  D.deserializeValue(DeviceImageHostPtr);

  __tgt_device_image *Image =
      HostToRemoteDeviceImage[(void *)DeviceImageHostPtr];

  auto *TT = PM->Devices[DeviceNum].RTL->load_binary(
      mapHostRTLDeviceId(DeviceNum), Image);

  if (TT) {
    EncoderTy E(&Allocator, MessageTy::LoadBinary, false);
    serializeTT(E, TT);
    E.initializeIndirectMemorySlabs();
    E.finalize();
    Interface->send(E.MessageSlabs);
  } else {
    llvm::report_fatal_error("Could not load binary");
  }
}

void SelfSerializationServer::dataAlloc(Decoder &D) {
  int32_t DeviceNum;
  int64_t Size;
  uintptr_t HstPtr;

  D.deserializeValue(DeviceNum);
  D.deserializeValue(Size);
  D.deserializeValue(HstPtr);

  uint64_t TgtPtr = (uint64_t)PM->Devices[DeviceNum].RTL->data_alloc(
      mapHostRTLDeviceId(DeviceNum), Size, (void *)HstPtr,
      TARGET_ALLOC_DEFAULT);

  EncoderTy E(&Allocator, MessageTy::DataAlloc, TgtPtr);
  Interface->send(E.MessageSlabs);
}

void SelfSerializationServer::dataSubmit(Decoder &D) {
  auto *DSA = (DataSubmitTy *)Allocator.Allocate(sizeof(DataSubmitTy), 4);
  deserialize<DataSubmitTy *>(D, DSA);
  D.initializeIndirect();

  int32_t Response = PM->Devices[DSA->DeviceId].RTL->data_submit(
      mapHostRTLDeviceId(DSA->DeviceId), DSA->TgtPtr, DSA->HstPtr, DSA->Size);

  EncoderTy E(&Allocator, MessageTy::DataSubmit, Response);

  Interface->send(E.MessageSlabs);
}

void SelfSerializationServer::dataRetrieve(Decoder &D) {
  int32_t DeviceId;
  uintptr_t TgtPtr;
  int64_t Size;
  uintptr_t Info;

  D.deserializeValue(DeviceId);
  D.deserializeValue(TgtPtr);
  D.deserializeValue(Size);
  D.deserializeValue(Info);

  // auto HstPtr = std::make_unique<char>(Size);
  auto *HstPtr = Allocator.Allocate(Size, 4);

  int32_t Response = PM->Devices[DeviceId].RTL->data_retrieve(
      mapHostRTLDeviceId(DeviceId), (void *)HstPtr, (void *)TgtPtr, Size);
  // mapHostRTLDeviceId(DeviceId), (void *)HstPtr.get(), (void *)TgtPtr, Size);

  EncoderTy E(&Allocator, MessageTy::DataRetrieve, false);
  E.serializeValue(Response);
  // E.serializeMemory(HstPtr.get(), Size);
  E.serializeMemory(HstPtr, Size);
  E.initializeIndirectMemorySlabs();
  E.finalize();

  Interface->send(E.MessageSlabs);
}

void SelfSerializationServer::runTargetRegion(Decoder &D) {
  int32_t DeviceId, ArgNum;
  uintptr_t TgtEntryPtr;

  D.deserializeValue(DeviceId);
  D.deserializeValue(TgtEntryPtr);
  D.deserializeValue(ArgNum);

  std::vector<uint64_t> TgtArgs(ArgNum);
  for (auto I = 0; I < ArgNum; I++)
    D.deserializeValue((TgtArgs[I]));

  std::vector<ptrdiff_t> TgtOffsets(ArgNum);
  for (auto I = 0; I < ArgNum; I++)
    D.deserializeValue((TgtOffsets[I]));

  int32_t Response = PM->Devices[DeviceId].RTL->run_region(
      mapHostRTLDeviceId(DeviceId), (void *)TgtEntryPtr,
      (void **)TgtArgs.data(), (ptrdiff_t *)TgtOffsets.data(), ArgNum);

  EncoderTy E(&Allocator, MessageTy::RunTargetTeamRegion, Response);
  Interface->send(E.MessageSlabs);
}

void SelfSerializationServer::runTargetTeamRegion(Decoder &D) {
  int32_t DeviceId, ArgNum, TeamNum, ThreadLimit;
  uint64_t LoopTripCount;
  uintptr_t TgtEntryPtr;

  D.deserializeValue(DeviceId);
  D.deserializeValue(TgtEntryPtr);
  D.deserializeValue(ArgNum);

  std::vector<uint64_t> TgtArgs(ArgNum);
  for (auto I = 0; I < ArgNum; I++)
    D.deserializeValue((TgtArgs[I]));

  std::vector<ptrdiff_t> TgtOffsets(ArgNum);

  D.deserializeValue(TeamNum);
  D.deserializeValue(ThreadLimit);
  D.deserializeValue(LoopTripCount);

  int32_t Response = PM->Devices[DeviceId].RTL->run_team_region(
      mapHostRTLDeviceId(DeviceId), (void *)TgtEntryPtr,
      (void **)TgtArgs.data(), (ptrdiff_t *)TgtOffsets.data(), ArgNum, TeamNum,
      ThreadLimit, LoopTripCount);

  EncoderTy E(&Allocator, MessageTy::RunTargetTeamRegion, Response);
  Interface->send(E.MessageSlabs);
}

void SelfSerializationServer::dataDelete(Decoder &D) {
  int32_t DeviceId;
  uintptr_t TgtPtr;

  D.deserializeValue(DeviceId);
  D.deserializeValue(TgtPtr);

  int32_t Response = PM->Devices[DeviceId].RTL->data_delete(
      mapHostRTLDeviceId(DeviceId), (void *)TgtPtr);

  EncoderTy E(&Allocator, MessageTy::DataDelete, Response);
  Interface->send(E.MessageSlabs);
}

void SelfSerializationServer::unregisterLib(Decoder &D) {
  uintptr_t Desc;
  D.deserializeValue(Desc);

  PM->RTLs.UnregisterLib(TBD.get());

  // TODO: handle multiple tgt_bin_descs / application if necessary
  EncoderTy E(&Allocator, MessageTy::UnregisterLib);
  Interface->send(E.MessageSlabs);
}
} // namespace ucx
} // namespace transport
