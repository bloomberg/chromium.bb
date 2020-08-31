// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_WEB_VIEW_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_WEB_VIEW_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "ui/views/view.h"

class GURL;
enum class WindowOpenDisposition;

namespace ash {

// A view which wraps a views::WebView (and associated WebContents) to work
// around dependency restrictions in Ash.
class ASH_PUBLIC_EXPORT AssistantWebView : public views::View {
 public:
  // Initialization parameters which dictate how an instance of AssistantWebView
  // should behave.
  struct InitParams {
    InitParams();
    InitParams(const InitParams& copy);
    ~InitParams();

    // If enabled, AssistantWebView will automatically resize to the size
    // desired by its embedded WebContents. Note that, if specified, the
    // WebContents will be bounded by |min_size| and |max_size|.
    bool enable_auto_resize = false;
    base::Optional<gfx::Size> min_size = base::nullopt;
    base::Optional<gfx::Size> max_size = base::nullopt;

    // If enabled, AssistantWebView will suppress navigation attempts of its
    // embedded WebContents. When navigation suppression occurs,
    // Observer::DidSuppressNavigation() will be invoked.
    bool suppress_navigation = false;

    // If enabled, AssistantWebView can be minimized once we received a ash
    // synthesized back event when we're at the bottom of the stack.
    bool minimize_on_back_key = false;
  };

  // An observer which receives AssistantWebView events.
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the embedded WebContents has stopped loading.
    virtual void DidStopLoading() {}

    // Invoked when the embedded WebContents has suppressed navigation.
    virtual void DidSuppressNavigation(const GURL& url,
                                       WindowOpenDisposition disposition,
                                       bool from_user_gesture) {}

    // Invoked when the embedded WebContents' ability to go back has changed.
    virtual void DidChangeCanGoBack(bool can_go_back) {}

    // Invoked when the focused node within the embedded WebContents has
    // changed.
    virtual void DidChangeFocusedNode(const gfx::Rect& node_bounds_in_screen) {}
  };

  ~AssistantWebView() override;

  // Adds/removes the specified |observer|.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the native view associated w/ the underlying WebContents.
  virtual gfx::NativeView GetNativeView() = 0;

  // Invoke to navigate back in the embedded WebContents' navigation stack. If
  // backwards navigation is not possible, return |false|. Otherwise returns
  // |true| to indicate success.
  virtual bool GoBack() = 0;

  // Invoke to navigate the embedded WebContents' to |url|.
  virtual void Navigate(const GURL& url) = 0;

 protected:
  AssistantWebView();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_WEB_VIEW_H_
