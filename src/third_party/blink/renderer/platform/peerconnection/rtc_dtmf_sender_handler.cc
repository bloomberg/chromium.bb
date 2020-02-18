// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_dtmf_sender_handler.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using webrtc::DtmfSenderInterface;

namespace blink {

std::unique_ptr<WebRTCDTMFSenderHandler> CreateRTCDTMFSenderHandler(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    webrtc::DtmfSenderInterface* dtmf_sender) {
  return std::make_unique<RtcDtmfSenderHandler>(std::move(main_thread),
                                                dtmf_sender);
}

class RtcDtmfSenderHandler::Observer
    : public base::RefCountedThreadSafe<Observer>,
      public webrtc::DtmfSenderObserverInterface {
 public:
  explicit Observer(scoped_refptr<base::SingleThreadTaskRunner> main_thread,
                    const base::WeakPtr<RtcDtmfSenderHandler>& handler)
      : main_thread_(std::move(main_thread)), handler_(handler) {}

 private:
  friend class base::RefCountedThreadSafe<Observer>;

  ~Observer() override {}

  void OnToneChange(const std::string& tone) override {
    PostCrossThreadTask(*main_thread_.get(), FROM_HERE,
                        CrossThreadBindOnce(&Observer::OnToneChangeOnMainThread,
                                            scoped_refptr<Observer>(this),
                                            String(tone.data())));
  }

  void OnToneChangeOnMainThread(const String& tone) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (handler_)
      handler_->OnToneChange(tone);
  }

  THREAD_CHECKER(thread_checker_);
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  base::WeakPtr<RtcDtmfSenderHandler> handler_;
};

RtcDtmfSenderHandler::RtcDtmfSenderHandler(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    DtmfSenderInterface* dtmf_sender)
    : dtmf_sender_(dtmf_sender), webkit_client_(nullptr) {
  DVLOG(1) << "::ctor";
  observer_ = new Observer(std::move(main_thread), weak_factory_.GetWeakPtr());
  dtmf_sender_->RegisterObserver(observer_.get());
}

RtcDtmfSenderHandler::~RtcDtmfSenderHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "::dtor";
  dtmf_sender_->UnregisterObserver();
  // Release |observer| before |weak_factory_| is destroyed.
  observer_ = nullptr;
}

void RtcDtmfSenderHandler::SetClient(
    blink::WebRTCDTMFSenderHandlerClient* client) {
  webkit_client_ = client;
}

blink::WebString RtcDtmfSenderHandler::CurrentToneBuffer() {
  return blink::WebString::FromUTF8(dtmf_sender_->tones());
}

bool RtcDtmfSenderHandler::CanInsertDTMF() {
  return dtmf_sender_->CanInsertDtmf();
}

bool RtcDtmfSenderHandler::InsertDTMF(const blink::WebString& tones,
                                      int duration,
                                      int interToneGap) {
  std::string utf8_tones = tones.Utf8();
  return dtmf_sender_->InsertDtmf(utf8_tones, duration, interToneGap);
}

void RtcDtmfSenderHandler::OnToneChange(const String& tone) {
  if (!webkit_client_) {
    LOG(ERROR) << "WebRTCDTMFSenderHandlerClient not set.";
    return;
  }
  webkit_client_->DidPlayTone(tone);
}

}  // namespace blink
