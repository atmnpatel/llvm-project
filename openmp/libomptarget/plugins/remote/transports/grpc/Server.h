//===-------------------------- Server.h - Server -------------------------===//
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

#ifndef LLVM_OPENMP_LIBOMPTARGET_PLUGINS_REMOTE_SERVER_SERVER_H
#define LLVM_OPENMP_LIBOMPTARGET_PLUGINS_REMOTE_SERVER_SERVER_H

#include <grpcpp/server_context.h>

#include "Utils.h"
#include "device.h"
#include "omptarget.h"
#include "rtl.h"
#include "grpc.grpc.pb.h"
#include "messages.pb.h"

using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::Status;

extern PluginManager *PM;

namespace transport::grpc {

class RemoteOffloadImpl final : public RemoteOffload::Service {
private:
  int32_t mapHostRTLDeviceId(int32_t RTLDeviceID);

  std::unordered_map<const void *, __tgt_device_image *>
      HostToRemoteDeviceImage;
  std::unordered_map<const void *, __tgt_bin_desc *> Descriptions;
  __tgt_target_table *Table = nullptr;

  int DebugLevel;
  uint64_t MaxSize;
  uint64_t BlockSize;
  std::unique_ptr<google::protobuf::Arena> Arena;

public:
  RemoteOffloadImpl(uint64_t MaxSize, uint64_t BlockSize)
      : MaxSize(MaxSize), BlockSize(BlockSize) {
    PM->RTLs.BlocklistedRTLs = {"libomptarget.rtl.ucx.so",
                                "libomptarget.rtl.rpc.so"};
    DebugLevel = getDebugLevel();
    Arena = std::make_unique<google::protobuf::Arena>();
  }

  Status RegisterLib(ServerContext *Context,
                     const transport::messages::TargetBinaryDescription *Description,
                     transport::messages::I32 *Reply) override;
  Status UnregisterLib(ServerContext *Context, const transport::messages::Pointer *Request,
                       transport::messages::I32 *Reply) override;

  Status IsValidBinary(ServerContext *Context, const transport::messages::Pointer *Image,
                       transport::messages::I32 *IsValid) override;
  Status GetNumberOfDevices(ServerContext *Context, const transport::messages::Null *Null,
                            transport::messages::I32 *NumberOfDevices) override;

  Status InitDevice(ServerContext *Context, const transport::messages::I32 *DeviceNum,
                    transport::messages::I32 *Reply) override;
  Status InitRequires(ServerContext *Context, const transport::messages::I64 *RequiresFlag,
                      transport::messages::I64 *Reply) override;

  Status LoadBinary(ServerContext *Context, const transport::messages::Binary *Binary,
                    transport::messages::TargetTable *Reply) override;

  Status DataAlloc(ServerContext *Context, const transport::messages::AllocData *Request,
                   transport::messages::Pointer *Reply) override;

  Status DataSubmit(ServerContext *Context, ServerReader<transport::messages::SSubmitData> *Reader,
                    transport::messages::I32 *Reply) override;
  Status DataRetrieve(ServerContext *Context, const transport::messages::RetrieveData *Request,
                      ServerWriter<transport::messages::SData> *Writer) override;

  Status DataDelete(ServerContext *Context, const transport::messages::DeleteData *Request,
                    transport::messages::I32 *Reply) override;

  Status RunTargetRegion(ServerContext *Context, const transport::messages::TargetRegion *Request,
                         transport::messages::I32 *Reply) override;

  Status RunTargetTeamRegion(ServerContext *Context,
                             const transport::messages::TargetTeamRegion *Request,
                             transport::messages::I32 *Reply) override;

  Status Shutdown(ServerContext *Context, const transport::messages::Null *Request,
                  transport::messages::Null *Reply) override;
};

} // namespace transport::grpc

#endif
