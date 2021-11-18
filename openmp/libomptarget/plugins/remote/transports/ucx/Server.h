#pragma once

#include "Base.h"
#include "Serialization.h"
#include "Utils.h"
#include "device.h"
#include "ucs/type/status.h"
#include "Serializer.h"

extern PluginManager *PM;

namespace transport::ucx {

class ServerTy : public Base {
protected:
  /* UCX Interfaces */
  std::vector<std::unique_ptr<InterfaceTy>> Interfaces;

  /* Map Host Device Images to Remote Device Images */
  std::unordered_map<const void *, __tgt_device_image *>
      HostToRemoteDeviceImage;

  /* Function to map host device id to remote device id */
  static int32_t mapHostRTLDeviceId(int32_t RTLDeviceID);

  /* Forward declaration of Listener */
  class ListenerTy;

  /* Worker for transmitting data */
  WorkerTy ConnectionWorker;

  /* Cached Debug Level */
  uint32_t DebugLevel;

  /* Unique Target Binary Description */
  __tgt_bin_desc *TBD;

  /* Thread */
  std::thread Thread;

  int32_t Devices = 0;
  std::atomic<bool> Running;

  SerializerTy *Serializer;

  void send(MessageKind Kind, std::string Message) {
    if (!MultiThreaded) {
    auto SendFuture = asyncSend(Kind, Message);

    WorkAvailable.notify_all();

    std::unique_lock<std::mutex> UniqueLock((WorkDoneMtx));
    WorkDone.wait(UniqueLock, [&]() {
      return (bool) (*SendFuture.IsCompleted);
    });

    if (!Interface->EP.Connected)
      return;
    }
  }

  std::pair<MessageKind, std::string> recv(uint64_t Tag) {
    if (!MultiThreaded) {
      auto RecvFuture = asyncRecv(Tag);

      WorkAvailable.notify_all();

      std::unique_lock<std::mutex> UniqueLock((WorkDoneMtx));
      WorkDone.wait(UniqueLock,
                    [&]() { return ((bool)*RecvFuture.IsCompleted); });

      return {(MessageKind)(*RecvFuture.Tag >> 60), *RecvFuture.Buffer};
    }

    return {MessageKind::DataSubmit, std::string()};
  }

public:
  ServerTy(SerializerType Type);
  ~ServerTy();

  void listenForConnections(const ConnectionConfigTy &Config);

  void run();
  void getNumberOfDevices(size_t InterfaceIdx);
  void registerLib(size_t InterfaceIdx, std::string &Message);
  void isValidBinary(size_t InterfaceIdx, std::string &Message);
  void initRequires(size_t InterfaceIdx, std::string &Message);
  void initDevice(size_t InterfaceIdx, std::string &Message);
  void loadBinary(size_t InterfaceIdx, std::string &Message);
  void dataAlloc(size_t InterfaceIdx, std::string &Message);
  void dataSubmit(size_t InterfaceIdx, std::string &Message);
  void dataRetrieve(size_t InterfaceIdx, std::string &Message);
  void runTargetRegion(size_t InterfaceIdx, std::string &Message);
  void runTargetTeamRegion(size_t InterfaceIdx, std::string &Message);
  void dataDelete(size_t InterfaceIdx, std::string &Message);
  void unregisterLib(size_t InterfaceIdx, std::string &Message);
};

class ServerTy::ListenerTy {
  ucp_listener_h *Listener;

  static void handleConnectionCallback(ucp_conn_request_h ConnRequest,
                                       void *Arg) {
    ServerContextTy *Context = (ServerContextTy *)Arg;

    ucp_conn_request_attr_t Attr = {
        .field_mask = UCP_CONN_REQUEST_ATTR_FIELD_CLIENT_ADDR};

    auto Status = ucp_conn_request_query(ConnRequest, &Attr);
    if (Status == UCS_OK) {
      DP("Server received a connection request from client at address "
         "%s:%s\n",
         getIP(&Attr.client_address).c_str(),
         getPort(&Attr.client_address).c_str());
    } else if (Status != UCS_ERR_UNSUPPORTED) {
      ERR("failed to query the connection request ({0})\n",
          ucs_status_string(Status))
    }

    Context->ConnRequests.push_back(ConnRequest);
  }

public:
  ListenerTy(ucp_worker_h Worker, ServerContextTy *Context,
             const ConnectionConfigTy &Config);
  ~ListenerTy() { ucp_listener_destroy(*Listener); }

  void query();
};

} // namespace transport::ucx
