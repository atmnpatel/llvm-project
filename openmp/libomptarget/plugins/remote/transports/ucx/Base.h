#pragma once

#include "Debug.h"
#include "Serialization.h"
#include "Utils.h"
#include "ucp/api/ucp.h"
#include "ucs/type/status.h"
#include "ucx.pb.h"
#include <queue>
#include <vector>

#ifndef TARGET_NAME
#define TARGET_NAME UCX
#endif
#define DEBUG_PREFIX "TARGET " GETNAME(TARGET_NAME) " RTL"

using namespace openmp::libomptarget::ucx;

namespace transport {
namespace ucx {

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

  /// Callback to execute when message has been sent
  static void sendCallback(void *, ucs_status_t, void *Data) {
    RequestStatus *Context = (RequestStatus *)Data;

    Context->Complete = 1;
  }

  /// Callback to execute when message has been received
  static void receiveCallback(void *, ucs_status_t, const ucp_tag_recv_info_t *,
                              void *UserData) {
    RequestStatus *Ctx = (RequestStatus *)UserData;

    Ctx->Complete = 1;
  }

  /// Callback to execute when message has been received
  static void receiveCallback(void *Request, ucs_status_t,
                              ucp_tag_recv_info_t *) {
    RequestStatus *Ctx = (RequestStatus *)Request;

    Ctx->Complete = 1;
  }

  /// Initialize context if not already initialized
  void initialize(ucp_context_h *Context);

  ///
  void wait(RequestStatus *Request);

public:
  WorkerTy() = default;
  WorkerTy(ucp_context_h *Context);

  ~WorkerTy();

  SendFutureTy asyncSend(ucp_ep_h EP, const uint64_t Tag, const char *Message,
                         const size_t Size);
  ReceiveFutureTy asyncReceive(const uint64_t Tag, Decoder &D,
                               std::vector<SlabTy> &Slabs);

  std::string receive(const uint64_t Tag);

  operator ucp_worker_h() const { return Worker; }
};

class Endpoint {
  ucp_ep_h EP;
  ucp_worker_h DataWorker;

  static void errorCallback(void *Arg, ucp_ep_h EP, ucs_status_t Status) {
    DP("error handling callback was invoked with status %d (%s)\n", Status,
       ucs_status_string(Status));
    bool *Connected = (bool *) Arg;
    *Connected = false;
  }

public:
  Endpoint(ucp_worker_h Worker, ucp_conn_request_h ConnRequest);
  Endpoint(ucp_worker_h Worker, const ConnectionConfigTy &Config);

  ~Endpoint();

  /* Helper Functions */
  operator ucp_ep_h() { return EP; }
  bool Connected;
};

struct Base {
  ///
  ContextTy Context;

  AllocatorTy Allocator;

  Base() = default;
  virtual ~Base() = default;

  struct TagTy {
    uint64_t Tag = 0;
    uint64_t IntermediateTag = 0;

    TagTy() = default;
    TagTy(const TagTy &T);

    uint64_t nextRequest();
    uint64_t nextIntermediateTag();

    operator uint64_t();
  };

  struct InterfaceTy {
    WorkerTy Worker;
    Endpoint EP;

    AllocatorTy *Allocator;

    TagTy LastSendTag, LastRecvTag;

    InterfaceTy(ContextTy &Context, AllocatorTy *Allocator,
                         const ConnectionConfigTy &Config);
    InterfaceTy(ContextTy &Context, AllocatorTy *Allocator,
                         ucp_conn_request_h ConnRequest);

    std::vector<SlabTy> asyncReceive(Decoder &D,
                                     std::queue<ReceiveFutureTy> &Futures);

    template <typename... ArgsTy>
    void sendMessage(const MessageTy &Type, ArgsTy const &...Args) {
      EncoderTy E(Allocator, Type, Args...);
      send(E.MessageSlabs);
    }

    void send(const SlabListTy &Messages);
    void send(const std::vector<std::string> &Messages);

    std::queue<SendFutureTy> asyncSend(const SlabListTy &Messages);
    std::queue<SendFutureTy>
    asyncSend(const std::vector<std::string> &Messages);

    void await(SendFutureTy &Future);
    void await(ReceiveFutureTy &Future);

    void awaitReceives(std::queue<ReceiveFutureTy> &Futures, Decoder &D,
                       std::vector<SlabTy> &Slabs);

    HeaderTy receive();

    std::string receiveMessage(bool Header = true);

    void receive(Decoder &D);
  };
};
} // namespace ucx
} // namespace transport
