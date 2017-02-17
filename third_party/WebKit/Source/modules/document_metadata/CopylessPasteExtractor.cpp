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

String extractMetadata(Element& root) {
  StringBuilder result;
  result.append("[");
  bool multiple = false;
  for (Element& element : ElementTraversal::descendantsOf(root)) {
    if (element.hasTagName(HTMLNames::scriptTag) &&
        element.getAttribute(HTMLNames::typeAttr) == "application/ld+json") {
      if (multiple) {
        result.append(",");
      }
      result.append(element.textContent());
      multiple = true;
    }
  }
  result.append("]");
  return result.toString();
}

}  // namespace

String CopylessPasteExtractor::extract(Document& document) {
  TRACE_EVENT0("blink", "CopylessPasteExtractor::extract");

  if (!document.frame() || !document.frame()->isMainFrame())
    return emptyString;

  DCHECK(document.hasFinishedParsing());

  Element* html = document.documentElement();
  if (!html)
    return emptyString;

  double startTime = monotonicallyIncreasingTime();

  // Traverse the DOM tree and extract the metadata.
  String result = extractMetadata(*html);

  double elapsedTime = monotonicallyIncreasingTime() - startTime;

  DEFINE_STATIC_LOCAL(CustomCountHistogram, extractionHistogram,
                      ("CopylessPaste.ExtractionUs", 1, 1000000, 50));
  extractionHistogram.count(static_cast<int>(1e6 * elapsedTime));
  return result;
}

}  // namespace blink
