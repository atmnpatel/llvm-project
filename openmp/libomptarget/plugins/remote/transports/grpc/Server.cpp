//===----------------- Server.cpp - Server Implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Offloading gRPC server for remote host.
//
//===----------------------------------------------------------------------===//

#include <cmath>
#include <future>

#include "Server.h"
#include "omptarget.h"

using grpc::WriteOptions;

extern std::promise<void> ShutdownPromise;

namespace transport::grpc {

Status
RemoteOffloadImpl::RegisterLib(ServerContext *Context,
                               const transport::messages::TargetBinaryDescription *Description,
                               transport::messages::I32 *Reply) {
  auto Desc = new __tgt_bin_desc;

  unloadTargetBinaryDescription(Description, Desc, HostToRemoteDeviceImage);
  PM->RTLs.RegisterLib(Desc);

  if (Descriptions.find((void *)Description->bin_ptr()) != Descriptions.end())
    freeTargetBinaryDescription(Descriptions[(void *)Description->bin_ptr()]);
  else
    Descriptions[(void *)Description->bin_ptr()] = Desc;

  SERVER_DBG("Registered library")
  Reply->set_number(0);
  return Status::OK;
}

Status RemoteOffloadImpl::UnregisterLib(ServerContext *Context,
                                        const transport::messages::Pointer *Request, transport::messages::I32 *Reply) {
  if (Descriptions.find((void *)Request->number()) == Descriptions.end()) {
    Reply->set_number(1);
    return Status::OK;
  }

  PM->RTLs.UnregisterLib(Descriptions[(void *)Request->number()]);
  freeTargetBinaryDescription(Descriptions[(void *)Request->number()]);
  Descriptions.erase((void *)Request->number());

  SERVER_DBG("Unregistered library")
  Reply->set_number(0);
  return Status::OK;
}

Status RemoteOffloadImpl::IsValidBinary(ServerContext *Context,
                                        const transport::messages::Pointer *DeviceImage,
                                        transport::messages::I32 *IsValid) {
  __tgt_device_image *Image =
      HostToRemoteDeviceImage[(void *)DeviceImage->number()];

  IsValid->set_number(0);

  for (auto &RTL : PM->RTLs.AllRTLs)
    if (auto Ret = RTL.is_valid_binary(Image)) {
      IsValid->set_number(Ret);
      break;
    }

  SERVER_DBG("Checked if binary (%p) is valid", (void *)(DeviceImage->number()))
  return Status::OK;
}

Status RemoteOffloadImpl::GetNumberOfDevices(ServerContext *Context,
                                             const transport::messages::Null *Null,
                                             transport::messages::I32 *NumberOfDevices) {
  int32_t Devices = 0;

  PM->RTLsMtx.lock();
  for (auto &RTL : PM->RTLs.AllRTLs)
    Devices += RTL.NumberOfDevices;
  PM->RTLsMtx.unlock();

  NumberOfDevices->set_number(Devices);

  SERVER_DBG("Got number of devices")
  return Status::OK;
}

Status RemoteOffloadImpl::InitDevice(ServerContext *Context,
                                     const transport::messages::I32 *DeviceNum, transport::messages::I32 *Reply) {
  Reply->set_number(PM->Devices[DeviceNum->number()]->RTL->init_device(
      mapHostRTLDeviceId(DeviceNum->number())));

  SERVER_DBG("Initialized device %d", DeviceNum->number())
  return Status::OK;
}

Status RemoteOffloadImpl::InitRequires(ServerContext *Context,
                                       const transport::messages::I64 *RequiresFlag, transport::messages::I64 *Reply) {
  for (auto &Device : PM->Devices)
    if (Device->RTL->init_requires)
      Device->RTL->init_requires(RequiresFlag->number());
  Reply->set_number(RequiresFlag->number());

  SERVER_DBG("Initialized requires for devices")
  return Status::OK;
}

Status RemoteOffloadImpl::LoadBinary(ServerContext *Context,
                                     const transport::messages::Binary *Binary, transport::messages::TargetTable *Reply) {
  __tgt_device_image *Image =
      HostToRemoteDeviceImage[(void *)Binary->image_ptr()];

  Table = PM->Devices[Binary->device_id()]->RTL->load_binary(
      mapHostRTLDeviceId(Binary->device_id()), Image);
  if (Table)
    loadTargetTable(Table, *Reply);

  SERVER_DBG("Loaded binary (%p) to device %d", (void *)Binary->image_ptr(),
             Binary->device_id())
  return Status::OK;
}

Status RemoteOffloadImpl::DataAlloc(ServerContext *Context,
                                    const transport::messages::AllocData *Request, transport::messages::Pointer *Reply) {
  uint64_t TgtPtr = (uint64_t)PM->Devices[Request->device_id()]->RTL->data_alloc(
      mapHostRTLDeviceId(Request->device_id()), Request->size(),
      (void *)Request->hst_ptr(), TARGET_ALLOC_DEFAULT);
  Reply->set_number(TgtPtr);

  SERVER_DBG("Allocated at " DPxMOD "", DPxPTR((void *)TgtPtr))

  //  printf("Server: Allocated %ld bytes at %p on device %d\n",
  //  Request->size(),
  //         (void *)TgtPtr, Request->device_id());

  return Status::OK;
}

Status RemoteOffloadImpl::DataSubmit(ServerContext *Context,
                                     ServerReader<transport::messages::SSubmitData> *Reader,
                                     transport::messages::I32 *Reply) {
  transport::messages::SSubmitData Request;
  uint8_t *HostCopy = nullptr;
  while (Reader->Read(&Request)) {
    if (Request.start() == 0 && Request.size() == Request.data().size()) {
      Reader->SendInitialMetadata();

      Reply->set_number(PM->Devices[Request.device_id()]->RTL->data_submit(
          mapHostRTLDeviceId(Request.device_id()), (void *)Request.tgt_ptr(),
          (void *)Request.data().data(), Request.data().size()));

      SERVER_DBG("Submitted %lu bytes async to (%p) on device %d",
                 Request.data().size(), (void *)Request.tgt_ptr(),
                 Request.device_id())

      return Status::OK;
    }
    if (!HostCopy) {
      HostCopy = new uint8_t[Request.size()];
      Reader->SendInitialMetadata();
    }

    memcpy((void *)((char *)HostCopy + Request.start()), Request.data().data(),
           Request.data().size());
  }

  Reply->set_number(PM->Devices[Request.device_id()]->RTL->data_submit(
      mapHostRTLDeviceId(Request.device_id()), (void *)Request.tgt_ptr(),
      HostCopy, Request.size()));

  delete[] HostCopy;

  SERVER_DBG("Submitted %lu bytes to (%p) on device %d", Request.data().size(),
             (void *)Request.tgt_ptr(), Request.device_id())

  return Status::OK;
}

Status RemoteOffloadImpl::DataRetrieve(ServerContext *Context,
                                       const transport::messages::RetrieveData *Request,
                                       ServerWriter<transport::messages::SData> *Writer) {
  auto HstPtr = std::make_unique<char[]>(Request->size());

  auto Ret = PM->Devices[Request->device_id()]->RTL->data_retrieve(
      mapHostRTLDeviceId(Request->device_id()), HstPtr.get(),
      (void *)Request->tgt_ptr(), Request->size());

  if (Arena->SpaceAllocated() >= MaxSize)
    Arena->Reset();

  if (Request->size() > BlockSize) {
    uint64_t Start = 0, End = BlockSize;
    for (auto I = 0; I < ceil((float)Request->size() / BlockSize); I++) {
      auto *Reply = google::protobuf::Arena::CreateMessage<transport::messages::SData>(Arena.get());

      Reply->set_start(Start);
      Reply->set_size(Request->size());
      Reply->set_data((char *)HstPtr.get() + Start, End - Start);
      Reply->set_ret(Ret);

      if (!Writer->Write(*Reply)) {
        CLIENT_DBG("Broken stream when submitting data")
      }

      SERVER_DBG("Retrieved %lu-%lu/%lu bytes from (%p) on device %d", Start,
                 End, Request->size(), (void *)Request->tgt_ptr(),
                 mapHostRTLDeviceId(Request->device_id()))

      Start += BlockSize;
      End += BlockSize;
      if (End >= Request->size())
        End = Request->size();
    }
  } else {
    auto *Reply = google::protobuf::Arena::CreateMessage<transport::messages::SData>(Arena.get());

    Reply->set_start(0);
    Reply->set_size(Request->size());
    Reply->set_data((char *)HstPtr.get(), Request->size());
    Reply->set_ret(Ret);

    SERVER_DBG("Retrieved %lu bytes from (%p) on device %d", Request->size(),
               (void *)Request->tgt_ptr(),
               mapHostRTLDeviceId(Request->device_id()))

    Writer->WriteLast(*Reply, WriteOptions());
  }

  return Status::OK;
}

Status RemoteOffloadImpl::DataDelete(ServerContext *Context,
                                     const transport::messages::DeleteData *Request, transport::messages::I32 *Reply) {
  auto Ret = PM->Devices[Request->device_id()]->RTL->data_delete(
      mapHostRTLDeviceId(Request->device_id()), (void *)Request->tgt_ptr());
  Reply->set_number(Ret);

  SERVER_DBG("Deleted data from (%p) on device %d", (void *)Request->tgt_ptr(),
             mapHostRTLDeviceId(Request->device_id()))
  return Status::OK;
}

Status RemoteOffloadImpl::RunTargetRegion(ServerContext *Context,
                                          const transport::messages::TargetRegion *Request,
                                          transport::messages::I32 *Reply) {
  std::vector<uint64_t> TgtArgs(Request->tgt_args_size());
  for (auto I = 0; I < Request->tgt_args_size(); I++)
    TgtArgs[I] = (uint64_t)Request->tgt_args()[I];

  std::vector<ptrdiff_t> TgtOffsets(Request->tgt_args_size());
  const auto *TgtOffsetItr = Request->tgt_offsets().begin();
  for (auto I = 0; I < Request->tgt_args_size(); I++, TgtOffsetItr++)
    TgtOffsets[I] = (ptrdiff_t)*TgtOffsetItr;

  void *TgtEntryPtr = ((__tgt_offload_entry *)Request->tgt_entry_ptr())->addr;

  int32_t Ret = PM->Devices[Request->device_id()]->RTL->run_region(
      mapHostRTLDeviceId(Request->device_id()), TgtEntryPtr,
      (void **)TgtArgs.data(), TgtOffsets.data(), Request->tgt_args_size());

  Reply->set_number(Ret);

  SERVER_DBG("Ran TargetRegion on device %d with %d args",
             mapHostRTLDeviceId(Request->device_id()), Request->tgt_args_size())
  return Status::OK;
}

Status RemoteOffloadImpl::RunTargetTeamRegion(ServerContext *Context,
                                              const transport::messages::TargetTeamRegion *Request,
                                              transport::messages::I32 *Reply) {
  std::vector<uint64_t> TgtArgs(Request->tgt_args_size());
  for (auto I = 0; I < Request->tgt_args_size(); I++)
    TgtArgs[I] = (uint64_t)Request->tgt_args()[I];

  std::vector<ptrdiff_t> TgtOffsets(Request->tgt_args_size());
  const auto *TgtOffsetItr = Request->tgt_offsets().begin();
  for (auto I = 0; I < Request->tgt_args_size(); I++, TgtOffsetItr++)
    TgtOffsets[I] = (ptrdiff_t)*TgtOffsetItr;

  void *TgtEntryPtr = ((__tgt_offload_entry *)Request->tgt_entry_ptr())->addr;

  int32_t Ret = PM->Devices[Request->device_id()]->RTL->run_team_region(
      mapHostRTLDeviceId(Request->device_id()), TgtEntryPtr,
      (void **)TgtArgs.data(), TgtOffsets.data(), Request->tgt_args_size(),
      Request->team_num(), Request->thread_limit(), Request->loop_tripcount());

  Reply->set_number(Ret);

  SERVER_DBG("Ran TargetTeamRegion on device %d with %d args",
             mapHostRTLDeviceId(Request->device_id()), Request->tgt_args_size())
  return Status::OK;
}

Status RemoteOffloadImpl::Shutdown(ServerContext *Context, const transport::messages::Null *Request,
                                   transport::messages::Null *Reply) {
  ShutdownPromise.set_value();
  return Status::OK;
}

int32_t RemoteOffloadImpl::mapHostRTLDeviceId(int32_t RTLDeviceID) {
  for (auto &RTL : PM->RTLs.UsedRTLs) {
    if (RTLDeviceID - RTL->NumberOfDevices >= 0)
      RTLDeviceID -= RTL->NumberOfDevices;
    else
      break;
  }
  return RTLDeviceID;
}
} // namespace transport::grpc
