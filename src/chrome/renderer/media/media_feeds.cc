// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/media_feeds.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "url/gurl.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebNode;
using blink::WebString;

base::Optional<GURL> MediaFeeds::GetMediaFeedURL(content::RenderFrame* frame) {
  // Media Feeds are only discovered on the main frame.
  if (!frame->IsMainFrame())
    return base::nullopt;

  WebDocument document = frame->GetWebFrame()->GetDocument();
  if (document.IsNull())
    return base::nullopt;

  WebElement head = document.Head();
  if (head.IsNull())
    return base::nullopt;

  url::Origin document_origin = document.GetSecurityOrigin();

  // Look for a <link> element in the <head> of the document.
  for (WebNode child = head.FirstChild(); !child.IsNull();
       child = child.NextSibling()) {
    if (!child.IsElementNode())
      continue;

    WebElement elem = child.To<WebElement>();
    if (!elem.HasHTMLTagName("link"))
      continue;

    // The <link> rel must be feed.
    std::string rel = elem.GetAttribute("rel").Utf8();
    if (!base::LowerCaseEqualsASCII(rel, "feed"))
      continue;

    // The <link> type must the JSON+LD mime type.
    std::string type = elem.GetAttribute("type").Utf8();
    if (!base::LowerCaseEqualsASCII(type, "application/ld+json"))
      continue;

    WebString href = elem.GetAttribute("href");
    if (href.IsNull() || href.IsEmpty())
      continue;

    // If the URL is not valid then we should throw an error.
    GURL url = document.CompleteURL(href);
    if (!url.is_valid()) {
      frame->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                                 "The Media Feed URL is not a valid URL.");

      return base::nullopt;
    }

    // If the URL is not the same origin as the document then we should throw
    // and error.
    auto feed_origin = url::Origin::Create(url);
    if (!document_origin.IsSameOriginWith(feed_origin)) {
      frame->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                                 "The Media Feed URL needs to be the same "
                                 "origin as the document URL.");

      return base::nullopt;
    }

    return url;
  }

  return base::nullopt;
}
