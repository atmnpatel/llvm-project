#include "Base.h"
#include "ucp/api/ucp.h"
#include "ucs/type/thread_mode.h"
#include "llvm/Support/ErrorHandling.h"

#define MSG DataSubmit

namespace transport::ucx {

ContextTy::ContextTy() : Context() {
  ucp_params_t Params = {.field_mask = UCP_PARAM_FIELD_FEATURES,
                         .features = UCP_FEATURE_TAG | UCP_FEATURE_WAKEUP};

  if (auto Status = ucp_init(&Params, nullptr, &Context))
    ERR("failed to ucp_init {0}\n", ucs_status_string(Status))
}

ContextTy::~ContextTy() { ucp_cleanup(Context); }

void WorkerTy::initialize(ucp_context_h *Context) {
  ucp_worker_params_t Params = {.field_mask =
                                    UCP_WORKER_PARAM_FIELD_THREAD_MODE,
                                .thread_mode = UCS_THREAD_MODE_SERIALIZED};

  if (auto Status = ucp_worker_create(*Context, &Params, &Worker))
    ERR("Could not initialize another worker {0}\n", ucs_status_string(Status))

  Initialized = true;
}

void WorkerTy::wait(RequestStatus *Request) {
  ucs_status_t Status;

  if (UCS_PTR_IS_ERR(Request)) {
    Status = UCS_PTR_STATUS(Request);
  } else if (UCS_PTR_IS_PTR(Request)) {
    while (!Request->Complete)
      ucp_worker_progress(Worker);

    Request->Complete = 0;
    Status = ucp_request_check_status(Request);
    ucp_request_release(Request);

    if (Status != UCS_OK)
      ERR("unable to {0}\n", ucs_status_string(Status))
  }
}

WorkerTy::WorkerTy(ucp_context_h *Context) : Worker(nullptr) {
  initialize(Context);
}

WorkerTy::~WorkerTy() {
  if (Initialized)
    ucp_worker_destroy(Worker);
}

SendFutureTy EndpointTy::asyncSend(const uint64_t Tag, const char *Message,
                                   const size_t Size) {
  RequestStatus *Req;
  auto *Ctx = new RequestStatus();

  if ((Tag & ~TAG_MASK) == MSG)
    dump(Message, Message + Size);

  Ctx->Complete = 0;
  ucp_request_param_t Param = {.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                                               UCP_OP_ATTR_FIELD_USER_DATA,
                               .cb = {.send = sendCallback},
                               .user_data = Ctx};
  Req = (RequestStatus *)ucp_tag_send_nbx(EP, Message, Size, Tag, &Param);

  if (Req == nullptr)
    return {};

  if (UCS_PTR_IS_ERR(Req))
    ERR("failed to send message {0}\n", ucs_status_string(UCS_PTR_STATUS(Req)))

  return {Req, Ctx, Message};
}

std::pair<MessageKind, std::string> WorkerTy::receive(const uint64_t Tag) {
  ucp_tag_recv_info_t InfoTag;
  ucs_status_t Status;
  ucp_tag_message_h MsgTag;

  while (true) {
    MsgTag = ucp_tag_probe_nb(Worker, Tag, TAG_MASK, 1, &InfoTag);
    if (MsgTag != nullptr)
      break;
    if (ucp_worker_progress(Worker))
      continue;

    Status = ucp_worker_wait(Worker);
    if (Status != UCS_OK) {
      ERR("Failed to send message {0}", ucs_status_string(Status))
    }
  }

  auto Message = std::make_unique<char[]>(InfoTag.length);
  auto *Request = (RequestStatus *)ucp_tag_msg_recv_nb(
      Worker, Message.get(), InfoTag.length, ucp_dt_make_contig(1), MsgTag,
      receiveCallback);

  wait(Request);

  if ((Tag & ~TAG_MASK) == MSG)
    dump(Message.get(), Message.get() + InfoTag.length);

  return {(MessageKind)(InfoTag.sender_tag >> 60),
          std::string(Message.get(), InfoTag.length)};
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

void Base::InterfaceTy::send(MessageKind Type, std::string Message) {
  auto SlabTag = LastSendTag++;
  SlabTag = ((uint64_t)Type << 60) | SlabTag;

  Queue.emplace(EP.asyncSend(SlabTag, Message.data(), Message.length()));
}

void Base::InterfaceTy::synchronize() {
  while (!Queue.empty()) {
    await(Queue.front());
    Queue.pop();
  }
}

void Base::InterfaceTy::send(MessageKind Type,
                             std::pair<char *, size_t> Message) {
  auto SlabTag = LastSendTag++;
  SlabTag = ((uint64_t)Type << 60) | SlabTag;

  Queue.emplace(EP.asyncSend(SlabTag, Message.first, Message.second));
}

void Base::InterfaceTy::await(SendFutureTy &Future) const {
  if (Future.Request == nullptr)
    return;

  if (UCS_PTR_IS_ERR(Future.Request)) {
    ERR("Failed to send message {0}\n",
        ucs_status_string(UCS_PTR_STATUS(Future.Request)))
  }

  while (Future.Context->Complete == 0)
    ucp_worker_progress(Worker);

  ucs_status_t Status = ucp_request_check_status(Future.Request);
  ucp_request_free(Future.Request);

  if (Status != UCS_OK && EP.Connected)
    ERR("failed to send message {0}\n", ucs_status_string(Status))
}

void Base::InterfaceTy::await(ReceiveFutureTy &Future) const {
  if (!Future.Request)
    return;

  ucs_status_t Status;

  if (UCS_PTR_IS_ERR(Future.Request)) {
    ERR("failed to send message {0}\n",
        ucs_status_string(UCS_PTR_STATUS(Future.Request)))
  } else if (UCS_PTR_IS_PTR(Future.Request)) {
    while (!Future.Request->Complete) {
      ucp_worker_progress(Worker);
    }

    Future.Request->Complete = 0;
    Status = ucp_request_check_status(Future.Request);
    ucp_request_release(Future.Request);

    if (Status != UCS_OK)
      ERR("failed to send message {0}\n", ucs_status_string(Status))
  }
}

std::pair<MessageKind, std::string> Base::InterfaceTy::receive() {
  auto SlabTag = LastRecvTag++;
  return Worker.receive(SlabTag);
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
