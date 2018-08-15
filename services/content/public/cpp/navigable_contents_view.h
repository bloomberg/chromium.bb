// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CONTENT_PUBLIC_CPP_NAVIGABLE_CONTENTS_VIEW_H_
#define SERVICES_CONTENT_PUBLIC_CPP_NAVIGABLE_CONTENTS_VIEW_H_

#include <memory>

#include "base/component_export.h"
#include "base/unguessable_token.h"
#include "services/content/public/cpp/buildflags.h"

namespace views {
class RemoteViewHost;
class View;
}  // namespace views

namespace content {

class NavigableContents;
class NavigableContentsImpl;

// NavigableContentsView encapsulates cross-platform manipulation and
// presentation of a NavigableContents within a native application UI based on
// either Aura, UIKit, AppKit, or the Android Framework.
//
// TODO(https://crbug.com/855092): Actually support UI frameworks other than
// Aura.
class COMPONENT_EXPORT(CONTENT_SERVICE_CPP) NavigableContentsView {
 public:
  ~NavigableContentsView();

  // Used to set/query whether the calling process is the same process in which
  // all Content Service instances are running. This should be used sparingly,
  // and in general is only here to support internal sanity checks when
  // performing, e.g., UI embedding operations on platforms where remote
  // NavigableContentsViews are not yet supported.
  static void SetClientRunningInServiceProcess();
  static bool IsClientRunningInServiceProcess();

#if BUILDFLAG(ENABLE_NAVIGABLE_CONTENTS_VIEW_AURA)
  views::View* view() const { return view_.get(); }
#endif

 private:
  friend class NavigableContents;
  friend class NavigableContentsImpl;

  NavigableContentsView();

  // Establishes a hierarchical relationship between this view's native UI
  // object and another native UI object within the Content Service.
  void EmbedUsingToken(const base::UnguessableToken& token);

  // Used by the service directly when running in the same process. Establishes
  // a way for an embed token to be used without the UI service.
  static void RegisterInProcessEmbedCallback(
      const base::UnguessableToken& token,
      base::OnceCallback<void(NavigableContentsView*)> callback);

#if BUILDFLAG(ENABLE_NAVIGABLE_CONTENTS_VIEW_AURA)
  // This NavigableContents's View. Only initialized if |GetView()| is called,
  // and only on platforms which support View embedding via Aura.
  std::unique_ptr<views::View> view_;

#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
  views::RemoteViewHost* remote_view_host_ = nullptr;
#endif
#endif  // BUILDFLAG(ENABLE_NAVIGABLE_CONTENTS_VIEW_AURA)

  DISALLOW_COPY_AND_ASSIGN(NavigableContentsView);
};

}  // namespace content

#endif  // SERVICES_CONTENT_PUBLIC_CPP_NAVIGABLE_CONTENTS_VIEW_H_
