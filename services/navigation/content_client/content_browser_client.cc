// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/navigation/content_client/content_browser_client.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "content/public/common/mojo_shell_connection.h"
#include "content/shell/browser/shell_browser_context.h"
#include "services/navigation/content_client/browser_main_parts.h"
#include "services/navigation/navigation.h"

namespace navigation {
namespace {

class ConnectionListener : public content::MojoShellConnection::Listener {
 public:
  explicit ConnectionListener(std::unique_ptr<shell::ShellClient> wrapped)
      : wrapped_(std::move(wrapped)) {}

 private:
  bool AcceptConnection(shell::Connection* connection) override {
    return wrapped_->AcceptConnection(connection);
  }

  std::unique_ptr<shell::ShellClient> wrapped_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionListener);
};

}  // namespace

ContentBrowserClient::ContentBrowserClient() {}
ContentBrowserClient::~ContentBrowserClient() {}

content::BrowserMainParts* ContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  browser_main_parts_ = new BrowserMainParts(parameters);
  return browser_main_parts_;
}

void ContentBrowserClient::AddMojoShellConnectionListeners() {
  Navigation* navigation = new Navigation;
  browser_main_parts_->set_navigation(navigation);
  content::MojoShellConnection::Get()->AddListener(
      base::WrapUnique(new ConnectionListener(base::WrapUnique(navigation))));
}

}  // namespace navigation
