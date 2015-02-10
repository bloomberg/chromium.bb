// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package system

//#include "c_allocators.h"
//#include "mojo/public/c/system/core.h"
import "C"
import "unsafe"

// SharedBufferHandle is a handle for a buffer that can be shared between
// applications.
type SharedBufferHandle interface {
	Handle

	// DuplicateBufferHandle duplicates the handle to a buffer.
	DuplicateBufferHandle(opts *DuplicateBufferHandleOptions) (MojoResult, SharedBufferHandle)

	// MapBuffer maps the requested part of the shared buffer given by handle
	// into memory with specified flags. On success, it returns slice that
	// points to the requested shared buffer.
	MapBuffer(offset uint64, numBytes int, flags MojoMapBufferFlags) (MojoResult, []byte)

	// UnmapBuffer unmaps a buffer that was returned by MapBuffer.
	UnmapBuffer(buffer []byte) MojoResult
}

type sharedBuffer struct {
	baseHandle
}

func (h *sharedBuffer) DuplicateBufferHandle(opts *DuplicateBufferHandleOptions) (MojoResult, SharedBufferHandle) {
	h.core.mu.Lock()
	defer h.core.mu.Unlock()

	cParams := C.MallocDuplicateBufferHandleParams()
	defer C.FreeDuplicateBufferHandleParams(cParams)
	result := C.MojoDuplicateBufferHandle(h.mojoHandle.cValue(), opts.cValue(cParams.opts), cParams.duplicate)
	return MojoResult(result), core.acquireCHandle(*cParams.duplicate).ToSharedBufferHandle()
}

func (h *sharedBuffer) MapBuffer(offset uint64, numBytes int, flags MojoMapBufferFlags) (MojoResult, []byte) {
	h.core.mu.Lock()
	defer h.core.mu.Unlock()

	cParams := C.MallocMapBufferParams()
	defer C.FreeMapBufferParams(cParams)
	result := C.MojoMapBuffer(h.mojoHandle.cValue(), C.uint64_t(offset), C.uint64_t(numBytes), cParams.buffer, flags.cValue())
	if result != C.MOJO_RESULT_OK {
		return MojoResult(result), nil
	}
	return MOJO_RESULT_OK, unsafeByteSlice(unsafe.Pointer(*cParams.buffer), numBytes)
}

func (h *sharedBuffer) UnmapBuffer(buffer []byte) MojoResult {
	h.core.mu.Lock()
	defer h.core.mu.Unlock()

	return MojoResult(C.MojoUnmapBuffer(unsafe.Pointer(&buffer[0])))
}
