/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/cpu/backend/x64/x64_function.h"

#include "xenia/base/logging.h"
#include "xenia/cpu/backend/x64/x64_backend.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/thread_state.h"

namespace xe {
namespace cpu {
namespace backend {
namespace x64 {

X64Function::X64Function(Module* module, uint32_t address)
    : GuestFunction(module, address) {}

X64Function::~X64Function() {
  // machine_code_ is freed by code cache.
}

void X64Function::Setup(uint8_t* machine_code, size_t machine_code_length) {
  machine_code_ = machine_code;
  machine_code_length_ = machine_code_length;
}

bool X64Function::CallImpl(ThreadState* thread_state, uint32_t return_address) {
  if (!thread_state || !thread_state->context()) {
    XELOGE("X64Function::CallImpl called with invalid thread_state/context");
    return false;
  }
  auto context = thread_state->context();
  if (!context->virtual_membase) {
    XELOGE("X64Function::CallImpl invalid virtual_membase for thread {:08X}",
           thread_state->GetThreadID());
    return false;
  }
  auto backend =
      reinterpret_cast<X64Backend*>(thread_state->processor()->backend());
  auto thunk = backend->host_to_guest_thunk();
  if (!thunk || !machine_code_) {
    XELOGE("X64Function::CallImpl invalid thunk/machine_code target={:08X}"
           " thunk={:p} machine={:p}",
           address(), reinterpret_cast<void*>(thunk),
           reinterpret_cast<void*>(machine_code_));
    return false;
  }
  thunk(machine_code_, context, reinterpret_cast<void*>(uintptr_t(return_address)));
  return true;
}

}  // namespace x64
}  // namespace backend
}  // namespace cpu
}  // namespace xe
