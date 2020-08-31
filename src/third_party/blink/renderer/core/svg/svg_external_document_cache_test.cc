// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_external_document_cache.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"

namespace blink {

class DummyCacheClient : public GarbageCollected<DummyCacheClient>,
                         public SVGExternalDocumentCache::Client {
  USING_GARBAGE_COLLECTED_MIXIN(DummyCacheClient);

 public:
  DummyCacheClient() = default;
  void NotifyFinished(Document*) override {}
};

class SVGExternalDocumentCacheTest : public SimTest {};

TEST_F(SVGExternalDocumentCacheTest, GetDocumentBeforeLoadComplete) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete("<html><body></body></html>");

  const char kSVGUrl[] = "https://example.com/svg.svg";
  SimRequest::Params params;
  params.response_http_headers = {{"Content-Type", "application/xml"}};
  SimSubresourceRequest svg_resource(kSVGUrl, "application/xml", params);
  DummyCacheClient* client = MakeGarbageCollected<DummyCacheClient>();

  // Request a resource from the cache.
  auto* entry =
      SVGExternalDocumentCache::From(GetDocument())
          ->Get(client, KURL(kSVGUrl), fetch_initiator_type_names::kCSS);

  // Write part of the response. The document should not be initialized yet,
  // because the response is not complete. The document would be invalid at this
  // point.
  svg_resource.Start();
  svg_resource.Write("<sv");
  EXPECT_EQ(nullptr, entry->GetDocument());

  // Finish the response, the Document should now be accessible.
  svg_resource.Complete("g></svg>");
  EXPECT_NE(nullptr, entry->GetDocument());
}

}  // namespace blink
