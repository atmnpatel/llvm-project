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
  ucp_worker_h Worker;

public:
  WorkerTy(ucp_context_h *Context);
  ~WorkerTy();

  operator ucp_worker_h() const { return Worker; }
};

class EndpointTy {
  ucp_ep_h EP;

  static void errorCallback(void *Arg, ucp_ep_h EP, ucs_status_t Status) {
    DP("error handling callback was invoked with status %d (%s)\n", Status,
       ucs_status_string(Status));
    bool *Connected = (bool *)Arg;
    *Connected = false;
  }

public:
  EndpointTy(ucp_worker_h Worker, ucp_conn_request_h ConnRequest);
  EndpointTy(ucp_worker_h Worker, const ConnectionConfigTy &Config);

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

    std::atomic<bool> Running = true;

    std::vector<char> Buffer;

    std::mutex SendFuturesMtx;

    InterfaceTy(ContextTy &Context, const ConnectionConfigTy &Config);
    InterfaceTy(ContextTy &Context, ucp_conn_request_h ConnRequest);
    ~InterfaceTy();

//    std::string ReceiveBuffer;

    ////////////////////////////////////////////////////////////////////////////
    // Endpoint Callbacks
    ////////////////////////////////////////////////////////////////////////////

    /// Callback to execute when message has been sent
    static void sendCallback(void *, ucs_status_t, void *Data) {
      auto *Context = (RequestStatus *)Data;

      Context->Complete = 1;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Worker Callbacks
    ////////////////////////////////////////////////////////////////////////////

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

    void send(MessageKind Type, std::string Message);
//    void asyncSend(MessageKind Type, std::string Message);

    bool await(SendFutureTy *Future);

    MessageTy receive();

    std::queue<SendFutureTy*> SendFutures;

    void wait(RequestStatus *Request);
  };
};
} // namespace transport::ucx
