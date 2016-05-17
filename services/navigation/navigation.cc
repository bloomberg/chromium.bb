// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/navigation/navigation.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "services/navigation/view_impl.h"

namespace navigation {

Navigation::Navigation()
    : ref_factory_(base::MessageLoop::QuitWhenIdleClosure()) {
  bindings_.set_connection_error_handler(
      base::Bind(&Navigation::ViewFactoryLost, base::Unretained(this)));
}
Navigation::~Navigation() {}

void Navigation::Init(shell::Connector* connector,
                      content::BrowserContext* browser_context) {
  connector_ = connector;
  browser_context_ = browser_context;
  for (auto& pending : pending_creates_)
    CreateView(std::move(pending.first), std::move(pending.second));
}

bool Navigation::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mojom::ViewFactory>(this);
  return true;
}

void Navigation::Create(shell::Connection* connection,
                        mojom::ViewFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
  refs_.insert(ref_factory_.CreateRef());
}

void Navigation::CreateView(mojom::ViewClientPtr client,
                            mojom::ViewRequest request) {
  if (!browser_context_) {
    pending_creates_.push_back(
        std::make_pair(std::move(client), std::move(request)));
    return;
  }
  new ViewImpl(connector_, browser_context_, std::move(client),
               std::move(request), ref_factory_.CreateRef());
}

void Navigation::ViewFactoryLost() {
  refs_.erase(refs_.begin());
}

}  // navigation
