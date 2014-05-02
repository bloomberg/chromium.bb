// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/native_messaging/native_messaging_channel.h"

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

NativeMessagingChannel::NativeMessagingChannel(
    base::File input,
    base::File output)
    : native_messaging_reader_(DuplicatePlatformFile(input.Pass())),
      native_messaging_writer_(new NativeMessagingWriter(
          DuplicatePlatformFile(output.Pass()))),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

NativeMessagingChannel::~NativeMessagingChannel() {
}

void NativeMessagingChannel::Start(const SendMessageCallback& received_message,
                                   const base::Closure& quit_closure) {
  DCHECK(CalledOnValidThread());
  DCHECK(received_message_.is_null());
  DCHECK(quit_closure_.is_null());

  received_message_ = received_message;
  quit_closure_ = quit_closure;

  native_messaging_reader_.Start(
      base::Bind(&NativeMessagingChannel::ProcessMessage, weak_ptr_),
      base::Bind(&NativeMessagingChannel::Shutdown, weak_ptr_));
}

void NativeMessagingChannel::ProcessMessage(scoped_ptr<base::Value> message) {
  DCHECK(CalledOnValidThread());

  if (message->GetType() != base::Value::TYPE_DICTIONARY) {
    LOG(ERROR) << "Expected DictionaryValue";
    Shutdown();
    return;
  }

  scoped_ptr<base::DictionaryValue> message_dict(
      static_cast<base::DictionaryValue*>(message.release()));
  received_message_.Run(message_dict.Pass());
}

void NativeMessagingChannel::SendMessage(
    scoped_ptr<base::DictionaryValue> message) {
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

void NativeMessagingChannel::Shutdown() {
  DCHECK(CalledOnValidThread());

  if (!quit_closure_.is_null())
    base::ResetAndReturn(&quit_closure_).Run();
}

}  // namespace remoting
