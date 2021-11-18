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

class ContextTy {
  ucp_context_h Context;

public:
  ContextTy();
  ~ContextTy();

  operator ucp_context_h *() { return &Context; }
};

class WorkerTy {
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

public:
  WorkerTy(ucp_context_h *Context);
  ~WorkerTy();

  RecvFutureTy asyncRecv(RecvTaskTy &Task);

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

  SendFutureTy asyncSend(SendTaskTy &Task);

  /* Helper Functions */
  operator ucp_ep_h() { return EP; }
  bool Connected;
};

struct Base {
  struct InterfaceTy {
    WorkerTy Worker;
    EndpointTy EP;

    std::atomic<uint64_t> LastRecvTag = 0;

    InterfaceTy(ContextTy &Context, const ConnectionConfigTy &Config);
    InterfaceTy(ContextTy &Context, ucp_conn_request_h ConnRequest);
    ~InterfaceTy();
  };

  ///
  ContextTy Context;

  std::atomic<bool> Running = true;
  std::thread WorkerThread;
  std::condition_variable WorkAvailable;
  std::condition_variable WorkDone;
  std::mutex WorkDoneMtx;

  bool MultiThreaded = false;

  InterfaceTy *Interface;

  struct SendTaskQueueTy {
    std::queue<SendTaskTy> Queue;
    uint64_t Tag = 0;
    std::mutex Mtx;

    bool empty() {
      std::lock_guard Guard(Mtx);
      return Queue.empty();
    }

    bool unsafe_empty() { return Queue.empty(); }

    uint64_t emplace_back(MessageKind Kind, std::string Buffer,
                          std::atomic<bool> *IsCompleted) {
      std::lock_guard Guard(Mtx);
      uint64_t MsgTag = ((uint64_t)Kind << 60) | Tag;
      Queue.emplace(Kind, Buffer, IsCompleted, MsgTag);
      Tag++;
      return MsgTag;
    }

    SendTaskTy &front() { return Queue.front(); }

    void pop() { Queue.pop(); }
  } SendTaskQueue;

  struct RecvTaskQueueTy {
    std::queue<RecvTaskTy> Queue;
    std::mutex Mtx;

    bool empty() {
      std::lock_guard Guard(Mtx);
      return Queue.empty();
    }

    bool unsafe_empty() { return Queue.empty(); }

    void emplace(uint64_t Tag, std::string *Message,
                 std::atomic<bool> *IsCompleted, uint64_t *TagHandle) {
      std::lock_guard Guard(Mtx);
      Queue.emplace(Tag, Message, IsCompleted, TagHandle);
    }

    RecvTaskTy &front() { return Queue.front(); }

    void pop() { Queue.pop(); }

  } RecvTaskQueue;

  SendFutureHandleTy asyncSend(MessageKind Kind, std::string Buffer) {
    auto *IsCompleted = new std::atomic<bool>(false);
    auto Tag = SendTaskQueue.emplace_back(Kind, Buffer, IsCompleted);
    return SendFutureHandleTy{Tag, IsCompleted};
  }

  RecvFutureHandleTy asyncRecv(uint64_t Tag) {
    auto *Message = new std::string();
    auto *IsCompleted = new std::atomic<bool>(false);
    auto *TagHandle = new uint64_t(Tag);
    RecvTaskQueue.emplace(Tag, Message, IsCompleted, TagHandle);
    return {Message, IsCompleted, TagHandle};
  }

  std::queue<SendFutureTy> AwaitSendTaskQueue;
  std::queue<RecvFutureTy> AwaitRecvTaskQueue;
  std::mutex AwaitSendTaskQueueMtx;
  std::mutex AwaitRecvTaskQueueMtx;

  std::mutex WorkAvailableMtx;

  bool IsCompleted(SendFutureTy &Future);
  bool IsCompleted(RecvFutureTy &Future);

  bool isWorkAvailable() {
    return !SendTaskQueue.Queue.empty() || !RecvTaskQueue.Queue.empty() ||
           !AwaitRecvTaskQueue.empty() || !AwaitSendTaskQueue.empty();
  }

  // For Server (tmp)
  Base() = default;

  void initializeWorkerThread() {
    WorkerThread = std::thread([&]() {
      while (Running) {
        std::unique_lock<std::mutex> UniqueLock(WorkAvailableMtx);

        WorkAvailable.wait(UniqueLock, [&]() {
          if (!Running)
            return true;

          return isWorkAvailable();
        });

        // Worker Thread needs to Exit
        if (!isWorkAvailable()) {
          continue;
        }

        {
          std::lock_guard Guard(SendTaskQueue.Mtx);
          while (!SendTaskQueue.unsafe_empty()) {
            AwaitSendTaskQueue.emplace(
                Interface->EP.asyncSend(SendTaskQueue.front()));
            SendTaskQueue.pop();
          }
        }

        {
          std::lock_guard Guard(RecvTaskQueue.Mtx);
          while (!RecvTaskQueue.unsafe_empty()) {
            AwaitRecvTaskQueue.emplace(
                Interface->Worker.asyncRecv(RecvTaskQueue.front()));
            RecvTaskQueue.pop();
          }
        }

        while (ucp_worker_progress(Interface->Worker))
          ;

        {
          std::lock_guard Guard(AwaitSendTaskQueueMtx);
          while (!AwaitSendTaskQueue.empty()) {
            if (IsCompleted(AwaitSendTaskQueue.front())) {
              AwaitSendTaskQueue.pop();
              WorkDone.notify_all();
            } else
              break;
          }
        }

        {
          std::lock_guard Guard(AwaitRecvTaskQueueMtx);
          while (!AwaitRecvTaskQueue.empty()) {
            if (IsCompleted(AwaitRecvTaskQueue.front())) {
              AwaitRecvTaskQueue.pop();
              WorkDone.notify_all();
            } else
              break;
          }
        }

        ucp_worker_progress(Interface->Worker);
      }
    });
  }

  // For Client
  Base(const ConnectionConfigTy &Config) {
    Interface = new InterfaceTy(Context, Config);
    initializeWorkerThread();
  }

  virtual ~Base() {
    if (WorkerThread.joinable())
      WorkerThread.join();
  }
};
} // namespace transport::ucx
