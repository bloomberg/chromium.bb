// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/clipboard_aura.h"

#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "content/public/browser/browser_thread.h"
#include "remoting/base/constants.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace {

// Clipboard polling interval in milliseconds.
const int64 kClipboardPollingIntervalMs = 500;

}  // namespace

namespace remoting {

class ClipboardAura::Core {
 public:
  Core();

  // Mirror the public interface.
  void Start(scoped_ptr<protocol::ClipboardStub> client_clipboard);
  void InjectClipboardEvent(const protocol::ClipboardEvent& event);
  void Stop();

  // Overrides the clipboard polling interval for unit test.
  void SetPollingIntervalForTesting(base::TimeDelta polling_interval);

 private:
  void CheckClipboardForChanges();

  scoped_ptr<protocol::ClipboardStub> client_clipboard_;
  scoped_ptr<base::RepeatingTimer<Core>> clipboard_polling_timer_;
  uint64 current_change_count_;
  base::TimeDelta polling_interval_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

ClipboardAura::ClipboardAura(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : core_(new Core()),
      ui_task_runner_(ui_task_runner) {
}

ClipboardAura::~ClipboardAura() {
  ui_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void ClipboardAura::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Core::Start, base::Unretained(core_.get()),
                            base::Passed(&client_clipboard)));
}

void ClipboardAura::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&Core::InjectClipboardEvent,
                                       base::Unretained(core_.get()), event));
}

void ClipboardAura::Stop() {
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Core::Stop, base::Unretained(core_.get())));
};

void ClipboardAura::SetPollingIntervalForTesting(
    base::TimeDelta polling_interval) {
  core_->SetPollingIntervalForTesting(polling_interval);
}

ClipboardAura::Core::Core()
    : current_change_count_(0),
      polling_interval_(
          base::TimeDelta::FromMilliseconds(kClipboardPollingIntervalMs)) {
}

void ClipboardAura::Core::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  client_clipboard_.reset(client_clipboard.release());

  // Aura doesn't provide a clipboard-changed notification. The only way to
  // detect clipboard changes is by polling.
  clipboard_polling_timer_.reset(new base::RepeatingTimer<Core>());
  clipboard_polling_timer_->Start(
      FROM_HERE, polling_interval_, this,
      &ClipboardAura::Core::CheckClipboardForChanges);
}

void ClipboardAura::Core::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  // Currently we only handle UTF-8 text.
  if (event.mime_type().compare(kMimeTypeTextUtf8) != 0) {
    return;
  }

  ui::ScopedClipboardWriter clipboard_writer(ui::CLIPBOARD_TYPE_COPY_PASTE);
  clipboard_writer.WriteText(base::UTF8ToUTF16(event.data()));

  // Update local change-count to prevent this change from being picked up by
  // CheckClipboardForChanges.
  current_change_count_++;
}

void ClipboardAura::Core::Stop() {
  clipboard_polling_timer_.reset();
  client_clipboard_.reset();
}

void ClipboardAura::Core::SetPollingIntervalForTesting(
    base::TimeDelta polling_interval) {
  polling_interval_ = polling_interval;
}

void ClipboardAura::Core::CheckClipboardForChanges() {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  uint64 change_count =
      clipboard->GetSequenceNumber(ui::CLIPBOARD_TYPE_COPY_PASTE);

  if (change_count == current_change_count_) {
    return;
  }

  current_change_count_ = change_count;

  protocol::ClipboardEvent event;
  std::string data;

  clipboard->ReadAsciiText(ui::CLIPBOARD_TYPE_COPY_PASTE, &data);
  event.set_mime_type(kMimeTypeTextUtf8);
  event.set_data(data);

  client_clipboard_->InjectClipboardEvent(event);
}

scoped_ptr<Clipboard> Clipboard::Create() {
  return make_scoped_ptr(
      new ClipboardAura(content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI)));
}

}  // namespace remoting
