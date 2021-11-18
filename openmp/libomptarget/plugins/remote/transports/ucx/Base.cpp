#include "Base.h"
#include "ucp/api/ucp.h"
#include "ucs/type/thread_mode.h"
#include "ucx/Serialization.h"
#include "llvm/Support/ErrorHandling.h"

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
                                .thread_mode = UCS_THREAD_MODE_SERIALIZED};

  if (auto Status = ucp_worker_create(*Context, &Params, &Worker))
    ERR("failed ot ucp_worker_create ({0})", ucs_status_string(Status))

  Running = true;
}

WorkerTy::~WorkerTy() { ucp_worker_destroy(Worker); }

SendFutureTy EndpointTy::asyncSend(SendTaskTy &Task) {
  if (Task.Kind == MessageKind::DataSubmit) {
    printf("Submitting %p, %zu\n", Task.Buffer.data(), Task.Buffer.size());
    // dump(Task.Buffer);
  }
  RequestStatus *Req;
  auto *Ctx = new RequestStatus();

  Ctx->Complete = 0;
  ucp_request_param_t Param = {.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                                               UCP_OP_ATTR_FIELD_USER_DATA,
                               .cb = {.send = sendCallback},
                               .user_data = Ctx};
  Req = (RequestStatus *)ucp_tag_send_nbx(
      EP, Task.Buffer.data(), Task.Buffer.length(), Task.Tag, &Param);

  if (Req == nullptr)
    return {nullptr, nullptr, Task.Completed};

  if (UCS_PTR_IS_ERR(Req))
    ERR("failed to send message {0}\n", ucs_status_string(UCS_PTR_STATUS(Req)))

  return {Req, Ctx, Task.Completed};
}

RecvFutureTy WorkerTy::asyncRecv(RecvTaskTy &Task) {
  ucp_tag_recv_info_t InfoTag;
  ucs_status_t Status;
  ucp_tag_message_h MsgTag;

  while (Running) {
    std::lock_guard Guard(ProgressMtx);
    MsgTag = ucp_tag_probe_nb(Worker, Task.Tag, TAG_MASK, 1, &InfoTag);
    if (MsgTag != nullptr)
      break;
    if (ucp_worker_progress(Worker))
      continue;

    Status = ucp_worker_wait(Worker);
    if (Status != UCS_OK) {
      ERR("Failed to recv message {0}", ucs_status_string(Status))
    }
  }

  if (!Running)
    return {};

  Task.Message->resize(InfoTag.length);
  auto *Request = (RequestStatus *)ucp_tag_msg_recv_nb(
      Worker, Task.Message->data(), InfoTag.length, ucp_dt_make_contig(1),
      MsgTag, receiveCallback);

  *Task.TagHandle = InfoTag.sender_tag;

  return {(MessageKind)(InfoTag.sender_tag >> 60), Request, Task.Message,
          Task.Completed};
}

EndpointTy::EndpointTy(ucp_worker_h Worker, ucp_conn_request_h ConnRequest)
    : EP(nullptr), Connected(true) {

  ucp_ep_params_t Params = {
      .field_mask =
          UCP_EP_PARAM_FIELD_ERR_HANDLER | UCP_EP_PARAM_FIELD_CONN_REQUEST,
      .err_handler = {.cb = errorCallback, .arg = &Connected},
      .conn_request = ConnRequest};

  if (auto Status = ucp_ep_create(Worker, &Params, &EP))
    ERR("failed to create an endpoint on the server: {0}\n",
        ucs_status_string(Status))
}

EndpointTy::EndpointTy(ucp_worker_h Worker, const ConnectionConfigTy &Config)
    : EP(nullptr), Connected(true) {
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

bool Base::IsCompleted(SendFutureTy &Future) {
  if (Future.Request == nullptr) {
    *Future.IsCompleted = true;
    return true;
  }

  if (UCS_PTR_IS_ERR(Future.Request)) {
    ERR("Failed to send message {0}\n",
        ucs_status_string(UCS_PTR_STATUS(Future.Request)))
  }

  if (Future.Context->Complete == 0)
    return false;

  ucs_status_t Status = ucp_request_check_status(Future.Request);

  if (Status == UCS_INPROGRESS)
    return false;

  ucp_request_free(Future.Request);

  *Future.IsCompleted = true;

  if (Status != UCS_OK && Interface->EP.Connected)
    ERR("failed to send message {0}\n", ucs_status_string(Status))
  return true;
}

bool Base::IsCompleted(RecvFutureTy &Future) {
  ucs_status_t Status;

  if (UCS_PTR_IS_ERR(Future.Request)) {
    Status = UCS_PTR_STATUS(Future.Request);

    if (Status != UCS_OK)
      ERR("unable to {0}\n", ucs_status_string(Status))
    return false;
  }
  if (UCS_PTR_IS_PTR(Future.Request)) {
    if (!Future.Request->Complete)
      return false;

    Status = ucp_request_check_status(Future.Request);

    if (Status == UCS_INPROGRESS)
      return false;

    ucp_request_release(Future.Request);

    *Future.IsCompleted = true;
    if (Status != UCS_OK)
      ERR("unable to {0}\n", ucs_status_string(Status))
    return true;
  }
  ERR("unimplemented?");
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
