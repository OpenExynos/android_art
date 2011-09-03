// Copyright 2011 Google Inc. All Rights Reserved.

#include "calling_convention_x86.h"
#include "logging.h"
#include "managed_register_x86.h"
#include "utils.h"

namespace art {
namespace x86 {

// Calling convention

ManagedRegister X86ManagedRuntimeCallingConvention::InterproceduralScratchRegister() {
  return X86ManagedRegister::FromCpuRegister(ECX);
}

ManagedRegister X86JniCallingConvention::InterproceduralScratchRegister() {
  return X86ManagedRegister::FromCpuRegister(ECX);
}

static ManagedRegister ReturnRegisterForMethod(Method* method) {
  if (method->IsReturnAFloatOrDouble()) {
    return X86ManagedRegister::FromX87Register(ST0);
  } else if (method->IsReturnALong()) {
    return X86ManagedRegister::FromRegisterPair(EAX_EDX);
  } else if (method->IsReturnVoid()) {
    return ManagedRegister::NoRegister();
  } else {
    return X86ManagedRegister::FromCpuRegister(EAX);
  }
}

ManagedRegister X86ManagedRuntimeCallingConvention::ReturnRegister() {
  return ReturnRegisterForMethod(GetMethod());
}

ManagedRegister X86JniCallingConvention::ReturnRegister() {
  return ReturnRegisterForMethod(GetMethod());
}

// Managed runtime calling convention

ManagedRegister X86ManagedRuntimeCallingConvention::MethodRegister() {
  return X86ManagedRegister::FromCpuRegister(EDI);
}

bool X86ManagedRuntimeCallingConvention::IsCurrentParamInRegister() {
  return false;  // Everything is passed by stack
}

bool X86ManagedRuntimeCallingConvention::IsCurrentParamOnStack() {
  return true;  // Everything is passed by stack
}

ManagedRegister X86ManagedRuntimeCallingConvention::CurrentParamRegister() {
  LOG(FATAL) << "Should not reach here";
  return ManagedRegister::NoRegister();
}

FrameOffset X86ManagedRuntimeCallingConvention::CurrentParamStackOffset() {
  return FrameOffset(displacement_.Int32Value() +   // displacement
                     kPointerSize +                 // Method*
                     (itr_slots_ * kPointerSize));  // offset into in args
}

// JNI calling convention

size_t X86JniCallingConvention::FrameSize() {
  // Return address and Method*
  size_t frame_data_size = 2 * kPointerSize;
  // References plus 2 words for SIRT header
  size_t sirt_size = (ReferenceCount() + 2) * kPointerSize;
  // Plus return value spill area size
  return RoundUp(frame_data_size + sirt_size + SizeOfReturnValue(),
                 kStackAlignment);
}

size_t X86JniCallingConvention::OutArgSize() {
  return RoundUp(NumberOfOutgoingStackArgs() * kPointerSize, kStackAlignment);
}

size_t X86JniCallingConvention::ReturnPcOffset() {
  // Return PC is pushed at the top of the frame by the call into the method
  return FrameSize() - kPointerSize;
}


size_t X86JniCallingConvention::SpillAreaSize() {
  // No spills, return address was pushed at the top of the frame
  return 0;
}

bool X86JniCallingConvention::IsOutArgRegister(ManagedRegister) {
  return false;  // Everything is passed by stack
}

bool X86JniCallingConvention::IsCurrentParamInRegister() {
  return false;  // Everything is passed by stack
}

bool X86JniCallingConvention::IsCurrentParamOnStack() {
  return true;  // Everything is passed by stack
}

ManagedRegister X86JniCallingConvention::CurrentParamRegister() {
  LOG(FATAL) << "Should not reach here";
  return ManagedRegister::NoRegister();
}

FrameOffset X86JniCallingConvention::CurrentParamStackOffset() {
  return FrameOffset(displacement_.Int32Value() - OutArgSize() +
                     (itr_slots_ * kPointerSize));
}

size_t X86JniCallingConvention::NumberOfOutgoingStackArgs() {
  size_t static_args = GetMethod()->IsStatic() ? 1 : 0;  // count jclass
  // regular argument parameters and this
  size_t param_args = GetMethod()->NumArgs() +
                      GetMethod()->NumLongOrDoubleArgs();
  return static_args + param_args + 1;  // count JNIEnv*
}

}  // namespace x86
}  // namespace art
