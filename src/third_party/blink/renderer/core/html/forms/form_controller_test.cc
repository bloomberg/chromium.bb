// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/form_controller.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

TEST(DocumentStateTest, ToStateVectorConnected) {
  Document& doc = *Document::CreateForTest();
  Element* html = doc.CreateRawElement(html_names::kHTMLTag);
  doc.appendChild(html);
  Node* body = html->appendChild(doc.CreateRawElement(html_names::kBodyTag));
  ToElement(body)->SetInnerHTMLFromString("<select form='ff'></select>");
  DocumentState* document_state = doc.GetFormController().FormElementsState();
  Vector<String> state1 = document_state->ToStateVector();
  // <signature>, <control-size>, <form-key>, <name>, <type>, <data-size(0)>
  EXPECT_EQ(6u, state1.size());
  Node* select = body->firstChild();
  select->remove();
  // Success if the following ToStateVector() doesn't fail with a DCHECK.
  Vector<String> state2 = document_state->ToStateVector();
  EXPECT_EQ(0u, state2.size());
}

}  // namespace blink
