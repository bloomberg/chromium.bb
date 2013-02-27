// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/clipboard.h"

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/sys_string_conversions.h"
#include "base/timer.h"
#include "remoting/base/constants.h"
#include "remoting/base/util.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"

namespace {

// Clipboard polling interval in milliseconds.
const int64 kClipboardPollingIntervalMs = 500;

} // namespace

namespace remoting {

class ClipboardMac : public Clipboard {
 public:
  ClipboardMac();
  virtual ~ClipboardMac();

  // Must be called on the UI thread.
  virtual void Start(
      scoped_ptr<protocol::ClipboardStub> client_clipboard) OVERRIDE;
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  void CheckClipboardForChanges();

  scoped_ptr<protocol::ClipboardStub> client_clipboard_;
  scoped_ptr<base::RepeatingTimer<ClipboardMac> > clipboard_polling_timer_;
  NSInteger current_change_count_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardMac);
};

ClipboardMac::ClipboardMac() : current_change_count_(0) {
}

ClipboardMac::~ClipboardMac() {
  // In it2me the destructor is not called in the same thread that the timer is
  // created. Thus the timer must have already been destroyed by now.
  DCHECK(clipboard_polling_timer_.get() == NULL);
}

void ClipboardMac::Start(scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  client_clipboard_.reset(client_clipboard.release());

  // Synchronize local change-count with the pasteboard's. The change-count is
  // used to detect clipboard changes.
  current_change_count_ = [[NSPasteboard generalPasteboard] changeCount];

  // OS X doesn't provide a clipboard-changed notification. The only way to
  // detect clipboard changes is by polling.
  clipboard_polling_timer_.reset(new base::RepeatingTimer<ClipboardMac>());
  clipboard_polling_timer_->Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kClipboardPollingIntervalMs),
      this, &ClipboardMac::CheckClipboardForChanges);
}

void ClipboardMac::InjectClipboardEvent(const protocol::ClipboardEvent& event) {
  // Currently we only handle UTF-8 text.
  if (event.mime_type().compare(kMimeTypeTextUtf8) != 0)
    return;
  if (!StringIsUtf8(event.data().c_str(), event.data().length())) {
    LOG(ERROR) << "ClipboardEvent data is not UTF-8 encoded.";
    return;
  }

  // Write UTF-8 text to clipboard.
  NSString* text = base::SysUTF8ToNSString(event.data());
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  [pasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType]
                     owner:nil];
  [pasteboard setString:text forType:NSStringPboardType];

  // Update local change-count to prevent this change from being picked up by
  // CheckClipboardForChanges.
  current_change_count_ = [[NSPasteboard generalPasteboard] changeCount];
}

void ClipboardMac::Stop() {
  clipboard_polling_timer_.reset();
  client_clipboard_.reset();
}

void ClipboardMac::CheckClipboardForChanges() {
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  NSInteger change_count = [pasteboard changeCount];
  if (change_count == current_change_count_) {
    return;
  }
  current_change_count_ = change_count;

  NSString* data = [pasteboard stringForType:NSStringPboardType];
  if (data == nil) {
    return;
  }

  protocol::ClipboardEvent event;
  event.set_mime_type(kMimeTypeTextUtf8);
  event.set_data(base::SysNSStringToUTF8(data));
  client_clipboard_->InjectClipboardEvent(event);
}

scoped_ptr<Clipboard> Clipboard::Create() {
  return scoped_ptr<Clipboard>(new ClipboardMac());
}

}  // namespace remoting
