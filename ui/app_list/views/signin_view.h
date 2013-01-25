// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SIGNIN_VIEW_H_
#define UI_APP_LIST_VIEWS_SIGNIN_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}

namespace views {
class WebView;
}

namespace app_list {

class SigninDelegate;

// The SigninView is shown in the app list when the user needs to sign in.
// It just shows a webview, which is prepared for signin by the signin delegate.
class SigninView : public views::View {
 public:
  SigninView(SigninDelegate* delegate);
  virtual ~SigninView();

 private:
  // Overridden from views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;

  scoped_ptr<content::WebContents> web_contents_;
  views::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(SigninView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SIGNIN_VIEW_H_
