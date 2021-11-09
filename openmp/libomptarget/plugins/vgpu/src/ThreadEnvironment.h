//===---- ThreadEnvironment.h - OpenMP VGPU thread environment ---- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// VGPU Thread environment
//
//===----------------------------------------------------------------------===//

#ifndef OPENMP_LIBOMPTARGET_PLUGINS_VGPU_SRC_THREADENVIRONMENT_H
#define OPENMP_LIBOMPTARGET_PLUGINS_VGPU_SRC_THREADENVIRONMENT_H

// deviceRTL uses <stdint> and DeviceRTL uses explicit definitions

using LaneMaskTy = uint64_t;

// Forward declaration
class WarpEnvironmentTy;
class ThreadBlockEnvironmentTy;
class CTAEnvironmentTy;
namespace VGPUImpl {
class ThreadEnvironmentTy;
void setLock(uint32_t *Lock, uint32_t Unset, uint32_t Set, uint32_t OmpSpin,
             uint32_t BlockId,
             uint32_t(atomicCAS)(uint32_t *, uint32_t, uint32_t, int));
uint32_t atomicInc(uint32_t *Address, uint32_t Val, int Ordering);
} // namespace VGPUImpl

class ThreadEnvironmentTy {
  VGPUImpl::ThreadEnvironmentTy *Impl;

public:
  ThreadEnvironmentTy(unsigned Id, WarpEnvironmentTy *WE,
                      CTAEnvironmentTy *CTAE);

  ~ThreadEnvironmentTy();

  unsigned getThreadIdInWarp() const;

  unsigned getThreadIdInBlock() const;

  unsigned getGlobalThreadId() const;

  unsigned getBlockSize() const;

  unsigned getKernelSize() const;

  unsigned getBlockId() const;

  unsigned getNumberOfBlocks() const;

  LaneMaskTy getActiveMask() const;

  unsigned getWarpSize() const;

  int32_t shuffle(int32_t Var, uint64_t SrcLane);

  int32_t shuffleDown(int32_t Var, uint32_t Delta);

  void fenceKernel(int32_t MemoryOrder);

  void fenceTeam();

  void syncWarp();

  void namedBarrier(bool Generic);

  void setBlockEnv(ThreadBlockEnvironmentTy *TBE);

  void resetBlockEnv();
};

ThreadEnvironmentTy *getThreadEnvironment(void);

#endif