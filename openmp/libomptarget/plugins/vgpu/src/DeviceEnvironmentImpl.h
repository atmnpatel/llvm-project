//===---- DeviceEnvironmentImpl.h - Virtual GPU device environment - C++ --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef OPENMP_LIBOMPTARGET_PLUGINS_VGPU_SRC_DEVICEENVIRONMENTIMPL_H
#define OPENMP_LIBOMPTARGET_PLUGINS_VGPU_SRC_DEVICEENVIRONMENTIMPL_H

#include "ThreadEnvironment.h"
#include <barrier>
#include <cstdio>
#include <functional>
#include <map>
#include <thread>
#include <vector>

class WarpEnvironmentTy {
  const unsigned ID;
  const unsigned NumThreads;

  std::vector<int32_t> ShuffleBuffer;

  std::barrier<std::function<void(void)>> Barrier;
  std::barrier<std::function<void(void)>> ShuffleBarrier;
  std::barrier<std::function<void(void)>> ShuffleDownBarrier;

public:
  WarpEnvironmentTy(unsigned ID, unsigned NumThreads)
      : ID(ID), NumThreads(NumThreads), ShuffleBuffer(NumThreads),
        Barrier(NumThreads, []() {}), ShuffleBarrier(NumThreads, []() {}),
        ShuffleDownBarrier(NumThreads, []() {}) {}

  unsigned getWarpId() const { return ID; }
  int getNumThreads() const { return NumThreads; }

  void sync() { Barrier.arrive_and_wait(); }
  void writeShuffleBuffer(int32_t Var, unsigned LaneId) {
    ShuffleBuffer[LaneId] = Var;
  }

  int32_t getShuffleBuffer(unsigned LaneId) { return ShuffleBuffer[LaneId]; }

  void waitShuffleBarrier() { ShuffleBarrier.arrive_and_wait(); }

  void waitShuffleDownBarrier() { ShuffleBarrier.arrive_and_wait(); }
};

class CTAEnvironmentTy {
public:
  unsigned ID;
  unsigned NumThreads;
  unsigned NumBlocks;

  std::barrier<std::function<void(void)>> Barrier;
  std::barrier<std::function<void(void)>> SyncThreads;
  std::barrier<std::function<void(void)>> NamedBarrier;

  CTAEnvironmentTy(unsigned ID, unsigned NumThreads, unsigned NumBlocks)
      : ID(ID), NumThreads(NumThreads), NumBlocks(NumBlocks),
        Barrier(NumThreads, []() {}), SyncThreads(NumThreads, []() {}),
        NamedBarrier(NumThreads, []() {}) {}

  unsigned getId() const { return ID; }
  unsigned getNumThreads() const { return NumThreads; }

  unsigned getNumBlocks() const { return NumBlocks; }

  void fence() { Barrier.arrive_and_wait(); }
  void syncThreads() { SyncThreads.arrive_and_wait(); }
  void namedBarrier() { NamedBarrier.arrive_and_wait(); }
};

class ThreadBlockEnvironmentTy {
  unsigned ID;
  unsigned NumBlocks;

public:
  ThreadBlockEnvironmentTy(unsigned ID, unsigned NumBlocks)
      : ID(ID), NumBlocks(NumBlocks) {}

  unsigned getId() const { return ID; }
  unsigned getNumBlocks() const { return NumBlocks; }
};

namespace VGPUImpl {
class ThreadEnvironmentTy {
  unsigned ThreadIdInWarp;
  unsigned ThreadIdInBlock;
  unsigned GlobalThreadIdx;

  WarpEnvironmentTy *WarpEnvironment;
  ThreadBlockEnvironmentTy *ThreadBlockEnvironment;
  CTAEnvironmentTy *CTAEnvironment;

public:
  ThreadEnvironmentTy(unsigned ThreadId, WarpEnvironmentTy *WE,
                      CTAEnvironmentTy *CTAE)
      : ThreadIdInWarp(ThreadId),
        ThreadIdInBlock(WE->getWarpId() * WE->getNumThreads() + ThreadId),
        GlobalThreadIdx(CTAE->getId() * CTAE->getNumThreads() +
                        ThreadIdInBlock),
        WarpEnvironment(WE), CTAEnvironment(CTAE) {}

  void setBlockEnv(ThreadBlockEnvironmentTy *TBE) {
    ThreadBlockEnvironment = TBE;
  }

  void resetBlockEnv() {
    delete ThreadBlockEnvironment;
    ThreadBlockEnvironment = nullptr;
  }

  unsigned getThreadIdInWarp() const { return ThreadIdInWarp; }
  unsigned getThreadIdInBlock() const { return ThreadIdInBlock; }
  unsigned getGlobalThreadId() const { return GlobalThreadIdx; }

  unsigned getBlockSize() const { return CTAEnvironment->getNumThreads(); }

  unsigned getBlockId() const { return ThreadBlockEnvironment->getId(); }

  unsigned getNumberOfBlocks() const {
    return ThreadBlockEnvironment->getNumBlocks();
  }
  unsigned getKernelSize() const {}

  // FIXME: This is wrong
  LaneMaskTy getActiveMask() const { return ~0U; }

  void fenceTeam() { CTAEnvironment->fence(); }
  void syncWarp() { WarpEnvironment->sync(); }

  int32_t shuffle(int32_t Var, uint64_t SrcLane) {
    WarpEnvironment->waitShuffleBarrier();
    WarpEnvironment->writeShuffleBuffer(Var, ThreadIdInWarp);
    WarpEnvironment->waitShuffleBarrier();
    Var = WarpEnvironment->getShuffleBuffer(ThreadIdInWarp);
    return Var;
  }

  int32_t shuffleDown(int32_t Var, uint32_t Delta) {
    WarpEnvironment->waitShuffleDownBarrier();
    WarpEnvironment->writeShuffleBuffer(Var, ThreadIdInWarp);
    WarpEnvironment->waitShuffleDownBarrier();
    Var = WarpEnvironment->getShuffleBuffer((ThreadIdInWarp + Delta) %
                                            getWarpSize());
    return Var;
  }

  void namedBarrier(bool Generic) {
    if (Generic) {
      CTAEnvironment->namedBarrier();
    } else {
      CTAEnvironment->syncThreads();
    }
  }

  void fenceKernel(int32_t MemoryOrder) {
    std::atomic_thread_fence(static_cast<std::memory_order>(MemoryOrder));
  }

  unsigned getWarpSize() const { return WarpEnvironment->getNumThreads(); }
};
} // namespace VGPUImpl

#endif // OPENMP_LIBOMPTARGET_PLUGINS_VGPU_SRC_DEVICEENVIRONMENTIMPL_H
