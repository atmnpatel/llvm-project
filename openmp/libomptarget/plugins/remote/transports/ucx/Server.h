#pragma once

#include "Base.h"
#include "Serialization.h"
#include "Utils.h"
#include "device.h"
#include "ucs/type/status.h"

extern PluginManager *PM;

namespace transport::ucx {

class ServerTy : public Base {
protected:
  /* UCX Interface */
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
  std::unique_ptr<__tgt_bin_desc> TBD;

  /* Worker Thread */
  std::thread *Thread;

  int32_t Devices;

public:
  ServerTy();
  ~ServerTy();

  void listenForConnections(const ConnectionConfigTy &Config);

  void run();
  virtual void getNumberOfDevices() = 0;
  virtual void registerLib(std::string &Message) = 0;
  virtual void isValidBinary(std::string &Message) = 0;
  virtual void initRequires(std::string &Message) = 0;
  virtual void initDevice(std::string &Message) = 0;
  virtual void loadBinary(std::string &Message) = 0;
  virtual void dataAlloc(std::string &Message) = 0;
  virtual void dataSubmit(std::string &Message) = 0;
  virtual void dataRetrieve(std::string &Message) = 0;
  virtual void runTargetRegion(std::string &Message) = 0;
  virtual void runTargetTeamRegion(std::string &Message) = 0;
  virtual void dataDelete(std::string &Message) = 0;
  virtual void unregisterLib(std::string &Message) = 0;
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

class ProtobufServerTy : public ServerTy {
  template <typename T> T deserialize() {
    auto Message = Interface->receive().second;
    T Request;
    if (!Request.ParseFromString(Message))
      ERR("Could not parse message");
    return Request;
  }

  template <typename T> T deserialize(std::string &Message) {
    T Request;
    if (!Request.ParseFromString(Message))
      ERR("Could not parse message");
    return Request;
  }

public:

  void getNumberOfDevices() override;
  void registerLib(std::string &Message) override;
  void isValidBinary(std::string &Message) override;
  void initRequires(std::string &Message) override;
  void initDevice(std::string &Message) override;
  void loadBinary(std::string &Message) override;
  void dataAlloc(std::string &Message) override;
  void dataSubmit(std::string &Message) override;
  void dataRetrieve(std::string &Message) override;
  void runTargetRegion(std::string &Message) override;
  void runTargetTeamRegion(std::string &Message) override;
  void dataDelete(std::string &Message) override;
  void unregisterLib(std::string &Message) override;
};

struct CustomServerTy : public ServerTy {
  void getNumberOfDevices() override;
  void registerLib(std::string &Message) override;
  void isValidBinary(std::string &Message) override;
  void initRequires(std::string &Message) override;
  void initDevice(std::string &Message) override;
  void loadBinary(std::string &Message) override;
  void dataAlloc(std::string &Message) override;
  void dataSubmit(std::string &Message) override;
  void dataRetrieve(std::string &Message) override;
  void runTargetRegion(std::string &Message) override;
  void runTargetTeamRegion(std::string &Message) override;
  void dataDelete(std::string &Message) override;
  void unregisterLib(std::string &Message) override;
};
} // namespace transport::ucx
