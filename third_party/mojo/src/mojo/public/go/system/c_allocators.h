// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_GO_SYSTEM_C_ALLOCATORS_H_
#define MOJO_PUBLIC_GO_SYSTEM_C_ALLOCATORS_H_

#include <stdint.h>
#include "mojo/public/c/system/core.h"

// Used by cgo: a set of helpers to make calls from Go to C code.
// The reason for these helpers is that you should not pass a Go pointer to
// C code, because Go has a moving garbage collector and passing a pointer
// to object allocated in Go heap to C code can create a dangling pointer:
// var x C.T
// C.Call(&x)
//
// To solve this problem you should allocate memory from C heap, so that
// the pointed object will not be moved go Go GC during C function call:
// x := C.malloc(unsafe.Sizeof(C.T{}))
// defer C.free(x)
// C.Call((*C.T)x)
//
// So these helpers should be used to allocate and free memory in C heap.
// Here is a set of triples:
// struct FunctionParams{};
// struct FunctionParams MallocFunctionParams(...);
// void FreeFunctionParams(struct FunctionParams p);
//
// struct FunctionParams stores all pointers required by Function call,
// MallocFunctionParams and FreeFunctionParams allocate and free memory
// respectively. Use them as:
// ...
// p := C.MallocFunctionParams(...)
// defer C.FreeFunctionParams(p)
// ... (copy data to p)
//
// Allocating and freeing memory in one call is a performance optimization,
// as calls from Go to C are not very cheap.
//
// TODO(rogulenko): consider allocating arrays other way to avoid allocating
//                  memory for one array two times.

struct WaitParams {
  struct MojoHandleSignalsState* state;
};
struct WaitParams MallocWaitParams();
void FreeWaitParams(struct WaitParams p);

struct WaitManyParams {
  MojoHandle* handles;
  MojoHandleSignals* signals;
  uint32_t* index;
  struct MojoHandleSignalsState* states;
};
struct WaitManyParams MallocWaitManyParams(uint32_t length);
void FreeWaitManyParams(struct WaitManyParams p);

struct CreateDataPipeParams {
  MojoHandle* producer;
  MojoHandle* consumer;
  struct MojoCreateDataPipeOptions* opts;
};
struct CreateDataPipeParams MallocCreateDataPipeParams();
void FreeCreateDataPipeParams(struct CreateDataPipeParams p);

struct CreateMessagePipeParams {
  MojoHandle* handle0;
  MojoHandle* handle1;
  struct MojoCreateMessagePipeOptions* opts;
};
struct CreateMessagePipeParams MallocCreateMessagePipeParams();
void FreeCreateMessagePipeParams(struct CreateMessagePipeParams p);

struct CreateSharedBufferParams {
  MojoHandle* handle;
  struct MojoCreateSharedBufferOptions* opts;
};
struct CreateSharedBufferParams MallocCreateSharedBufferParams();
void FreeCreateSharedBufferParams(struct CreateSharedBufferParams p);

struct ReadMessageParams {
  uint32_t* num_bytes;
  uint32_t* num_handles;
};
struct ReadMessageParams MallocReadMessageParams();
void FreeReadMessageParams(struct ReadMessageParams p);

struct MessageArrays {
  void* bytes;
  MojoHandle* handles;
};
struct MessageArrays MallocMessageArrays(uint32_t num_bytes,
                                         uint32_t num_handles);
void FreeMessageArrays(struct MessageArrays arrays);

struct DuplicateBufferHandleParams {
  MojoHandle* duplicate;
  struct MojoDuplicateBufferHandleOptions* opts;
};
struct DuplicateBufferHandleParams MallocDuplicateBufferHandleParams();
void FreeDuplicateBufferHandleParams(struct DuplicateBufferHandleParams p);

struct MapBufferParams {
  void** buffer;
};
struct MapBufferParams MallocMapBufferParams();
void FreeMapBufferParams(struct MapBufferParams p);

struct ReadDataParams {
  uint32_t* num_bytes;
};
struct ReadDataParams MallocReadDataParams();
void FreeReadDataParams(struct ReadDataParams p);

struct DataArray {
  void* elements;
};
struct DataArray MallocDataArray(uint32_t length);
void FreeDataArray(struct DataArray array);

struct WriteDataParams {
  uint32_t* num_bytes;
  void* elements;
};
struct WriteDataParams MallocWriteDataParams(uint32_t length);
void FreeWriteDataParams(struct WriteDataParams p);

#endif  // MOJO_PUBLIC_GO_SYSTEM_C_ALLOCATORS_H_
