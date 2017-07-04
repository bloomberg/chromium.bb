// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLEmbedElement.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLObjectElement.h"
#include "core/style/ComputedStyle.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class HTMLEmbedElementTest : public ::testing::Test {
 protected:
  HTMLEmbedElementTest() {}

  void SetUp() override;

  Document& GetDocument() const { return *document_; }

  void SetHtmlInnerHTML(const char* html_content);

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<Document> document_;
};

void HTMLEmbedElementTest::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  document_ = &dummy_page_holder_->GetDocument();
  DCHECK(document_);
}

void HTMLEmbedElementTest::SetHtmlInnerHTML(const char* html_content) {
  GetDocument().documentElement()->setInnerHTML(String::FromUTF8(html_content));
  GetDocument().View()->UpdateAllLifecyclePhases();
}

TEST_F(HTMLEmbedElementTest, FallbackState) {
  // Load <object> element with a <embed> child.
  // This can be seen on sites with Flash cookies,
  // for example on www.yandex.ru
  SetHtmlInnerHTML(
      "<div>"
      "<object classid='clsid:D27CDB6E-AE6D-11cf-96B8-444553540000' width='1' "
      "height='1' id='fco'>"
      "<param name='movie' value='//site.com/flash-cookie.swf'>"
      "<param name='allowScriptAccess' value='Always'>"
      "<embed src='//site.com/flash-cookie.swf' allowscriptaccess='Always' "
      "width='1' height='1' id='fce'>"
      "</object></div>");

  auto* object_element = GetDocument().getElementById("fco");
  ASSERT_TRUE(object_element);
  ASSERT_TRUE(isHTMLObjectElement(object_element));
  HTMLObjectElement* object = toHTMLObjectElement(object_element);

  // At this moment updatePlugin() function is not called, so
  // useFallbackContent() will return false.
  // But the element will likely to use fallback content after updatePlugin().
  EXPECT_TRUE(object->HasFallbackContent());
  EXPECT_FALSE(object->UseFallbackContent());
  EXPECT_TRUE(object->WillUseFallbackContentAtLayout());

  auto* embed_element = GetDocument().getElementById("fce");
  ASSERT_TRUE(embed_element);
  ASSERT_TRUE(isHTMLEmbedElement(embed_element));
  HTMLEmbedElement* embed = toHTMLEmbedElement(embed_element);

  GetDocument().View()->UpdateAllLifecyclePhases();

  // We should get |true| as a result and don't trigger a DCHECK.
  EXPECT_TRUE(static_cast<Element*>(embed)->LayoutObjectIsNeeded(
      ComputedStyle::InitialStyle()));

  // This call will update fallback state of the object.
  object->UpdatePlugin();

  EXPECT_TRUE(object->HasFallbackContent());
  EXPECT_TRUE(object->UseFallbackContent());
  EXPECT_TRUE(object->WillUseFallbackContentAtLayout());

  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(static_cast<Element*>(embed)->LayoutObjectIsNeeded(
      ComputedStyle::InitialStyle()));
}

}  // namespace blink
