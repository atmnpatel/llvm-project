#pragma once

#include "Base.h"
#include "Serialization.h"
#include "Utils.h"
#include "device.h"
#include "ucs/type/status.h"

extern PluginManager *PM;

namespace transport {
namespace ucx {

class Server : public Base {
protected:
  /* UCX Interface */
  std::unique_ptr<InterfaceTy> Interface;

  /* Map Host Device Images to Remote Device Images */
  std::unordered_map<const void *, __tgt_device_image *>
      HostToRemoteDeviceImage;

  /* Function to map host device id to remote device id */
  int32_t mapHostRTLDeviceId(int32_t RTLDeviceID);

  /* Forward declaration of Listener */
  class ListenerTy;

  /* Worker for transmitting data */
  WorkerTy ConnectionWorker;

  /* Cached Debug Level */
  int DebugLevel;

  /* Unique Target Binary Description */
  std::unique_ptr<__tgt_bin_desc> TBD;

  /* Worker Thread */
  std::thread *Thread;

public:
  Server();
  ~Server();

  virtual void listenForConnections(const ConnectionConfigTy &Config) = 0;
};

class Server::ListenerTy {
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
      llvm::report_fatal_error(
          llvm::formatv("failed to query the connection request ({0})\n",
                        ucs_status_string(Status))
              .str());
    }

    Context->ConnRequests.push_back(ConnRequest);
  }

public:
  ListenerTy(ucp_worker_h Worker, ServerContextTy *Context,
             const ConnectionConfigTy &Config);
  ~ListenerTy() { ucp_listener_destroy(*Listener); }

  void query();
};

class ProtobufServer : public Server {
  template <typename T> T deserialize() {
    auto Message = Interface->receiveMessage(false);
    T Request;
    if (!Request.ParseFromString(Message))
      llvm::report_fatal_error("Could not parse message");
    return Request;
  }

  std::string getHeader(MessageTy Type) {
    HeaderTy Header;
    Header.set_type(Type);
    return Header.SerializeAsString();
  }

public:
  void listenForConnections(const ConnectionConfigTy &Config) override;

  void run();
  void getNumberOfDevices();
  void registerLib();
  void isValidBinary();
  void initRequires();
  void initDevice();
  void loadBinary();
  void dataAlloc();
  void dataSubmit();
  void dataRetrieve();
  void runTargetRegion();
  void runTargetTeamRegion();
  void dataDelete();
  void unregisterLib();
};

class SelfSerializationServer : public Server {
public:
  void listenForConnections(const ConnectionConfigTy &Config) override;

  void deserialize(Decoder &D, __tgt_bin_desc *TBD);

  void run();
  void getNumberOfDevices();
  void registerLib(Decoder &D);
  void isValidBinary(Decoder &D);
  void initRequires(Decoder &D);
  void initDevice(Decoder &D);
  void loadBinary(Decoder &D);
  void dataAlloc(Decoder &D);
  void dataSubmit(Decoder &D);
  void dataRetrieve(Decoder &D);
  void runTargetRegion(Decoder &D);
  void runTargetTeamRegion(Decoder &D);
  void dataDelete(Decoder &D);
  void unregisterLib(Decoder &D);
};
} // namespace ucx
} // namespace transport