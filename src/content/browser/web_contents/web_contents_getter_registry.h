// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_GETTER_REGISTRY_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_GETTER_REGISTRY_H_

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"
#include "content/public/browser/web_contents.h"

namespace content {

// A global map of UnguessableToken to WebContentsGetter. Unlike most other
// WebContents related objects, this registry lives and is used only on the IO
// thread, as it's convenient for the current user of the class
// (ServiceWorkerProviderHost, which should move to the UI thread eventually).
// However, the WebContentsGetter callbacks must only be run on the UI thread.
class WebContentsGetterRegistry {
 public:
  using WebContentsGetter = base::RepeatingCallback<WebContents*()>;

  static WebContentsGetterRegistry* GetInstance();

  void Add(const base::UnguessableToken& id,
           const WebContentsGetter& web_contents_getter);
  void Remove(const base::UnguessableToken&);
  // Returns null getter if not found.
  const WebContentsGetter& Get(const base::UnguessableToken& id) const;

 private:
  friend class base::NoDestructor<WebContentsGetterRegistry>;

  WebContentsGetterRegistry();
  ~WebContentsGetterRegistry();

  static const WebContentsGetter& GetNullGetter();

  base::flat_map<base::UnguessableToken, WebContentsGetter> map_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(WebContentsGetterRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_GETTER_REGISTRY_H_
