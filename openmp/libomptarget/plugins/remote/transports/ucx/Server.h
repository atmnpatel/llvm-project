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
  std::unique_ptr<InterfaceTy> Interface;

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

  int32_t Devices = 0;
  std::atomic<bool> Running;

  SerializerTy *Serializer;

  int NumThreads = 1;
  std::mutex QueueMtx;
  int BusyThreads = 0;
  std::condition_variable TaskAvailable, TaskFinished;
  std::vector<std::thread> ThreadPool;
  std::queue<std::pair<MessageKind, std::string>> Tasks;

public:
  ServerTy(SerializerType Type);
  ~ServerTy();

  void listenForConnections(const ConnectionConfigTy &Config);

  void run();
  void getNumberOfDevices();
  void registerLib(std::string_view Message);
  void isValidBinary(std::string_view Message);
  void initRequires(std::string_view Message);
  void initDevice(std::string_view Message);
  void loadBinary(std::string_view Message);
  void dataAlloc(std::string_view Message);
  void dataSubmit(std::string_view Message);
  void dataRetrieve(std::string_view Message);
  void runTargetRegion(std::string_view Message);
  void runTargetTeamRegion(std::string_view Message);
  void dataDelete(std::string_view Message);
  void unregisterLib(std::string_view Message);

  void process(MessageKind Kind, std::string_view Message);
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
