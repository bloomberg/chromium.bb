// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "InternalsNavigatorContentUtils.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/testing/Internals.h"
#include "modules/navigatorcontentutils/NavigatorContentUtils.h"
#include "modules/navigatorcontentutils/testing/NavigatorContentUtilsClientMock.h"

namespace blink {

void InternalsNavigatorContentUtils::setNavigatorContentUtilsClientMock(
    Internals&,
    Document* document) {
  DCHECK(document);
  DCHECK(document->GetPage());
  NavigatorContentUtils* navigator_content_utils =
      NavigatorContentUtils::From(*document->domWindow()->navigator());
  navigator_content_utils->SetClientForTest(
      NavigatorContentUtilsClientMock::Create());
}

}  // namespace blink
