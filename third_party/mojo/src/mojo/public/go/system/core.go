// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package system

import (
	"unsafe"

	t "mojo/public/go/system/impl"
)

// Core is an interface that defines the set of Mojo system APIs.
type Core interface {
	// GetTimeTicksNow returns a monotonically increasing platform
	// dependent tick count representing "right now". Resolution
	// depends on the system configuration.
	GetTimeTicksNow() t.MojoTimeTicks

	// Close closes the given handle.
	Close(handle t.MojoHandle) (result t.MojoResult)

	// Wait waits on the given handle until a signal indicated by signals
	// is satisfied or it becomes known that no signal indicated by
	// signals will ever be satisfied or until deadline has passed.
	// Notes about return values:
	//   |state| can be nil if the signal array could not be returned. This can
	//       happen with errors such as MOJO_RESULT_INVALID_ARGUMENT.
	Wait(handle t.MojoHandle, signal t.MojoHandleSignals, deadline t.MojoDeadline) (result t.MojoResult, state *t.MojoHandleSignalsState)

	// WaitMany behaves as if Wait were called on each handle/signal pair
	// simultaneously and completing when the first Wait would complete.
	// Notes about return values:
	//   |index| can be nil if the error returned was not caused by a
	//       particular handle. For example, the error MOJO_RESULT_DEADLINE_EXCEEDED
  //       is not related to a particular handle.
	//   |state| can be nil if the signal array could not be returned. This can
	//       happen with errors such as MOJO_RESULT_INVALID_ARGUMENT.
	WaitMany(handles []t.MojoHandle, signals []t.MojoHandleSignals, deadline t.MojoDeadline) (result t.MojoResult, index *uint32, state []t.MojoHandleSignalsState)

	// CreateMessagePipe creates a message pipe which is a bidirectional
	// communication channel for framed data (i.e., messages). Messages
	// can contain plain data and/or Mojo handles. On success, it returns
	// handles to the two endpoints of the message pipe.
	CreateMessagePipe(opts *t.MessagePipeOptions) (result t.MojoResult, handle0 t.MojoHandle, handle1 t.MojoHandle)

	// WriteMessage writes message data and optional attached handles to
	// the message pipe endpoint given by handle. On success the attached
	// handles will no longer be valid (ie: the receiver will receive
	// equivalent but logically different handles).
	WriteMessage(handle t.MojoHandle, msg []byte, attached []t.MojoHandle, flags t.MojoWriteMessageFlags) (result t.MojoResult)

	// ReadMessage reads a message from the message pipe endpoint given
	// by handle with the specified flags. Returns the message data and
	// attached handles that were received and the number of bytes and
	// attached handles in the "next" message.
	ReadMessage(handle t.MojoHandle, flags t.MojoReadMessageFlags) (result t.MojoResult, msg []byte, attached []t.MojoHandle, numBytes uint32, numHandles uint32)

	// CreateDataPipe creates a data pipe which is a unidirectional
	// communication channel for unframed data. On success, returns a
	// handle to the producer and consumer of the data pipe.
	CreateDataPipe(opts *t.DataPipeOptions) (result t.MojoResult, producer t.MojoHandle, consumer t.MojoHandle)

	// WriteData writes data to the data pipe producer handle with the
	// given flags. On success, returns the number of bytes that were
	// actually written.
	WriteData(producer t.MojoHandle, data []byte, flags t.MojoWriteDataFlags) (result t.MojoResult, numBytes uint32)

	// ReadData reads data from the data pipe consumer handle with the
	// given flags. On success, returns the data that was read.
	ReadData(consumer t.MojoHandle, flags t.MojoReadDataFlags) (result t.MojoResult, data []byte)

	// CreateSharedBuffer creates a buffer of size numBytes that can be
	// shared between applications. One must call MapBuffer to access
	// the buffer.
	CreateSharedBuffer(opts *t.SharedBufferOptions, numBytes uint64) (result t.MojoResult, handle t.MojoHandle)

	// DuplicateBufferHandle duplicates the handle to a buffer.
	DuplicateBufferHandle(handle t.MojoHandle, opts *t.DuplicateBufferHandleOptions) (result t.MojoResult, duplicate t.MojoHandle)

	// MapBuffer maps the requested part of the shared buffer given by
	// handle into memory with specified flags. On success, it returns
	// a pointer to the requested shared buffer.
	MapBuffer(handle t.MojoHandle, offset uint64, numBytes uint64, flags t.MojoMapBufferFlags) (result t.MojoResult, buffer unsafe.Pointer)

	// UnmapBuffer unmaps a buffer pointer that was returned by MapBuffer.
	UnmapBuffer(buffer unsafe.Pointer) (result t.MojoResult)
}
