// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_responder.h"

#include "discovery/mdns/mdns_publisher.h"
#include "discovery/mdns/mdns_querier.h"
#include "discovery/mdns/mdns_random.h"
#include "discovery/mdns/mdns_receiver.h"
#include "discovery/mdns/mdns_sender.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {

MdnsResponder::MdnsResponder(RecordHandler* record_handler,
                             MdnsSender* sender,
                             MdnsReceiver* receiver,
                             MdnsQuerier* querier,
                             platform::TaskRunner* task_runner,
                             platform::ClockNowFunctionPtr now_function,
                             MdnsRandom* random_delay)
    : record_handler_(record_handler),
      sender_(sender),
      receiver_(receiver),
      querier_(querier),
      task_runner_(task_runner),
      now_function_(now_function),
      random_delay_(random_delay) {
  OSP_DCHECK(record_handler_);
  OSP_DCHECK(sender_);
  OSP_DCHECK(receiver_);
  OSP_DCHECK(querier_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(now_function_);
  OSP_DCHECK(random_delay_);

  auto func = [this](const MdnsMessage& message, const IPEndpoint& src) {
    OnMessageReceived(message, src);
  };
  receiver_->SetQueryCallback(std::move(func));
}

MdnsResponder::~MdnsResponder() {
  receiver_->SetQueryCallback(nullptr);
}

void MdnsResponder::OnMessageReceived(const MdnsMessage& message,
                                      const IPEndpoint& src) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(message.type() == MessageType::Query);

  // TODO(rwkeane): implement responding to the query
}

}  // namespace discovery
}  // namespace openscreen
