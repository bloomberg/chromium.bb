// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_CONTENT_EMBEDDED_BROWSER_H_
#define ASH_SHELL_CONTENT_EMBEDDED_BROWSER_H_

#include <memory>

#include "base/macros.h"

class GURL;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace ash {
namespace shell {

// Exercises ServerRemoteViewHost to embed a content::WebContents.
class EmbeddedBrowser {
 public:
  EmbeddedBrowser(content::BrowserContext* context, const GURL& url);
  ~EmbeddedBrowser();

  // Factory.
  static void Create(content::BrowserContext* context, const GURL& url);

 private:
  // Callback invoked when the embedding is broken.
  void OnUnembed();

  std::unique_ptr<content::WebContents> contents_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedBrowser);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_CONTENT_EMBEDDED_BROWSER_H_
