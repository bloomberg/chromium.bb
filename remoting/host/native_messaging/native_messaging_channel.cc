// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/native_messaging/native_messaging_channel.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/values.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

namespace {

base::PlatformFile DuplicatePlatformFile(base::PlatformFile handle) {
  base::PlatformFile result;
#if defined(OS_WIN)
  if (!DuplicateHandle(GetCurrentProcess(),
                       handle,
                       GetCurrentProcess(),
                       &result,
                       0,
                       FALSE,
                       DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    PLOG(ERROR) << "Failed to duplicate handle " << handle;
    return base::kInvalidPlatformFileValue;
  }
  return result;
#elif defined(OS_POSIX)
  result = dup(handle);
  base::ClosePlatformFile(handle);
  return result;
#else
#error Not implemented.
#endif
}

}  // namespace

namespace remoting {

NativeMessagingChannel::NativeMessagingChannel(
    scoped_ptr<Delegate> delegate,
    base::PlatformFile input,
    base::PlatformFile output)
    : native_messaging_reader_(DuplicatePlatformFile(input)),
      native_messaging_writer_(new NativeMessagingWriter(
          DuplicatePlatformFile(output))),
      delegate_(delegate.Pass()),
      pending_requests_(0),
      shutdown_(false),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

NativeMessagingChannel::~NativeMessagingChannel() {
}

void NativeMessagingChannel::Start(const base::Closure& quit_closure) {
  DCHECK(CalledOnValidThread());
  DCHECK(quit_closure_.is_null());
  DCHECK(!quit_closure.is_null());

  quit_closure_ = quit_closure;
  native_messaging_reader_.Start(
      base::Bind(&NativeMessagingChannel::ProcessMessage, weak_ptr_),
      base::Bind(&NativeMessagingChannel::Shutdown, weak_ptr_));
}

void NativeMessagingChannel::ProcessMessage(scoped_ptr<base::Value> message) {
  DCHECK(CalledOnValidThread());

  // Don't process any more messages if Shutdown() has been called.
  if (shutdown_)
    return;

  if (message->GetType() != base::Value::TYPE_DICTIONARY) {
    LOG(ERROR) << "Expected DictionaryValue";
    Shutdown();
    return;
  }

  DCHECK_GE(pending_requests_, 0);
  pending_requests_++;

  scoped_ptr<base::DictionaryValue> message_dict(
      static_cast<base::DictionaryValue*>(message.release()));
  delegate_->ProcessMessage(
      message_dict.Pass(),
      base::Bind(&NativeMessagingChannel::SendResponse, weak_ptr_));
}

void NativeMessagingChannel::SendResponse(
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(CalledOnValidThread());

  bool success = response && native_messaging_writer_;
  if (success)
    success = native_messaging_writer_->WriteMessage(*response);

  if (!success) {
    // Close the write pipe so no more responses will be sent.
    native_messaging_writer_.reset();
    Shutdown();
  }

  pending_requests_--;
  DCHECK_GE(pending_requests_, 0);

  if (shutdown_ && !pending_requests_)
    quit_closure_.Run();
}

void NativeMessagingChannel::Shutdown() {
  DCHECK(CalledOnValidThread());

  if (shutdown_)
    return;

  shutdown_ = true;
  if (!pending_requests_)
    quit_closure_.Run();
}

}  // namespace remoting
