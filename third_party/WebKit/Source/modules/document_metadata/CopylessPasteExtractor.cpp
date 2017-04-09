// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/document_metadata/CopylessPasteExtractor.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLElement.h"
#include "platform/Histogram.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

String ExtractMetadata(Element& root) {
  StringBuilder result;
  result.Append("[");
  bool multiple = false;
  for (Element& element : ElementTraversal::DescendantsOf(root)) {
    if (element.HasTagName(HTMLNames::scriptTag) &&
        element.getAttribute(HTMLNames::typeAttr) == "application/ld+json") {
      if (multiple) {
        result.Append(",");
      }
      result.Append(element.textContent());
      multiple = true;
    }
  }
  result.Append("]");
  return result.ToString();
}

}  // namespace

String CopylessPasteExtractor::Extract(Document& document) {
  TRACE_EVENT0("blink", "CopylessPasteExtractor::extract");

  if (!document.GetFrame() || !document.GetFrame()->IsMainFrame())
    return g_empty_string;

  DCHECK(document.HasFinishedParsing());

  Element* html = document.documentElement();
  if (!html)
    return g_empty_string;

  double start_time = MonotonicallyIncreasingTime();

  // Traverse the DOM tree and extract the metadata.
  String result = ExtractMetadata(*html);

  double elapsed_time = MonotonicallyIncreasingTime() - start_time;

  DEFINE_STATIC_LOCAL(CustomCountHistogram, extraction_histogram,
                      ("CopylessPaste.ExtractionUs", 1, 1000000, 50));
  extraction_histogram.Count(static_cast<int>(1e6 * elapsed_time));
  return result;
}

}  // namespace blink
