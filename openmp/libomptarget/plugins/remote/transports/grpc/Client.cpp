//===----------------- Client.cpp - Client Implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// gRPC (Client) for the remote plugin.
//
//===----------------------------------------------------------------------===//

#include <cmath>

#include "Client.h"
#include "omptarget.h"
#include "grpc.pb.h"

using namespace std::chrono;

using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::Status;

namespace transport::grpc {

template <typename Fn1, typename Fn2, typename TReturn>
auto ClientTy::remoteCall(Fn1 Preprocessor, Fn2 Postprocessor,
                          TReturn ErrorValue, bool CanTimeOut) {
  ArenaAllocatorLock.lock();
  if (Arena->SpaceAllocated() >= MaxSize)
    Arena->Reset();
  ArenaAllocatorLock.unlock();

  ClientContext Context;
  if (CanTimeOut) {
    auto Deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(Timeout);
    Context.set_deadline(Deadline);
  }

  Status RPCStatus;
  auto Reply = Preprocessor(RPCStatus, Context);

  if (!RPCStatus.ok()) {
    CLIENT_DBG("%s", RPCStatus.error_message().c_str())
  } else {
    return Postprocessor(Reply);
  }

  CLIENT_DBG("Failed")
  return ErrorValue;
}

int32_t ClientTy::registerLib(__tgt_bin_desc *Desc) {
  return remoteCall(
      /* Preprocessor */
      [&](auto &RPCStatus, auto &Context) {
        auto *Request = protobuf::Arena::CreateMessage<transport::messages::TargetBinaryDescription>(
            Arena.get());
        auto *Reply = protobuf::Arena::CreateMessage<transport::messages::I32>(Arena.get());
        loadTargetBinaryDescription(Desc, *Request);
        Request->set_bin_ptr((uint64_t)Desc);

        RPCStatus = Stub->RegisterLib(&Context, *Request, Reply);
        return Reply;
      },
      /* Postprocessor */
      [&](const auto &Reply) {
        if (Reply->number() == 0) {
          CLIENT_DBG("Registered library")
          return 0;
        }
        return 1;
      },
      /* Error Value */ 1);
}

int32_t ClientTy::unregisterLib(__tgt_bin_desc *Desc) {
  return remoteCall(
      /* Preprocessor */
      [&](auto &RPCStatus, auto &Context) {
        auto *Request = protobuf::Arena::CreateMessage<transport::messages::Pointer>(Arena.get());
        auto *Reply = protobuf::Arena::CreateMessage<transport::messages::I32>(Arena.get());

        Request->set_number((uint64_t)Desc);

        RPCStatus = Stub->UnregisterLib(&Context, *Request, Reply);
        return Reply;
      },
      /* Postprocessor */
      [&](const auto &Reply) {
        if (Reply->number() == 0) {
          CLIENT_DBG("Unregistered library")
          return 0;
        }
        CLIENT_DBG("Failed to unregister library")
        return 1;
      },
      /* Error Value */ 1);
}

int32_t ClientTy::isValidBinary(__tgt_device_image *Image) {
  return remoteCall(
      /* Preprocessor */
      [&](auto &RPCStatus, auto &Context) {
        auto *Request = protobuf::Arena::CreateMessage<transport::messages::Pointer>(Arena.get());
        auto *Reply = protobuf::Arena::CreateMessage<transport::messages::I32>(Arena.get());

        Request->set_number((uint64_t)Image);

        RPCStatus = Stub->IsValidBinary(&Context, *Request, Reply);
        return Reply;
      },
      /* Postprocessor */
      [&](const auto &Reply) {
        if (Reply->number()) {
          CLIENT_DBG("Validated binary")
        } else {
          CLIENT_DBG("Could not validate binary")
        }
        return Reply->number();
      },
      /* Error Value */ 0);
}

int32_t ClientTy::getNumberOfDevices() {
  return remoteCall(
      /* Preprocessor */
      [&](Status &RPCStatus, ClientContext &Context) {
        auto *Request = protobuf::Arena::CreateMessage<transport::messages::Null>(Arena.get());
        auto *Reply = protobuf::Arena::CreateMessage<transport::messages::I32>(Arena.get());

        RPCStatus = Stub->GetNumberOfDevices(&Context, *Request, Reply);

        return Reply;
      },
      /* Postprocessor */
      [&](const auto &Reply) {
        if (Reply->number()) {
          CLIENT_DBG("Found %d devices", Reply->number())
        } else {
          CLIENT_DBG("Could not get the number of devices")
        }
        return Reply->number();
      },
      /*Error Value*/ 0);
}

int32_t ClientTy::initDevice(int32_t DeviceId) {
  return remoteCall(
      /* Preprocessor */
      [&](auto &RPCStatus, auto &Context) {
        auto *Request = protobuf::Arena::CreateMessage<transport::messages::I32>(Arena.get());
        auto *Reply = protobuf::Arena::CreateMessage<transport::messages::I32>(Arena.get());

        Request->set_number(DeviceId);

        RPCStatus = Stub->InitDevice(&Context, *Request, Reply);

        return Reply;
      },
      /* Postprocessor */
      [&](const auto &Reply) {
        if (!Reply->number()) {
          CLIENT_DBG("Initialized device %d", DeviceId)
        } else {
          CLIENT_DBG("Could not initialize device %d", DeviceId)
        }
        return Reply->number();
      },
      /* Error Value */ -1);
}

int64_t ClientTy::initRequires(int64_t RequiresFlags) {
  return remoteCall(
      /* Preprocessor */
      [&](auto &RPCStatus, auto &Context) {
        auto *Request = protobuf::Arena::CreateMessage<transport::messages::I64>(Arena.get());
        auto *Reply = protobuf::Arena::CreateMessage<transport::messages::I64>(Arena.get());
        Request->set_number(RequiresFlags);
        RPCStatus = Stub->InitRequires(&Context, *Request, Reply);
        return Reply;
      },
      /* Postprocessor */
      [&](const auto &Reply) {
        if (Reply->number()) {
          CLIENT_DBG("Initialized requires")
        } else {
          CLIENT_DBG("Could not initialize requires")
        }
        return Reply->number();
      },
      /* Error Value */ (int64_t)0);
}

__tgt_target_table *ClientTy::loadBinary(int32_t DeviceId,
                                         __tgt_device_image *Image) {
  return remoteCall(
      /* Preprocessor */
      [&](auto &RPCStatus, auto &Context) {
        auto *ImageMessage =
            protobuf::Arena::CreateMessage<transport::messages::Binary>(Arena.get());
        auto *Reply = protobuf::Arena::CreateMessage<transport::messages::TargetTable>(Arena.get());
        ImageMessage->set_image_ptr((uint64_t)Image);
        ImageMessage->set_device_id(DeviceId);

        RPCStatus = Stub->LoadBinary(&Context, *ImageMessage, Reply);
        return Reply;
      },
      /* Postprocessor */
      [&](auto &Reply) {
        if (Reply->entries_size() == 0) {
          CLIENT_DBG("Could not load image %p onto device %d", Image, DeviceId)
          return (__tgt_target_table *)nullptr;
        }
        DevicesToTables[DeviceId] = std::make_unique<__tgt_target_table>();
        unloadTargetTable(*Reply, DevicesToTables[DeviceId].get(),
                          RemoteEntries[DeviceId]);

        CLIENT_DBG("Loaded Image %p to device %d with %d entries", Image,
                   DeviceId, Reply->entries_size())

        return DevicesToTables[DeviceId].get();
      },
      /* Error Value */ (__tgt_target_table *)nullptr,
      /* CanTimeOut */ false);
}

void *ClientTy::dataAlloc(int32_t DeviceId, int64_t Size, void *HstPtr) {
  return remoteCall(
      /* Preprocessor */
      [&](auto &RPCStatus, auto &Context) {
        auto *Reply = protobuf::Arena::CreateMessage<transport::messages::Pointer>(Arena.get());
        auto *Request = protobuf::Arena::CreateMessage<transport::messages::AllocData>(Arena.get());
        Request->set_device_id(DeviceId);
        Request->set_size(Size);
        Request->set_hst_ptr((uint64_t)HstPtr);

        RPCStatus = Stub->DataAlloc(&Context, *Request, Reply);
        return Reply;
      },
      /* Postprocessor */
      [&](auto &Reply) {
        if (Reply->number()) {
          CLIENT_DBG("Allocated %ld bytes on device %d at %p", Size, DeviceId,
                     (void *)Reply->number())
        } else {
          CLIENT_DBG("Could not allocate %ld bytes on device %d at %p", Size,
                     DeviceId, (void *)Reply->number())
        }
        return (void *)Reply->number();
      },
      /* Error Value */ (void *)nullptr);
}

int32_t ClientTy::dataSubmit(int32_t DeviceId, void *TgtPtr, void *HstPtr,
                             int64_t Size) {
  return remoteCall(
      /* Preprocessor */
      [&](auto &RPCStatus, auto &Context) {
        auto *Reply = protobuf::Arena::CreateMessage<transport::messages::I32>(Arena.get());
        std::unique_ptr<ClientWriter<transport::messages::SSubmitData>> Writer(
            Stub->DataSubmit(&Context, Reply));

        if (Size > BlockSize) {
          uint64_t Start = 0, End = BlockSize;
          for (auto I = 0; I < std::ceil((float)Size / (float) BlockSize); I++) {
            auto *Request =
                protobuf::Arena::CreateMessage<transport::messages::SSubmitData>(Arena.get());

            Request->set_device_id(DeviceId);
            Request->set_data((char *)HstPtr + Start, End - Start);
            Request->set_tgt_ptr((uint64_t)TgtPtr);
            Request->set_start(Start);
            Request->set_size(End - Start);

            if (!Writer->Write(*Request)) {
              CLIENT_DBG("Broken stream when submitting data")
              Reply->set_number(0);
              return Reply;
            }

            Start += BlockSize;
            End += BlockSize;
            if (End >= Size)
              End = Size;
          }
        } else {
          auto *Request =
              protobuf::Arena::CreateMessage<transport::messages::SSubmitData>(Arena.get());

          Request->set_device_id(DeviceId);
          Request->set_data(HstPtr, Size);
          Request->set_tgt_ptr((uint64_t)TgtPtr);
          Request->set_start(0);
          Request->set_size(Size);

          if (!Writer->Write(*Request)) {
            CLIENT_DBG("Broken stream when submitting data")
            Reply->set_number(0);
            return Reply;
          }
        }

        Writer->WritesDone();
        RPCStatus = Writer->Finish();

        return Reply;
      },
      /* Postprocessor */
      [&](auto &Reply) {
        if (!Reply->number()) {
          CLIENT_DBG(" submitted %ld bytes on device %d at %p", Size, DeviceId,
                     TgtPtr)
        } else {
          CLIENT_DBG("Could not async submit %ld bytes on device %d at %p",
                     Size, DeviceId, TgtPtr)
        }
        return Reply->number();
      },
      /* Error Value */ -1,
      /* CanTimeOut */ false);
}

int32_t ClientTy::dataRetrieve(int32_t DeviceId, void *HstPtr, void *TgtPtr,
                               int64_t Size) {
  return remoteCall(
      /* Preprocessor */
      [&](auto &RPCStatus, auto &Context) {
        auto *Request =
            protobuf::Arena::CreateMessage<transport::messages::RetrieveData>(Arena.get());

        Request->set_device_id(DeviceId);
        Request->set_size(Size);
        Request->set_tgt_ptr((int64_t)TgtPtr);

        auto *Reply = protobuf::Arena::CreateMessage<transport::messages::SData>(Arena.get());
        std::unique_ptr<ClientReader<transport::messages::SData>> Reader(
            Stub->DataRetrieve(&Context, *Request));
        Reader->WaitForInitialMetadata();
        while (Reader->Read(Reply)) {
          if (Reply->ret()) {
            CLIENT_DBG("Could not async retrieve %ld bytes on device %d at %p "
                       "for %p",
                       Size, DeviceId, TgtPtr, HstPtr)
            return Reply;
          }

          if (Reply->start() == 0 && Reply->size() == Reply->data().size()) {
            memcpy(HstPtr, Reply->data().data(), Reply->data().size());

            return Reply;
          }

          memcpy((void *)((char *)HstPtr + Reply->start()),
                 Reply->data().data(), Reply->data().size());
        }
        RPCStatus = Reader->Finish();

        return Reply;
      },
      /* Postprocessor */
      [&](auto &Reply) {
        if (!Reply->ret()) {
          CLIENT_DBG("Retrieved %ld bytes on Device %d", Size, DeviceId)
        } else {
          CLIENT_DBG("Could not async retrieve %ld bytes on Device %d", Size,
                     DeviceId)
        }
        return Reply->ret();
      },
      /* Error Value */ -1,
      /* CanTimeOut */ false);
}

int32_t ClientTy::dataDelete(int32_t DeviceId, void *TgtPtr) {
  return remoteCall(
      /* Preprocessor */
      [&](auto &RPCStatus, auto &Context) {
        auto *Reply = protobuf::Arena::CreateMessage<transport::messages::I32>(Arena.get());
        auto *Request = protobuf::Arena::CreateMessage<transport::messages::DeleteData>(Arena.get());

        Request->set_device_id(DeviceId);
        Request->set_tgt_ptr((uint64_t)TgtPtr);

        RPCStatus = Stub->DataDelete(&Context, *Request, Reply);
        return Reply;
      },
      /* Postprocessor */
      [&](auto &Reply) {
        if (!Reply->number()) {
          CLIENT_DBG("Deleted data at %p on device %d", TgtPtr, DeviceId)
        } else {
          CLIENT_DBG("Could not delete data at %p on device %d", TgtPtr,
                     DeviceId)
        }
        return Reply->number();
      },
      /* Error Value */ -1);
}

int32_t ClientTy::runTargetRegion(int32_t DeviceId, void *TgtEntryPtr,
                                  void **TgtArgs, ptrdiff_t *TgtOffsets,
                                  int32_t ArgNum) {
  return remoteCall(
      /* Preprocessor */
      [&](auto &RPCStatus, auto &Context) {
        auto *Reply = protobuf::Arena::CreateMessage<transport::messages::I32>(Arena.get());
        auto *Request =
            protobuf::Arena::CreateMessage<transport::messages::TargetRegion>(Arena.get());

        Request->set_device_id(DeviceId);

        Request->set_tgt_entry_ptr(
            (uint64_t)RemoteEntries[DeviceId][TgtEntryPtr]);

        char **ArgPtr = (char **)TgtArgs;
        for (auto I = 0; I < ArgNum; I++, ArgPtr++)
          Request->add_tgt_args((uint64_t)*ArgPtr);

        char *OffsetPtr = (char *)TgtOffsets;
        for (auto I = 0; I < ArgNum; I++, OffsetPtr++)
          Request->add_tgt_offsets((uint64_t)*OffsetPtr);

        RPCStatus = Stub->RunTargetRegion(&Context, *Request, Reply);
        return Reply;
      },
      /* Postprocessor */
      [&](auto &Reply) {
        if (!Reply->number()) {
          CLIENT_DBG("Ran target region async on device %d", DeviceId)
        } else {
          CLIENT_DBG("Could not run target region async on device %d", DeviceId)
        }
        return Reply->number();
      },
      /* Error Value */ -1,
      /* CanTimeOut */ false);
}

int32_t ClientTy::runTargetTeamRegion(int32_t DeviceId, void *TgtEntryPtr,
                                      void **TgtArgs, ptrdiff_t *TgtOffsets,
                                      int32_t ArgNum, int32_t TeamNum,
                                      int32_t ThreadLimit,
                                      uint64_t LoopTripcount) {
  return remoteCall(
      /* Preprocessor */
      [&](auto &RPCStatus, auto &Context) {
        auto *Reply = protobuf::Arena::CreateMessage<transport::messages::I32>(Arena.get());
        auto *Request =
            protobuf::Arena::CreateMessage<transport::messages::TargetTeamRegion>(Arena.get());

        Request->set_device_id(DeviceId);

        Request->set_tgt_entry_ptr(
            (uint64_t)RemoteEntries[DeviceId][TgtEntryPtr]);

        char **ArgPtr = (char **)TgtArgs;
        for (auto I = 0; I < ArgNum; I++, ArgPtr++) {
          Request->add_tgt_args((uint64_t)*ArgPtr);
        }

        char *OffsetPtr = (char *)TgtOffsets;
        for (auto I = 0; I < ArgNum; I++, OffsetPtr++)
          Request->add_tgt_offsets((uint64_t)*OffsetPtr);

        Request->set_team_num(TeamNum);
        Request->set_thread_limit(ThreadLimit);
        Request->set_loop_tripcount(LoopTripcount);

        RPCStatus = Stub->RunTargetTeamRegion(&Context, *Request, Reply);
        return Reply;
      },
      /* Postprocessor */
      [&](auto &Reply) {
        if (!Reply->number()) {
          CLIENT_DBG("Ran target team region async on device %d", DeviceId)
        } else {
          CLIENT_DBG("Could not run target team region async on device %d",
                     DeviceId)
        }
        return Reply->number();
      },
      /* Error Value */ -1,
      /* CanTimeOut */ false);
}

void ClientTy::shutdown() {
  ClientContext Context;
  auto *Reply = protobuf::Arena::CreateMessage<transport::messages::Null>(Arena.get());
  auto *Request = protobuf::Arena::CreateMessage<transport::messages::Null>(Arena.get());

  auto RPCStatus = Stub->Shutdown(&Context, *Request, Reply);
}

} // namespace transport::grpc
