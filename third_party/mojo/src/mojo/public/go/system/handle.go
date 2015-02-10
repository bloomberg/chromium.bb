// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package system

//#include "c_allocators.h"
//#include "mojo/public/c/system/core.h"
import "C"

// Handle is a generic handle for mojo objects.
type Handle interface {
	// Close closes the given handle.
	Close() MojoResult

	// IsValid returns whether the handle is valid. A handle is valid until it
	// has been explicitly closed or sent through a message pipe.
	IsValid() bool

	// NativeHandle returns the native handle backed by this handle.
	//
	// Note: try to avoid using this method as you lose ownership tracking.
	NativeHandle() MojoHandle

	// ReleaseNativeHandle releases the native handle backed by this handle.
	// The caller owns the handle and must close it.
	ReleaseNativeHandle() MojoHandle

	// ToUntypedHandle converts this handle into an UntypedHandle, invalidating
	// this handle.
	ToUntypedHandle() UntypedHandle

	// Wait waits on the handle until a signal indicated by signals is satisfied
	// or it becomes known that no signal indicated by signals will ever be
	// satisified or until deadline has passed.
	Wait(signals MojoHandleSignals, deadline MojoDeadline) (MojoResult, MojoHandleSignalsState)
}

// UntypedHandle is a a mojo handle of unknown type. This handle can be typed by
// using one of its methods, which will return a handle of the requested type
// and invalidate this object. No validation is made when the conversion
// operation is called.
type UntypedHandle interface {
	Handle

	// ToConsumerHandle returns the underlying handle as a ConsumerHandle
	// and invalidates this UntypedHandle representation.
	ToConsumerHandle() ConsumerHandle

	// ToProducerHandle returns the underlying handle as a ProducerHandle
	// and invalidates this UntypedHandle representation.
	ToProducerHandle() ProducerHandle

	// ToMessagePipeHandle returns the underlying handle as a MessagePipeHandle
	// and invalidates this UntypedHandle representation.
	ToMessagePipeHandle() MessagePipeHandle

	// ToSharedBufferHandle returns the underlying handle as a
	// SharedBufferHandle and invalidates this UntypedHandle representation.
	ToSharedBufferHandle() SharedBufferHandle
}

type baseHandle struct {
	core       *coreImpl
	mojoHandle MojoHandle
}

func (h *baseHandle) invalidate() {
	h.mojoHandle = MOJO_HANDLE_INVALID
}

func (h *baseHandle) Close() MojoResult {
	h.core.mu.Lock()
	defer h.core.mu.Unlock()

	mojoHandle := h.mojoHandle
	h.invalidate()
	return MojoResult(C.MojoClose(mojoHandle.cValue()))
}

func (h *baseHandle) IsValid() bool {
	return h.mojoHandle != MOJO_HANDLE_INVALID
}

func (h *baseHandle) NativeHandle() MojoHandle {
	return h.mojoHandle
}

func (h *baseHandle) ReleaseNativeHandle() MojoHandle {
	mojoHandle := h.mojoHandle
	h.invalidate()
	return mojoHandle
}

func (h *baseHandle) ToUntypedHandle() UntypedHandle {
	handle := &untypedHandleImpl{*h}
	h.invalidate()
	return handle
}

func (h *baseHandle) Wait(signals MojoHandleSignals, deadline MojoDeadline) (MojoResult, MojoHandleSignalsState) {
	cParams := C.MallocWaitParams()
	defer C.FreeWaitParams(cParams)
	*cParams.state = C.struct_MojoHandleSignalsState{}
	result := C.MojoWait(h.mojoHandle.cValue(), signals.cValue(), deadline.cValue(), cParams.state)
	return MojoResult(result), cParams.state.goValue()
}

type untypedHandleImpl struct {
	baseHandle
}

func (h *untypedHandleImpl) ToConsumerHandle() ConsumerHandle {
	handle := &dataPipeConsumer{h.baseHandle}
	h.invalidate()
	return handle
}

func (h *untypedHandleImpl) ToProducerHandle() ProducerHandle {
	handle := &dataPipeProducer{h.baseHandle}
	h.invalidate()
	return handle
}

func (h *untypedHandleImpl) ToMessagePipeHandle() MessagePipeHandle {
	handle := &messagePipe{h.baseHandle}
	h.invalidate()
	return handle
}

func (h *untypedHandleImpl) ToSharedBufferHandle() SharedBufferHandle {
	handle := &sharedBuffer{h.baseHandle}
	h.invalidate()
	return handle
}
