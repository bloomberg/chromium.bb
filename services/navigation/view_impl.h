// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NAVIGATION_VIEW_IMPL_H_
#define SERVICES_NAVIGATION_VIEW_IMPL_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_delegate.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/navigation/public/interfaces/view.mojom.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection_ref.h"

namespace navigation {

class ViewImpl : public mojom::View,
                 public content::WebContentsDelegate {
 public:
  ViewImpl(content::BrowserContext* browser_context,
           mojom::ViewClientPtr client,
           mojom::ViewRequest request,
           std::unique_ptr<shell::ShellConnectionRef> ref);
  ~ViewImpl() override;

 private:
  // mojom::View:
  void LoadUrl(const GURL& url) override;

  // content::WebContentsDelegate:
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;

  mojo::StrongBinding<mojom::View> binding_;
  mojom::ViewClientPtr client_;
  std::unique_ptr<shell::ShellConnectionRef> ref_;

  std::unique_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ViewImpl);
};

}  // navigation

#endif  // SERVICES_NAVIGATION_VIEW_IMPL_H_
