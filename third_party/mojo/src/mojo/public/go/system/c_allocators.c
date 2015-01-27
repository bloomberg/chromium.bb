// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/go/system/c_allocators.h"

#include <stdlib.h>
#include "mojo/public/c/system/core.h"

struct WaitParams MallocWaitParams() {
  struct WaitParams p;
  p.state = (struct MojoHandleSignalsState*)malloc(
      sizeof(struct MojoHandleSignalsState));
  return p;
}

void FreeWaitParams(struct WaitParams p) {
  free(p.state);
}

struct WaitManyParams MallocWaitManyParams(uint32_t length) {
  struct WaitManyParams p;
  p.handles = (MojoHandle*)malloc(sizeof(MojoHandle) * length);
  p.signals = (MojoHandleSignals*)malloc(sizeof(MojoHandleSignals) * length);
  p.index = (uint32_t*)malloc(sizeof(uint32_t));
  p.states = (struct MojoHandleSignalsState*)malloc(
      sizeof(struct MojoHandleSignalsState) * length);
  return p;
}

void FreeWaitManyParams(struct WaitManyParams p) {
  free(p.handles);
  free(p.signals);
  free(p.index);
  free(p.states);
}

struct CreateDataPipeParams MallocCreateDataPipeParams() {
  struct CreateDataPipeParams p;
  p.producer = (MojoHandle*)malloc(sizeof(MojoHandle));
  p.consumer = (MojoHandle*)malloc(sizeof(MojoHandle));
  p.opts = (struct MojoCreateDataPipeOptions*)malloc(
      sizeof(struct MojoCreateDataPipeOptions));
  return p;
}

void FreeCreateDataPipeParams(struct CreateDataPipeParams p) {
  free(p.producer);
  free(p.consumer);
  free(p.opts);
}

struct CreateMessagePipeParams MallocCreateMessagePipeParams() {
  struct CreateMessagePipeParams p;
  p.handle0 = (MojoHandle*)malloc(sizeof(MojoHandle));
  p.handle1 = (MojoHandle*)malloc(sizeof(MojoHandle));
  p.opts = (struct MojoCreateMessagePipeOptions*)malloc(
      sizeof(struct MojoCreateMessagePipeOptions));
  return p;
}

void FreeCreateMessagePipeParams(struct CreateMessagePipeParams p) {
  free(p.handle0);
  free(p.handle1);
  free(p.opts);
}

struct CreateSharedBufferParams MallocCreateSharedBufferParams() {
  struct CreateSharedBufferParams p;
  p.handle = (MojoHandle*)malloc(sizeof(MojoHandle));
  p.opts = (struct MojoCreateSharedBufferOptions*)malloc(
      sizeof(struct MojoCreateSharedBufferOptions));
  return p;
}

void FreeCreateSharedBufferParams(struct CreateSharedBufferParams p) {
  free(p.handle);
  free(p.opts);
}

struct ReadMessageParams MallocReadMessageParams() {
  struct ReadMessageParams p;
  p.num_bytes = (uint32_t*)malloc(sizeof(uint32_t));
  p.num_handles = (uint32_t*)malloc(sizeof(uint32_t));
  return p;
}

void FreeReadMessageParams(struct ReadMessageParams p) {
  free(p.num_bytes);
  free(p.num_handles);
}

struct MessageArrays MallocMessageArrays(uint32_t num_bytes,
                                         uint32_t num_handles) {
  struct MessageArrays arrays;
  arrays.bytes = (void*)malloc(num_bytes * sizeof(void));
  arrays.handles = (MojoHandle*)malloc(num_handles * sizeof(MojoHandle));
  return arrays;
}

void FreeMessageArrays(struct MessageArrays arrays) {
  free(arrays.bytes);
  free(arrays.handles);
}

struct DuplicateBufferHandleParams MallocDuplicateBufferHandleParams() {
  struct DuplicateBufferHandleParams p;
  p.duplicate = (MojoHandle*)malloc(sizeof(MojoHandle));
  p.opts = (struct MojoDuplicateBufferHandleOptions*)malloc(
      sizeof(struct MojoDuplicateBufferHandleOptions));
  return p;
}

void FreeDuplicateBufferHandleParams(struct DuplicateBufferHandleParams p) {
  free(p.duplicate);
  free(p.opts);
}

struct MapBufferParams MallocMapBufferParams() {
  struct MapBufferParams p;
  p.buffer = (void**)malloc(sizeof(void*));
  return p;
}

void FreeMapBufferParams(struct MapBufferParams p) {
  free(p.buffer);
}

struct ReadDataParams MallocReadDataParams() {
  struct ReadDataParams p;
  p.num_bytes = (uint32_t*)malloc(sizeof(uint32_t));
  return p;
}

void FreeReadDataParams(struct ReadDataParams p) {
  free(p.num_bytes);
}

struct DataArray MallocDataArray(uint32_t length) {
  struct DataArray array;
  array.elements = (void*)malloc(length * sizeof(void));
  return array;
}

void FreeDataArray(struct DataArray array) {
  free(array.elements);
}

struct WriteDataParams MallocWriteDataParams(uint32_t length) {
  struct WriteDataParams p;
  p.num_bytes = (uint32_t*)malloc(sizeof(uint32_t));
  p.elements = (void*)malloc(length * sizeof(void));
  return p;
}

void FreeWriteDataParams(struct WriteDataParams p) {
  free(p.num_bytes);
  free(p.elements);
}

struct TwoPhaseActionParams MallocTwoPhaseActionParams() {
  struct TwoPhaseActionParams p;
  p.buffer = (void**)malloc(sizeof(void*));
  p.num_bytes = (uint32_t*)malloc(sizeof(uint32_t));
  return p;
}

void FreeTwoPhaseActionParams(struct TwoPhaseActionParams p) {
  free(p.buffer);
  free(p.num_bytes);
}
