#pragma once

#include "Debug.h"
#include "Serialization.h"
#include "Utils.h"
#include "messages.pb.h"
#include <condition_variable>
#include <queue>
#include <vector>

#ifndef DEBUG_PREFIX
#define DEBUG_PREFIX "TARGET " GETNAME(TARGET_NAME) " RTL"
#endif

namespace transport::ucx {

struct ServerContextTy {
  std::vector<ucp_conn_request_h> ConnRequests;
  ucp_listener_h Listener;
};

class ContextTy {
  ucp_context_h Context;

public:
  ContextTy();
  ~ContextTy();

  operator ucp_context_h *() { return &Context; }
};

class WorkerTy {
  bool Initialized = false;
  ucp_worker_h Worker;

  /// Callback to execute when message has been received
  static void receiveCallback(void *, ucs_status_t, const ucp_tag_recv_info_t *,
                              void *UserData) {
    auto *Ctx = (RequestStatus *)UserData;

    Ctx->Complete = 1;
  }

  /// Callback to execute when message has been received
  static void receiveCallback(void *Request, ucs_status_t,
                              ucp_tag_recv_info_t *) {
    auto *Ctx = (RequestStatus *)Request;

    Ctx->Complete = 1;
  }

  /// Initialize context if not already initialized
  void initialize(ucp_context_h *Context);

  ///
  void wait(RequestStatus *Request);

public:
  WorkerTy() = default;
  WorkerTy(ucp_context_h *Context);

  WorkerTy(const WorkerTy &OldWorker);

  ~WorkerTy();

  std::pair<MessageKind, std::string> receive(uint64_t Tag);

  std::mutex ProgressMtx;
  std::atomic<bool> Running;

  operator ucp_worker_h() const { return Worker; }
};

class EndpointTy {
  ucp_ep_h EP;

  /// Callback to execute when message has been sent
  static void sendCallback(void *, ucs_status_t, void *Data) {
    auto *Context = (RequestStatus *)Data;

    Context->Complete = 1;
  }

  static void errorCallback(void *Arg, ucp_ep_h EP, ucs_status_t Status) {
    DP("error handling callback was invoked with status %d (%s)\n", Status,
       ucs_status_string(Status));
    bool *Connected = (bool *)Arg;
    *Connected = false;
  }

public:
  EndpointTy(ucp_worker_h Worker, ucp_conn_request_h ConnRequest);
  EndpointTy(ucp_worker_h Worker, const ConnectionConfigTy &Config);

  SendFutureTy asyncSend(uint64_t Tag, const char *Message, size_t Size);

  /* Helper Functions */
  operator ucp_ep_h() { return EP; }
  bool Connected;
};

struct Base {
  ///
  ContextTy Context;

  Base() = default;
  virtual ~Base() = default;

  std::mutex GMtx;

  struct InterfaceTy {
    WorkerTy Worker;
    EndpointTy EP;
    ConnectionConfigTy Config;

    std::atomic<uint64_t> LastSendTag = 0, LastRecvTag = 0;

    InterfaceTy(ContextTy &Context, const ConnectionConfigTy &Config);
    InterfaceTy(ContextTy &Context, ucp_conn_request_h ConnRequest);
    ~InterfaceTy();


    void send(MessageKind Type, std::string Message, bool IsServer = false);

    void await(SendFutureTy Future);

    std::pair<MessageKind, std::string> receive();
  };
};
} // namespace transport::ucx
