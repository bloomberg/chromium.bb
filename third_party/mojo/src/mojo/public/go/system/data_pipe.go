// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package system

//#include "c_allocators.h"
//#include "mojo/public/c/system/core.h"
import "C"
import "unsafe"

// ConsumerHandle is a handle for the consumer part of a data pipe.
type ConsumerHandle interface {
	Handle

	// ReadData reads data from the data pipe consumer handle with the
	// given flags. On success, returns the data that was read.
	ReadData(flags MojoReadDataFlags) (MojoResult, []byte)
}

// ProducerHandle is a handle for the producer part of a data pipe.
type ProducerHandle interface {
	Handle

	// WriteData writes data to the data pipe producer handle with the
	// given flags. On success, returns the number of bytes that were
	// actually written.
	WriteData(data []byte, flags MojoWriteDataFlags) (MojoResult, int)
}

type dataPipeConsumer struct {
	baseHandle
}

func (h *dataPipeConsumer) ReadData(flags MojoReadDataFlags) (MojoResult, []byte) {
	cParams := C.MallocReadDataParams()
	defer C.FreeReadDataParams(cParams)
	*cParams.num_bytes = 0
	if result := C.MojoReadData(h.mojoHandle.cValue(), nil, cParams.num_bytes, C.MOJO_READ_DATA_FLAG_QUERY); result != C.MOJO_RESULT_OK {
		return MojoResult(result), nil
	}
	dataArray := C.MallocDataArray(*cParams.num_bytes)
	defer C.FreeDataArray(dataArray)
	result := C.MojoReadData(h.mojoHandle.cValue(), dataArray.elements, cParams.num_bytes, flags.cValue())
	dataSize := int(*cParams.num_bytes)
	data := make([]byte, dataSize)
	cData := unsafeByteSlice(unsafe.Pointer(dataArray.elements), dataSize)
	copy(data, cData)
	return MojoResult(result), data
}

type dataPipeProducer struct {
	baseHandle
}

func (h *dataPipeProducer) WriteData(data []byte, flags MojoWriteDataFlags) (MojoResult, int) {
	cParams := C.MallocWriteDataParams(C.uint32_t(len(data)))
	defer C.FreeWriteDataParams(cParams)
	*cParams.num_bytes = C.uint32_t(len(data))
	cArray := unsafeByteSlice(unsafe.Pointer(cParams.elements), len(data))
	copy(cArray, data)
	result := C.MojoWriteData(h.mojoHandle.cValue(), cParams.elements, cParams.num_bytes, flags.cValue())
	return MojoResult(result), int(*cParams.num_bytes)
}
