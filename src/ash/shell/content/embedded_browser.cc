// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/embedded_browser.h"

#include "ash/shell.h"
#include "ash/ws/window_service_owner.h"
#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "content/public/browser/web_contents.h"
#include "services/ws/remote_view_host/server_remote_view_host.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/mus/remote_view/remote_view_provider.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace ash {
namespace shell {

namespace {

// Simulate a widget created on Window Service side to host a native window.
// For classic, it is done by NativeViewHost. For single process mash,
// it uses ServerRemoteViewHost. To implement for mash, the code should be
// moved to ash process somehow and invoked over mojo.
class HostWidget : public views::WidgetDelegateView {
 public:
  // Creates HostWidget to host remote content represented by |token| for mash.
  static void CreateFromToken(const base::UnguessableToken& token) {
    // HostWidget deletes itself when underlying widget closes.
    new HostWidget(nullptr, token);
  }

  // Creates HostWidget that hosts |native_window| directly for classic.
  static void CreateFromNativeWindow(gfx::NativeWindow native_window) {
    // HostWidget deletes itself when underlying widget closes.
    new HostWidget(native_window, base::nullopt);
  }

 private:
  HostWidget(gfx::NativeWindow native_window,
             base::Optional<base::UnguessableToken> token) {
    // Note this does not work under multi process mash.
    DCHECK(!features::IsMultiProcessMash());

    SetLayoutManager(std::make_unique<views::FillLayout>());

    auto* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.context = Shell::GetPrimaryRootWindow();
    params.bounds = gfx::Rect(20, 0, 800, 600);
    params.delegate = this;
    widget->Init(params);
    widget->Show();

    if (native_window) {
      title_ = base::ASCIIToUTF16("Classic: Embed by NativeViewHost");
      auto* host = new views::NativeViewHost();
      AddChildView(host);
      host->Attach(native_window);
    } else {
      title_ = base::ASCIIToUTF16("SPM: Embed by ServerRemoteViewHost");
      auto* host = new ws::ServerRemoteViewHost(
          ash::Shell::Get()->window_service_owner()->window_service());
      AddChildView(host);
      host->EmbedUsingToken(token.value(), 0, base::DoNothing());
    }

    Layout();
  }

  // views::WidgetDelegateView:
  base::string16 GetWindowTitle() const override { return title_; }

  base::string16 title_;

  DISALLOW_COPY_AND_ASSIGN(HostWidget);
};

}  // namespace

EmbeddedBrowser::EmbeddedBrowser(content::BrowserContext* context,
                                 const GURL& url) {
  contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(context));
  contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));
  contents_->GetNativeView()->Show();

  if (!features::IsUsingWindowService()) {
    HostWidget::CreateFromNativeWindow(contents_->GetNativeView());
    return;
  }

  remote_view_provider_ =
      std::make_unique<views::RemoteViewProvider>(contents_->GetNativeView());
  remote_view_provider_->GetEmbedToken(
      base::BindOnce([](const base::UnguessableToken& token) {
        HostWidget::CreateFromToken(token);
      }));
}

EmbeddedBrowser::~EmbeddedBrowser() = default;

// static
void EmbeddedBrowser::Create(content::BrowserContext* context,
                             const GURL& url) {
  // EmbeddedBrowser deletes itself when the hosting widget is closed.
  new EmbeddedBrowser(context, url);
}

void EmbeddedBrowser::OnUnembed() {
  delete this;
}

}  // namespace shell
}  // namespace ash
