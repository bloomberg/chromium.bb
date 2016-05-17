// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NAVIGATION_VIEW_IMPL_H_
#define SERVICES_NAVIGATION_VIEW_IMPL_H_

#include "base/macros.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/navigation/public/interfaces/view.mojom.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection_ref.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class WebView;
class Widget;
}

namespace navigation {

class ViewImpl : public mojom::View,
                 public content::WebContentsDelegate,
                 public mus::WindowTreeDelegate,
                 public views::WidgetDelegate {
 public:
  ViewImpl(shell::Connector* connector,
           content::BrowserContext* browser_context,
           mojom::ViewClientPtr client,
           mojom::ViewRequest request,
           std::unique_ptr<shell::ShellConnectionRef> ref);
  ~ViewImpl() override;

 private:
  // mojom::View:
  void NavigateTo(const GURL& url) override;
  void GoBack() override;
  void GoForward() override;
  void Reload(bool skip_cache) override;
  void Stop() override;
  void GetWindowTreeClient(
      mus::mojom::WindowTreeClientRequest request) override;

  // content::WebContentsDelegate:
  void AddNewContents(content::WebContents* source,
                      content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void CloseContents(content::WebContents* source) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void LoadProgressChanged(content::WebContents* source,
                           double progress) override;

  // mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;
  void OnEventObserved(const ui::Event& event, mus::Window* target) override;

  // views::WidgetDelegate:
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  shell::Connector* connector_;
  mojo::StrongBinding<mojom::View> binding_;
  mojom::ViewClientPtr client_;
  std::unique_ptr<shell::ShellConnectionRef> ref_;

  views::WebView* web_view_;

  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(ViewImpl);
};

}  // navigation

#endif  // SERVICES_NAVIGATION_VIEW_IMPL_H_
