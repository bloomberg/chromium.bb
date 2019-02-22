// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/fuchsia/fidlgen_js/runtime/zircon.h"

#include <lib/zx/channel.h>
#include <zircon/errors.h>
#include <zircon/syscalls.h>
#include <zircon/types.h>

#include "base/bind.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/function_template.h"

namespace {

v8::Local<v8::Object> ZxChannelCreate(v8::Isolate* isolate) {
  zx::channel c1, c2;
  zx_status_t status = zx::channel::create(0, &c1, &c2);
  return gin::DataObjectBuilder(isolate)
      .Set("status", status)
      .Set("first", c1.release())
      .Set("second", c2.release())
      .Build();
}

zx_status_t ZxChannelWrite(gin::Arguments* args) {
  zx_handle_t handle;
  if (!args->GetNext(&handle)) {
    args->ThrowError();
    return ZX_ERR_INVALID_ARGS;
  }

  gin::ArrayBufferView data;
  if (!args->GetNext(&data)) {
    args->ThrowError();
    return ZX_ERR_INVALID_ARGS;
  }

  std::vector<zx_handle_t> handles;
  if (!args->GetNext(&handles)) {
    args->ThrowError();
    return ZX_ERR_INVALID_ARGS;
  }

  zx_status_t status =
      zx_channel_write(handle, 0, data.bytes(), data.num_bytes(),
                       handles.data(), handles.size());
  return status;
}

v8::Local<v8::Value> StrToUtf8Array(gin::Arguments* args) {
  std::string str;
  // This converts the string to utf8 from ucs2, so then just repackage the
  // string as an array and return it.
  if (!args->GetNext(&str)) {
    args->ThrowError();
    return v8::Local<v8::Object>();
  }

  // TODO(crbug.com/883496): Not sure how to make a Uint8Array to return here
  // which would be a bit more efficient.
  std::vector<int> data;
  std::copy(str.begin(), str.end(), std::back_inserter(data));
  return gin::ConvertToV8(args->isolate(), data);
}

v8::Local<v8::Object> GetOrCreateZxObject(v8::Isolate* isolate,
                                          v8::Local<v8::Object> global) {
  v8::Local<v8::Object> zx;
  v8::Local<v8::Value> zx_value = global->Get(gin::StringToV8(isolate, "zx"));
  if (zx_value.IsEmpty() || !zx_value->IsObject()) {
    zx = v8::Object::New(isolate);
    global->Set(gin::StringToSymbol(isolate, "zx"), zx);
  } else {
    zx = v8::Local<v8::Object>::Cast(zx);
  }
  return zx;
}

}  // namespace

namespace fidljs {

void InjectZxBindings(v8::Isolate* isolate, v8::Local<v8::Object> global) {
  v8::Local<v8::Object> zx = GetOrCreateZxObject(isolate, global);

#define SET_CONSTANT(k) \
  zx->Set(gin::StringToSymbol(isolate, #k), gin::ConvertToV8(isolate, k))

  // zx_status_t.
  SET_CONSTANT(ZX_OK);
  SET_CONSTANT(ZX_ERR_INTERNAL);
  SET_CONSTANT(ZX_ERR_NOT_SUPPORTED);
  SET_CONSTANT(ZX_ERR_NO_RESOURCES);
  SET_CONSTANT(ZX_ERR_NO_MEMORY);
  SET_CONSTANT(ZX_ERR_INTERNAL_INTR_RETRY);
  SET_CONSTANT(ZX_ERR_INVALID_ARGS);
  SET_CONSTANT(ZX_ERR_BAD_HANDLE);
  SET_CONSTANT(ZX_ERR_WRONG_TYPE);
  SET_CONSTANT(ZX_ERR_BAD_SYSCALL);
  SET_CONSTANT(ZX_ERR_OUT_OF_RANGE);
  SET_CONSTANT(ZX_ERR_BUFFER_TOO_SMALL);
  SET_CONSTANT(ZX_ERR_BAD_STATE);
  SET_CONSTANT(ZX_ERR_TIMED_OUT);
  SET_CONSTANT(ZX_ERR_SHOULD_WAIT);
  SET_CONSTANT(ZX_ERR_CANCELED);
  SET_CONSTANT(ZX_ERR_PEER_CLOSED);
  SET_CONSTANT(ZX_ERR_NOT_FOUND);
  SET_CONSTANT(ZX_ERR_ALREADY_EXISTS);
  SET_CONSTANT(ZX_ERR_ALREADY_BOUND);
  SET_CONSTANT(ZX_ERR_UNAVAILABLE);
  SET_CONSTANT(ZX_ERR_ACCESS_DENIED);
  SET_CONSTANT(ZX_ERR_IO);
  SET_CONSTANT(ZX_ERR_IO_REFUSED);
  SET_CONSTANT(ZX_ERR_IO_DATA_INTEGRITY);
  SET_CONSTANT(ZX_ERR_IO_DATA_LOSS);
  SET_CONSTANT(ZX_ERR_IO_NOT_PRESENT);
  SET_CONSTANT(ZX_ERR_IO_OVERRUN);
  SET_CONSTANT(ZX_ERR_IO_MISSED_DEADLINE);
  SET_CONSTANT(ZX_ERR_IO_INVALID);
  SET_CONSTANT(ZX_ERR_BAD_PATH);
  SET_CONSTANT(ZX_ERR_NOT_DIR);
  SET_CONSTANT(ZX_ERR_NOT_FILE);
  SET_CONSTANT(ZX_ERR_FILE_BIG);
  SET_CONSTANT(ZX_ERR_NO_SPACE);
  SET_CONSTANT(ZX_ERR_NOT_EMPTY);
  SET_CONSTANT(ZX_ERR_STOP);
  SET_CONSTANT(ZX_ERR_NEXT);
  SET_CONSTANT(ZX_ERR_ASYNC);
  SET_CONSTANT(ZX_ERR_PROTOCOL_NOT_SUPPORTED);
  SET_CONSTANT(ZX_ERR_ADDRESS_UNREACHABLE);
  SET_CONSTANT(ZX_ERR_ADDRESS_IN_USE);
  SET_CONSTANT(ZX_ERR_NOT_CONNECTED);
  SET_CONSTANT(ZX_ERR_CONNECTION_REFUSED);
  SET_CONSTANT(ZX_ERR_CONNECTION_RESET);
  SET_CONSTANT(ZX_ERR_CONNECTION_ABORTED);

  // Handle APIs.
  zx->Set(gin::StringToSymbol(isolate, "handleClose"),
          gin::CreateFunctionTemplate(isolate,
                                      base::BindRepeating(&zx_handle_close))
              ->GetFunction());
  SET_CONSTANT(ZX_HANDLE_INVALID);

  // Channel APIs.
  zx->Set(gin::StringToSymbol(isolate, "channelCreate"),
          gin::CreateFunctionTemplate(isolate,
                                      base::BindRepeating(&ZxChannelCreate))
              ->GetFunction());
  zx->Set(
      gin::StringToSymbol(isolate, "channelWrite"),
      gin::CreateFunctionTemplate(isolate, base::BindRepeating(&ZxChannelWrite))
          ->GetFunction());
  SET_CONSTANT(ZX_CHANNEL_READABLE);
  SET_CONSTANT(ZX_CHANNEL_WRITABLE);
  SET_CONSTANT(ZX_CHANNEL_PEER_CLOSED);
  SET_CONSTANT(ZX_CHANNEL_READ_MAY_DISCARD);
  SET_CONSTANT(ZX_CHANNEL_MAX_MSG_BYTES);
  SET_CONSTANT(ZX_CHANNEL_MAX_MSG_HANDLES);

  // Utility to make string handling easier to convert from a UCS2 JS string to
  // an array of UTF-8 (which is how strings are represented in FIDL).
  // TODO(crbug.com/883496): This is not really zx, should move to a generic
  // runtime helper file if there are more similar C++ helpers required.
  zx->Set(
      gin::StringToSymbol(isolate, "strToUtf8Array"),
      gin::CreateFunctionTemplate(isolate, base::BindRepeating(&StrToUtf8Array))
          ->GetFunction());

#undef SET_CONSTANT
}

}  // namespace fidljs
