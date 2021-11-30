#include "Base.h"
#include "ucp/api/ucp.h"
#include "ucs/type/thread_mode.h"
#include "ucx/Serialization.h"
#include "llvm/Support/ErrorHandling.h"
#include <sys/epoll.h>

namespace transport::ucx {

ContextTy::ContextTy() : Context() {
  ucp_params_t Params = {.field_mask = UCP_PARAM_FIELD_FEATURES,
                         .features = UCP_FEATURE_TAG | UCP_FEATURE_WAKEUP};

  if (auto Status = ucp_init(&Params, nullptr, &Context))
    ERR("failed to ucp_init ({0})", ucs_status_string(Status))
}

ContextTy::~ContextTy() { ucp_cleanup(Context); }

WorkerTy::WorkerTy(ucp_context_h *Context) : Worker(nullptr) {
  ucp_worker_params_t Params = {.field_mask =
                                    UCP_WORKER_PARAM_FIELD_THREAD_MODE,
                                .thread_mode = UCS_THREAD_MODE_MULTI};

  if (auto Status = ucp_worker_create(*Context, &Params, &Worker))
    ERR("failed ot ucp_worker_create ({0})", ucs_status_string(Status))
}

WorkerTy::~WorkerTy() {
  ucp_worker_destroy(Worker);
}

EndpointTy::EndpointTy(ucp_worker_h Worker, ucp_conn_request_h ConnRequest) {
  ucp_ep_params_t Params = {
      .field_mask =
          UCP_EP_PARAM_FIELD_ERR_HANDLER | UCP_EP_PARAM_FIELD_CONN_REQUEST,
      .err_handler = {.cb = errorCallback, .arg = &Connected},
      .conn_request = ConnRequest};

  if (auto Status = ucp_ep_create(Worker, &Params, &EP))
    ERR("failed to create an endpoint on the server: {0}\n",
        ucs_status_string(Status))
}

EndpointTy::EndpointTy(ucp_worker_h Worker, const ConnectionConfigTy &Config) {
  sockaddr_in ConnectAddress = {
      .sin_family = AF_INET,
      .sin_port = htons(Config.Port),
      .sin_addr = {.s_addr = inet_addr(Config.Address.c_str())}};

  ucp_ep_params_t Params = {
      .field_mask = UCP_EP_PARAM_FIELD_FLAGS | UCP_EP_PARAM_FIELD_SOCK_ADDR |
                    UCP_EP_PARAM_FIELD_ERR_HANDLER |
                    UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE,
      .err_mode = UCP_ERR_HANDLING_MODE_PEER,
      .err_handler = {.cb = errorCallback, .arg = &Connected},
      .flags = UCP_EP_PARAMS_FLAGS_CLIENT_SERVER,
      .sockaddr = {.addr = (struct sockaddr *)&ConnectAddress,
                   .addrlen = sizeof(ConnectAddress)}};

  if (auto Status = ucp_ep_create(Worker, &Params, &EP))
    ERR("Failed to connect to address ({0})\n",
        std::string(ucs_status_string(Status)))
}

Base::InterfaceTy::InterfaceTy(ContextTy &Context,
                               const ConnectionConfigTy &Config)
    : Worker((ucp_context_h *)Context), EP(Worker, Config) {}
Base::InterfaceTy::InterfaceTy(ContextTy &Context,
                               ucp_conn_request_h ConnRequest)
    : Worker((ucp_context_h *)Context), EP(Worker, ConnRequest) {}

void Base::InterfaceTy::send(uint64_t SlabTag, std::string Message) {
  SendFutureTy *Fut = new SendFutureTy(std::move(Message));
  Fut->Context->Complete = 0;
  ucp_request_param_t Param = {.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                                               UCP_OP_ATTR_FIELD_USER_DATA,
      .cb = {.send = sendCallback},
      .user_data = Fut->Context};
  Fut->Request = (RequestStatus *)ucp_tag_send_nbx(EP, Fut->Message.data(), Fut->Message.length(), SlabTag, &Param);

  if (Fut->Request == nullptr)
    return;

  if (UCS_PTR_IS_ERR(Fut->Request))
    ERR("failed to send message {0}\n", ucs_status_string(UCS_PTR_STATUS(Req)))

  std::lock_guard SendGuard(SendFuturesMtx);
  SendFutures.emplace(Fut);
}

bool Base::InterfaceTy::await(SendFutureTy *Future) {
  if (Future->Request == nullptr)
    return true;

  if (UCS_PTR_IS_ERR(Future->Request)) {
    ERR("Failed to send message {0}\n",
        ucs_status_string(UCS_PTR_STATUS(Future->Request)))
  }

  while (Future->Context->Complete == 0)
    return false;

  ucs_status_t Status = ucp_request_check_status(Future->Request);
  ucp_request_free(Future->Request);

  if (Status != UCS_OK && EP.Connected)
    ERR("failed to send message {0}\n", ucs_status_string(Status))

  return true;
}

void Base::InterfaceTy::wait(RequestStatus *Request) {
  ucs_status_t Status;

  if (UCS_PTR_IS_ERR(Request)) {
    Status = UCS_PTR_STATUS(Request);
  } else if (UCS_PTR_IS_PTR(Request)) {
    while (!Request->Complete) {
      ucp_worker_progress(Worker);
    }

    Request->Complete = 0;
    Status = ucp_request_check_status(Request);
    ucp_request_release(Request);

    if (Status != UCS_OK)
      ERR("unable to {0}\n", ucs_status_string(Status))
  }
}

MessageTy Base::InterfaceTy::receive(uint64_t SlabTag) {
  ucp_tag_recv_info_t InfoTag;
  ucs_status_t Status;
  ucp_tag_message_h MsgTag;

  while (Running) {
    std::unique_lock Latch(SendFuturesMtx);
    if (!SendFutures.empty() && await(SendFutures.front())) {
      delete SendFutures.front();
      SendFutures.pop();
    }
    Latch.unlock();
    MsgTag = ucp_tag_probe_nb(Worker, SlabTag, TAG_MASK, 1, &InfoTag);
    if (MsgTag != nullptr) {
      break;
    }
    
    Progressed = false;

    {
      std::lock_guard Guard(Mtx);
      if (Progressed)
        continue;
      Progressed = ucp_worker_progress(Worker);
      if (Progressed)
        continue;
      Status = ucp_worker_wait(Worker);
    }

    if (Status != UCS_OK) {
      ERR("Failed to recv message {0}", ucs_status_string(Status))
    }
  }

  if (!Running)
    return {(MessageKind) Count, ""};

  char * ReceiveBuffer = new char[InfoTag.length];
  auto *Request = (RequestStatus *)ucp_tag_msg_recv_nb(
      Worker, ReceiveBuffer, InfoTag.length, ucp_dt_make_contig(1), MsgTag,
      receiveCallback);

  wait(Request);

  return {(MessageKind)(InfoTag.sender_tag >> 60), {ReceiveBuffer, InfoTag.length}};
}

Base::InterfaceTy::~InterfaceTy() {
  ucp_request_param_t Param = {.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS,
                               .flags = UCP_EP_CLOSE_FLAG_FORCE};
  void *CloseEPRequest = ucp_ep_close_nbx(EP, &Param);
  if (UCS_PTR_IS_PTR(CloseEPRequest)) {
    ucs_status_t Status;
    do {
      ucp_worker_progress(Worker);
      Status = ucp_request_check_status(CloseEPRequest);
    } while (Status == UCS_INPROGRESS);

    ucp_request_free(CloseEPRequest);
  } else if (UCS_PTR_STATUS(CloseEPRequest) != UCS_OK) {
    ERR("failed to close ep {0}\n", (void *)EP)
  }
}

} // namespace transport::ucx
