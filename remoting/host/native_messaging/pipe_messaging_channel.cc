// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/native_messaging/pipe_messaging_channel.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/values.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

namespace {

base::File DuplicatePlatformFile(base::File file) {
  base::PlatformFile result;
#if defined(OS_WIN)
  if (!DuplicateHandle(GetCurrentProcess(),
                       file.TakePlatformFile(),
                       GetCurrentProcess(),
                       &result,
                       0,
                       FALSE,
                       DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    PLOG(ERROR) << "Failed to duplicate handle " << file.GetPlatformFile();
    return base::File();
  }
  return base::File(result);
#elif defined(OS_POSIX)
  result = dup(file.GetPlatformFile());
  return base::File(result);
#else
#error Not implemented.
#endif
}

}  // namespace

namespace remoting {

PipeMessagingChannel::PipeMessagingChannel(
    base::File input,
    base::File output)
    : native_messaging_reader_(DuplicatePlatformFile(input.Pass())),
      native_messaging_writer_(new NativeMessagingWriter(
          DuplicatePlatformFile(output.Pass()))),
      event_handler_(NULL),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

PipeMessagingChannel::~PipeMessagingChannel() {
}

void PipeMessagingChannel::Start(EventHandler* event_handler) {
  DCHECK(CalledOnValidThread());
  DCHECK(!event_handler_);

  event_handler_ = event_handler;
  DCHECK(event_handler_);

  native_messaging_reader_.Start(
      base::Bind(&PipeMessagingChannel::ProcessMessage, weak_ptr_),
      base::Bind(&PipeMessagingChannel::Shutdown, weak_ptr_));
}

void PipeMessagingChannel::ProcessMessage(scoped_ptr<base::Value> message) {
  DCHECK(CalledOnValidThread());

  if (message->GetType() != base::Value::TYPE_DICTIONARY) {
    LOG(ERROR) << "Expected DictionaryValue";
    Shutdown();
    return;
  }

  if (event_handler_)
    event_handler_->OnMessage(message.Pass());
}

void PipeMessagingChannel::SendMessage(
    scoped_ptr<base::Value> message) {
  DCHECK(CalledOnValidThread());

  bool success = message && native_messaging_writer_;
  if (success)
    success = native_messaging_writer_->WriteMessage(*message);

  if (!success) {
    // Close the write pipe so no more responses will be sent.
    native_messaging_writer_.reset();
    Shutdown();
  }
}

void PipeMessagingChannel::Shutdown() {
  DCHECK(CalledOnValidThread());

  if (event_handler_) {
    // Set event_handler_ to NULL to indicate the object is in a shutdown cycle.
    // Since event_handler->OnDisconnect() will destroy the current object,
    // |event_handler_| will become a dangling pointer after OnDisconnect()
    // returns. Therefore, we set |event_handler_| to NULL beforehand.
    EventHandler* handler = event_handler_;
    event_handler_ = NULL;
    handler->OnDisconnect();
  }
}

}  // namespace remoting
