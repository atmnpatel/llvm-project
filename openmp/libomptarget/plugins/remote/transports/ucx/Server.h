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
  /// Server UCX Interface.
  std::unique_ptr<InterfaceTy> Interface;

  /// Map primary host device image pointer to local device image pointer.
  std::unordered_map<const void *, __tgt_device_image *> LocalDeviceImage;

  /// Forward declaration of UCX Listener class.
  class ListenerTy;

  /// UCX worker responsible for data communication.
  WorkerTy ConnectionWorker;

  /// Cached Debug Level
  uint32_t DebugLevel;

  /// Target binary description of offloaded program.
  __tgt_bin_desc *TgtBinDesc;

  /// Number of devices found.
  int32_t NumDevices = 0;

  /// Is server running.
  std::atomic<bool> Running;

  /// Custom serializer.
  SerializerTy Serializer;

  int NumThreads = 1;
  std::mutex QueueMtx;
  std::atomic<int> BusyThreads;
  std::condition_variable TaskAvailable, TaskFinished;
  std::vector<std::thread> ThreadPool;

  std::queue<std::tuple<uint64_t, MessageKind, std::string>> Tasks;

  /// Start receiving messages from primary host.
  void receiveMessages();

  /// Stop all threads.
  void stop();

public:
  ServerTy();
  ~ServerTy();

  /// Start listening for connections.
  void listenForConnections(const ConnectionConfigTy &Config);

  /// Execute received message.
  void process(uint64_t Tag, MessageKind Kind, std::string_view Message);

  /// Get the number of devices.
  void getNumberOfDevices(uint64_t Tag);

  /// Register library.
  void registerLib(uint64_t Tag, std::string_view Message);

  /// Unregister library.
  void unregisterLib(uint64_t Tag, std::string_view Message);

  /// Check if binary is valid.
  void isValidBinary(uint64_t Tag, std::string_view Message);

  /// Initialize requires.
  void initRequires(uint64_t Tag, std::string_view Message);

  /// Initialize device.
  void initDevice(uint64_t Tag, std::string_view Message);

  /// Load binary.
  void loadBinary(uint64_t Tag, std::string_view Message);

  /// Allocate memory.
  void dataAlloc(uint64_t Tag, std::string_view Message);

  /// Free memory.
  void dataDelete(uint64_t Tag, std::string_view Message);

  /// Move data to device.
  void dataSubmit(uint64_t Tag, std::string_view Message);

  /// Move data from device.
  void dataRetrieve(uint64_t Tag, std::string_view Message);

  /// Run target region.
  void runTargetRegion(uint64_t Tag, std::string_view Message);

  /// Run target teams region.
  void runTargetTeamsRegion(uint64_t Tag, std::string_view Message);
};

class ServerTy::ListenerTy {
  /// UCX Listener
  ucp_listener_h Listener;

  /// New connection callback.
  static void newConnection(ucp_conn_request_h ConnRequest,
                                       void *Arg) {
    auto *Context = (ServerContextTy *)Arg;

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
  ~ListenerTy();

  /// Query details of new connection.
  void query();
};

} // namespace transport::ucx
