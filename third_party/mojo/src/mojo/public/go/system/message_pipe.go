// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package system

//#include "c_allocators.h"
//#include "mojo/public/c/system/core.h"
import "C"
import "unsafe"

// MessagePipeHandle is a handle for a bidirectional communication channel for
// framed data (i.e., messages). Messages can contain plain data and/or Mojo
// handles.
type MessagePipeHandle interface {
	Handle

	// ReadMessage reads a message from the message pipe endpoint with the
	// specified flags. Returns the message data and attached handles that were
	// received in the "next" message.
	ReadMessage(flags MojoReadMessageFlags) (MojoResult, []byte, []UntypedHandle)

	// WriteMessage writes message data and optional attached handles to
	// the message pipe endpoint given by handle. On success the attached
	// handles will no longer be valid (i.e.: the receiver will receive
	// equivalent but logically different handles).
	WriteMessage(bytes []byte, handles []UntypedHandle, flags MojoWriteMessageFlags) MojoResult
}

type messagePipe struct {
	baseHandle
}

func (h *messagePipe) ReadMessage(flags MojoReadMessageFlags) (MojoResult, []byte, []UntypedHandle) {
	h.core.mu.Lock()
	defer h.core.mu.Unlock()

	cParams := C.MallocReadMessageParams()
	defer C.FreeReadMessageParams(cParams)
	*cParams.num_bytes = 0
	*cParams.num_handles = 0
	if result := C.MojoReadMessage(h.mojoHandle.cValue(), nil, cParams.num_bytes, nil, cParams.num_handles, flags.cValue()); result != C.MOJO_RESULT_RESOURCE_EXHAUSTED {
		return MojoResult(result), nil, nil
	}
	cArrays := C.MallocMessageArrays(*cParams.num_bytes, *cParams.num_handles)
	defer C.FreeMessageArrays(cArrays)
	result := C.MojoReadMessage(h.mojoHandle.cValue(), cArrays.bytes, cParams.num_bytes, cArrays.handles, cParams.num_handles, C.MojoReadMessageFlags(flags))

	bytesLen := int(*cParams.num_bytes)
	cBytes := *(*[]byte)(newUnsafeSlice(unsafe.Pointer(cArrays.bytes), bytesLen))
	bytes := make([]byte, bytesLen)
	copy(bytes, cBytes)

	handlesLen := int(*cParams.num_handles)
	cHandles := *(*[]MojoHandle)(newUnsafeSlice(unsafe.Pointer(cArrays.handles), handlesLen))
	handles := []UntypedHandle{}
	for i := 0; i < handlesLen; i++ {
		handles = append(handles, h.core.AcquireNativeHandle(cHandles[i]))
	}
	return MojoResult(result), bytes, handles
}

func (h *messagePipe) WriteMessage(bytes []byte, handles []UntypedHandle, flags MojoWriteMessageFlags) MojoResult {
	h.core.mu.Lock()
	defer h.core.mu.Unlock()

	cArrays := C.MallocMessageArrays(C.uint32_t(len(bytes)), C.uint32_t(len(handles)))
	defer C.FreeMessageArrays(cArrays)
	cBytes := *(*[]byte)(newUnsafeSlice(unsafe.Pointer(cArrays.bytes), len(bytes)))
	copy(cBytes, bytes)

	cHandles := *(*[]MojoHandle)(newUnsafeSlice(unsafe.Pointer(cArrays.handles), len(handles)))
	for i := 0; i < len(handles); i++ {
		cHandles[i] = handles[i].ReleaseNativeHandle()
	}
	return MojoResult(C.MojoWriteMessage(h.mojoHandle.cValue(), cArrays.bytes, C.uint32_t(len(bytes)), cArrays.handles, C.uint32_t(len(handles)), flags.cValue()))
}
