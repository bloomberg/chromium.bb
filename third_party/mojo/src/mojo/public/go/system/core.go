// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package system

//#include "c_allocators.h"
//#include "mojo/public/c/system/core.h"
import "C"
import (
	"reflect"
	"unsafe"
)

// core is an instance of the Mojo system APIs implementation.
var core coreImpl

// Core is an interface giving access to the base operations.
// See |src/mojo/public/c/system/core.h| for the underlying api.
type Core interface {
	// AcquireNativeHandle acquires a handle from the native side. The handle
	// will be owned by the returned object and must not be closed outside of
	// it.
	AcquireNativeHandle(handle MojoHandle) UntypedHandle

	// GetTimeTicksNow returns a monotonically increasing platform dependent
	// tick count representing "right now". Resolution depends on the system
	// configuration.
	GetTimeTicksNow() MojoTimeTicks

	// WaitMany behaves as if Wait were called on each handle/signal pair
	// simultaneously and completing when the first Wait would complete.
	// Notes about return values:
	//   |index| can be -1 if the error returned was not caused by a
	//       particular handle. For example, the error MOJO_RESULT_DEADLINE_EXCEEDED
	//       is not related to a particular handle.
	//   |states| can be nil if the signal array could not be returned. This can
	//       happen with errors such as MOJO_RESULT_INVALID_ARGUMENT.
	WaitMany(handles []Handle, signals []MojoHandleSignals, deadline MojoDeadline) (result MojoResult, index int, states []MojoHandleSignalsState)

	// CreateDataPipe creates a data pipe which is a unidirectional
	// communication channel for unframed data. On success, returns a
	// handle to the producer and consumer of the data pipe.
	CreateDataPipe(opts *DataPipeOptions) (MojoResult, ProducerHandle, ConsumerHandle)

	// CreateMessagePipe creates a message pipe which is a bidirectional
	// communication channel for framed data (i.e., messages). Messages
	// can contain plain data and/or Mojo handles. On success, it returns
	// handles to the two endpoints of the message pipe.
	CreateMessagePipe(opts *MessagePipeOptions) (MojoResult, MessagePipeHandle, MessagePipeHandle)

	// CreateSharedBuffer creates a buffer of size numBytes that can be
	// shared between applications. One must call MapBuffer to access
	// the buffer.
	CreateSharedBuffer(opts *SharedBufferOptions, numBytes uint64) (MojoResult, SharedBufferHandle)
}

// coreImpl is an implementation of the Mojo system APIs.
type coreImpl struct {
}

func GetCore() Core {
	return &core
}

func (impl *coreImpl) acquireCHandle(handle C.MojoHandle) UntypedHandle {
	return impl.AcquireNativeHandle(MojoHandle(handle))
}

func (impl *coreImpl) AcquireNativeHandle(handle MojoHandle) UntypedHandle {
	return &untypedHandleImpl{baseHandle{handle}}
}

func (impl *coreImpl) GetTimeTicksNow() MojoTimeTicks {
	return MojoTimeTicks(C.MojoGetTimeTicksNow())
}

func (impl *coreImpl) WaitMany(handles []Handle, signals []MojoHandleSignals, deadline MojoDeadline) (result MojoResult, index int, states []MojoHandleSignalsState) {
	l := len(handles)
	if l == 0 {
		result = MojoResult(C.MojoWaitMany(nil, nil, 0, deadline.cValue(), nil, nil))
		index = -1
		return
	}
	cParams := C.MallocWaitManyParams(C.uint32_t(l))
	defer C.FreeWaitManyParams(cParams)
	cHandles := *(*[]MojoHandle)(newUnsafeSlice(unsafe.Pointer(cParams.handles), l))
	cStates := *(*[]C.struct_MojoHandleSignalsState)(newUnsafeSlice(unsafe.Pointer(cParams.states), l))
	for i := 0; i < l; i++ {
		cHandles[i] = handles[i].NativeHandle()
		cStates[i] = C.struct_MojoHandleSignalsState{}
	}
	cSignals := *(*[]MojoHandleSignals)(newUnsafeSlice(unsafe.Pointer(cParams.signals), l))
	copy(cSignals, signals)
	// Set "-1" using the instructions from http://blog.golang.org/constants
	*cParams.index = ^C.uint32_t(0)

	result = MojoResult(C.MojoWaitMany(cParams.handles, cParams.signals, C.uint32_t(l), deadline.cValue(), cParams.index, cParams.states))
	// Explicitly convert *cParams.index to int32 type as int has 32 or 64 bits
	// depending on system configuration.
	index = int(int32(*cParams.index))
	if result != MOJO_RESULT_INVALID_ARGUMENT && result != MOJO_RESULT_RESOURCE_EXHAUSTED {
		for _, cState := range cStates {
			states = append(states, cState.goValue())
		}
	}
	return
}

func (impl *coreImpl) CreateDataPipe(opts *DataPipeOptions) (MojoResult, ProducerHandle, ConsumerHandle) {
	cParams := C.MallocCreateDataPipeParams()
	defer C.FreeCreateDataPipeParams(cParams)
	result := C.MojoCreateDataPipe(opts.cValue(cParams.opts), cParams.producer, cParams.consumer)
	producer := impl.acquireCHandle(*cParams.producer).ToProducerHandle()
	consumer := impl.acquireCHandle(*cParams.consumer).ToConsumerHandle()
	return MojoResult(result), producer, consumer
}

func (impl *coreImpl) CreateMessagePipe(opts *MessagePipeOptions) (MojoResult, MessagePipeHandle, MessagePipeHandle) {
	cParams := C.MallocCreateMessagePipeParams()
	defer C.FreeCreateMessagePipeParams(cParams)
	result := C.MojoCreateMessagePipe(opts.cValue(cParams.opts), cParams.handle0, cParams.handle1)
	handle0 := impl.acquireCHandle(*cParams.handle0).ToMessagePipeHandle()
	handle1 := impl.acquireCHandle(*cParams.handle1).ToMessagePipeHandle()
	return MojoResult(result), handle0, handle1
}

func (impl *coreImpl) CreateSharedBuffer(opts *SharedBufferOptions, numBytes uint64) (MojoResult, SharedBufferHandle) {
	cParams := C.MallocCreateSharedBufferParams()
	defer C.FreeCreateSharedBufferParams(cParams)
	result := C.MojoCreateSharedBuffer(opts.cValue(cParams.opts), C.uint64_t(numBytes), cParams.handle)
	return MojoResult(result), impl.acquireCHandle(*cParams.handle).ToSharedBufferHandle()
}

func newUnsafeSlice(ptr unsafe.Pointer, length int) unsafe.Pointer {
	header := &reflect.SliceHeader{
		Data: uintptr(ptr),
		Len:  length,
		Cap:  length,
	}
	return unsafe.Pointer(header)
}

func unsafeByteSlice(ptr unsafe.Pointer, length int) []byte {
	return *(*[]byte)(newUnsafeSlice(ptr, length))
}
