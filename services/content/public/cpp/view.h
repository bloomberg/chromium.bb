// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CONTENT_PUBLIC_CPP_VIEW_H_
#define SERVICES_CONTENT_PUBLIC_CPP_VIEW_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/content/public/cpp/buildflags.h"
#include "services/content/public/mojom/view.mojom.h"
#include "services/content/public/mojom/view_factory.mojom.h"

namespace views {
class RemoteViewHost;
class View;
}  // namespace views

namespace content {

// A View is a navigable, top-level view of web content which applications can
// embed within their own UI.
//
// A View does not need to be used only for displaying visually renderable web
// contents, so by default it has no graphical presence. Call |CreateUI()| to
// get a usable Views widget which displays the navigated web contents and which
// can be attached to an existing window tree.
class COMPONENT_EXPORT(CONTENT_SERVICE_CPP) View : public mojom::ViewClient {
 public:
  // Constructs a new View using |factory|.
  explicit View(mojom::ViewFactory* factory);
  ~View() override;

  // Initialize's this View for use within a window tree. Returns the
  // corresponding |views::View|, which the caller may parent to some other
  // widget. This widget will display the web content navigated by the Content
  // Service on this View's behalf.
  views::View* CreateUI();

  // Begins an attempt to asynchronously navigate this View to |url|.
  void Navigate(const GURL& url);

  void set_did_stop_loading_callback_for_testing(
      base::RepeatingClosure callback) {
    did_stop_loading_callback_ = std::move(callback);
  }

 private:
  // mojom::ViewClient:
  void DidStopLoading() override;

  void OnEmbedTokenReceived(const base::UnguessableToken& token);

  mojom::ViewPtr view_;
  mojo::Binding<mojom::ViewClient> client_binding_;

#if BUILDFLAG(ENABLE_AURA_CONTENT_VIEW_EMBEDDING)
  // This View's node in the client's window tree. Non-existent by default, but
  // may be initialized by calling InitializeUI.
  std::unique_ptr<views::View> ui_view_;
  views::RemoteViewHost* remote_view_host_ = nullptr;
#endif

  base::RepeatingClosure did_stop_loading_callback_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace content

#endif  // SERVICES_CONTENT_PUBLIC_CPP_VIEW_H_
