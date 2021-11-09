//===---- DeviceEnvironment.cpp - Virtual GPU Device Environment -- C++ ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of VGPU environment classes.
//
//===----------------------------------------------------------------------===//

#include <cstdint>
#include "ThreadEnvironment.h"
#include "DeviceEnvironmentImpl.h"
#include <barrier>
#include <mutex>

std::mutex AtomicIncLock;

uint32_t VGPUImpl::atomicInc(uint32_t *Address, uint32_t Val, int Ordering) {
  std::lock_guard G(AtomicIncLock);
  uint32_t V = *Address;
  if (V >= Val)
    *Address = 0;
  else
    *Address += 1;
  return V;
}

void VGPUImpl::setLock(uint32_t *Lock, uint32_t Unset, uint32_t Set,
                       uint32_t OmpSpin, uint32_t BlockId,
                       uint32_t(atomicCAS)(uint32_t *, uint32_t, uint32_t,
                                           int)) {
  // TODO: not sure spinning is a good idea here..
  while (atomicCAS((uint32_t *)Lock, Unset, Set, __ATOMIC_SEQ_CST) != Unset) {
    std::clock_t start = std::clock();
    std::clock_t now;
    for (;;) {
      now = std::clock();
      std::clock_t cycles =
          now > start ? now - start : now + (0xffffffff - start);
      if (cycles >= 1000 * BlockId) {
        break;
      }
    }
  } // wait for 0 to be the read value
}

extern thread_local ThreadEnvironmentTy *ThreadEnvironment;

ThreadEnvironmentTy *getThreadEnvironment() { return ThreadEnvironment; }

ThreadEnvironmentTy::ThreadEnvironmentTy(unsigned Id, WarpEnvironmentTy *WE,
                                         CTAEnvironmentTy *CTAE)
    : Impl(new VGPUImpl::ThreadEnvironmentTy(Id, WE, CTAE)) {}

ThreadEnvironmentTy::~ThreadEnvironmentTy() { delete Impl; }

void ThreadEnvironmentTy::fenceTeam() { Impl->fenceTeam(); }

void ThreadEnvironmentTy::syncWarp() { Impl->syncWarp(); }

unsigned ThreadEnvironmentTy::getThreadIdInWarp() const {
  return Impl->getThreadIdInWarp();
}

unsigned ThreadEnvironmentTy::getThreadIdInBlock() const {
  return Impl->getThreadIdInBlock();
}

unsigned ThreadEnvironmentTy::getGlobalThreadId() const {
  return Impl->getGlobalThreadId();
}

unsigned ThreadEnvironmentTy::getBlockSize() const {
  return Impl->getBlockSize();
}

unsigned ThreadEnvironmentTy::getKernelSize() const {
  return Impl->getKernelSize();
}

unsigned ThreadEnvironmentTy::getBlockId() const { return Impl->getBlockId(); }

unsigned ThreadEnvironmentTy::getNumberOfBlocks() const {
  return Impl->getNumberOfBlocks();
}

LaneMaskTy ThreadEnvironmentTy::getActiveMask() const {
  return Impl->getActiveMask();
}

int32_t ThreadEnvironmentTy::shuffle(int32_t Var, uint64_t SrcLane) {
  return Impl->shuffle(Var, SrcLane);
}

int32_t ThreadEnvironmentTy::shuffleDown(int32_t Var, uint32_t Delta) {
  return Impl->shuffleDown(Var, Delta);
}

void ThreadEnvironmentTy::fenceKernel(int32_t MemoryOrder) {
  return Impl->fenceKernel(MemoryOrder);
}

void ThreadEnvironmentTy::namedBarrier(bool Generic) {
  Impl->namedBarrier(Generic);
}

void ThreadEnvironmentTy::setBlockEnv(ThreadBlockEnvironmentTy *TBE) {
  Impl->setBlockEnv(TBE);
}

void ThreadEnvironmentTy::resetBlockEnv() { Impl->resetBlockEnv(); }

unsigned ThreadEnvironmentTy::getWarpSize() const {
  return Impl->getWarpSize();
}
