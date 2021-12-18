#pragma once

#include "Debug.h"
#include "Serialization.h"
#include "Utils.h"
#include <condition_variable>
#include <queue>
#include <vector>

#ifndef DEBUG_PREFIX
#define DEBUG_PREFIX "TARGET " GETNAME(TARGET_NAME) " RTL"
#endif

namespace transport::ucx {

struct ServerContextTy {
  /// New connection requests.
  std::vector<ucp_conn_request_h> ConnRequests;

  /// UCX Listener.
  ucp_listener_h Listener;
};

class ContextTy {
  /// UCX Context.
  ucp_context_h Context;

public:
  ContextTy();
  ~ContextTy();

  operator ucp_context_h *() { return &Context; }
};

class WorkerTy {
  /// UCX Worker.
  ucp_worker_h Worker;

public:
  WorkerTy(ucp_context_h *Context);
  ~WorkerTy();

  operator ucp_worker_h() const { return Worker; }
};

class EndpointTy {
  /// UCX Endpoint.
  ucp_ep_h EP;

  /// Callback on error within UCX.
  static void onError(void *Arg, ucp_ep_h EP, ucs_status_t Status) {
    DP("error handling callback was invoked with status %d (%s)\n", Status,
       ucs_status_string(Status));
    bool *Connected = (bool *)Arg;
    *Connected = false;
  }

public:
  EndpointTy(ucp_worker_h Worker, ucp_conn_request_h ConnRequest);
  EndpointTy(ucp_worker_h Worker, const ConnectionConfigTy &Config);

  /// Is endpoint connected to remote.
  bool Connected = true;

  operator ucp_ep_h() { return EP; }
};

struct Base {
  /// UCX Context.
  ContextTy Context;

  Base() = default;
  virtual ~Base() = default;

  struct InterfaceTy {
    /// Worker for remote communication.
    WorkerTy Worker;

    /// Endpoint for remote communication.
    EndpointTy EP;

    ConnectionConfigTy Config;

    std::atomic<uint64_t> LastSendTag = 0;

    std::atomic<bool> Running = true;

    std::vector<char> Buffer;

    std::mutex SendFuturesMtx, Mtx;
    std::atomic<bool> Progressed = false;

    InterfaceTy(ContextTy &Context, const ConnectionConfigTy &Config);
    InterfaceTy(ContextTy &Context, ucp_conn_request_h ConnRequest);
    ~InterfaceTy();

    /// Callback on UCX send.
    static void onSend(void *, ucs_status_t, void *Data) {
      auto *Context = (RequestStatus *)Data;

      Context->Complete = 1;
    }

    /// Callback to UCX receive.
    static void onReceive(void *Request, ucs_status_t, ucp_tag_recv_info_t *) {
      auto *Ctx = (RequestStatus *)Request;

      Ctx->Complete = 1;
    }

    void send(uint64_t Tag, std::string Message);

    bool isSent(SendFutureTy *Future);

    MessageTy receive(uint64_t Tag);

    std::queue<SendFutureTy *> SendFutures;

    void wait(RequestStatus *Request);

    uint64_t getTag(MessageKind Type) {
      auto SlabTag = LastSendTag++;
      return ((uint64_t)Type << 60) | SlabTag;
    }

    uint64_t getTag() { return LastSendTag++; }

    uint64_t encodeTag(uint64_t SlabTag, MessageKind Type) {
      return ((uint64_t)Type << 60) | SlabTag;
    }
  };
};
} // namespace transport::ucx
