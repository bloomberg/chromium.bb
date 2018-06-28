// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_environment_variables.h"

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

static const char kVariableName[] = "test";

// red
static const Color kTestColorRed = Color(255, 0, 0);
static const char kVariableTestColor[] = "red";

// blue
static const Color kAltTestColor = Color(0, 0, 255);
static const char kVariableAltTestColor[] = "blue";

// no set
static const Color kNoColor = Color(0, 0, 0, 0);

}  // namespace

class StyleEnvironmentVariablesTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();

    RuntimeEnabledFeatures::SetCSSEnvironmentVariablesEnabled(true);
  }

  void TearDown() override {
    StyleEnvironmentVariables::GetRootInstance().ClearForTesting();
  }

  DocumentStyleEnvironmentVariables& GetDocumentVariables() {
    return GetStyleEngine().EnsureEnvironmentVariables();
  }

  void InitializeWithHTML(LocalFrame& frame, const String& html_content) {
    // Sets the inner html and runs the document lifecycle.
    frame.GetDocument()->body()->SetInnerHTMLFromString(html_content);
    frame.GetDocument()->View()->UpdateAllLifecyclePhases();
  }

  void InitializeTestPageWithVariableNamed(LocalFrame& frame,
                                           const String& name) {
    InitializeWithHTML(frame,
                       "<style>"
                       "  #target { background-color: env(" +
                           name +
                           "); }"
                           "</style>"
                           "<div>"
                           "  <div id=target></div>"
                           "</div>");
  }

  void SimulateNavigation() {
    const KURL& url = KURL(NullURL(), "https://www.example.com");
    FrameLoadRequest request(nullptr, ResourceRequest(url),
                             SubstituteData(SharedBuffer::Create()));
    GetDocument().GetFrame()->Loader().CommitNavigation(request);
    blink::test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
  }
};

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_AfterLoad) {
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Change) {
  GetDocumentVariables().SetVariable(kVariableName, kVariableAltTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Change the variable value after we have loaded the page.
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest,
       DocumentVariable_Override_RemoveDocument) {
  // Set the variable globally.
  StyleEnvironmentVariables::GetRootInstance().SetVariable(
      kVariableName, kVariableAltTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the global
  // variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value on the document after we have loaded the page.
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the document
  // variable.
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Remove the document variable.
  GetDocumentVariables().RemoveVariable(kVariableName);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the global
  // variable.
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Override_RemoveGlobal) {
  // Set the variable globally.
  StyleEnvironmentVariables::GetRootInstance().SetVariable(
      kVariableName, kVariableAltTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the global
  // variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value on the document after we have loaded the page.
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the document
  // variable.
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Remove the global variable.
  StyleEnvironmentVariables::GetRootInstance().RemoveVariable(kVariableName);

  // Ensure that the document has not been invalidated.
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Preset) {
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Remove) {
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value after we have loaded the page.
  GetDocumentVariables().RemoveVariable(kVariableName);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element does not have the background color any more.
  EXPECT_NE(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, MultiDocumentInvalidation_FromRoot) {
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Create a second page that uses the variable.
  std::unique_ptr<DummyPageHolder> new_page =
      DummyPageHolder::Create(IntSize(800, 600));
  InitializeTestPageWithVariableNamed(new_page->GetFrame(), kVariableName);

  // Create an empty page that does not use the variable.
  std::unique_ptr<DummyPageHolder> empty_page =
      DummyPageHolder::Create(IntSize(800, 600));
  empty_page->GetDocument().View()->UpdateAllLifecyclePhases();

  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);

  // The first two pages should be invalidated and the empty one should not.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_TRUE(new_page->GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(empty_page->GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(StyleEnvironmentVariablesTest, MultiDocumentInvalidation_FromDocument) {
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Create a second page that uses the variable.
  std::unique_ptr<DummyPageHolder> new_page =
      DummyPageHolder::Create(IntSize(800, 600));
  InitializeTestPageWithVariableNamed(new_page->GetFrame(), kVariableName);

  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Only the first document should be invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(new_page->GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(StyleEnvironmentVariablesTest, NavigateToClear) {
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Simulate a navigation to clear the variables.
  SimulateNavigation();
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has no background color.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kNoColor, target->ComputedStyleRef().VisitedDependentColor(
                          GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_AfterLoad) {
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);
  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_Change) {
  StyleEnvironmentVariables::GetRootInstance().SetVariable(
      kVariableName, kVariableAltTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Change the variable value after we have loaded the page.
  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_Preset) {
  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_Remove) {
  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value after we have loaded the page.
  StyleEnvironmentVariables::GetRootInstance().RemoveVariable(kVariableName);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element does not have the background color any more.
  EXPECT_NE(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest,
       DISABLED_PrintExpectedVariableNameHashes) {
  const AtomicString kVariableNames[] = {
      "safe-area-inset-top", "safe-area-inset-left", "safe-area-inset-bottom",
      "safe-area-inset-right"};
  for (const auto& name : kVariableNames) {
    printf("0x%x\n",
           DocumentStyleEnvironmentVariables::GenerateHashFromName(name));
  }
}

TEST_F(StyleEnvironmentVariablesTest, RecordUseCounter_InvalidProperty) {
  InitializeTestPageWithVariableNamed(GetFrame(), kVariableName);
  EXPECT_TRUE(UseCounter::IsCounted(GetDocument(),
                                    WebFeature::kCSSEnvironmentVariable));
}

TEST_F(StyleEnvironmentVariablesTest, RecordUseCounter_NoVariable) {
  InitializeWithHTML(GetFrame(), "");
  EXPECT_FALSE(UseCounter::IsCounted(GetDocument(),
                                     WebFeature::kCSSEnvironmentVariable));
}

TEST_F(StyleEnvironmentVariablesTest, RecordUseCounter_SafeAreaInsetBottom) {
  InitializeTestPageWithVariableNamed(GetFrame(), "safe-area-inset-bottom");

  EXPECT_TRUE(UseCounter::IsCounted(GetDocument(),
                                    WebFeature::kCSSEnvironmentVariable));
  EXPECT_TRUE(UseCounter::IsCounted(
      GetDocument(), WebFeature::kCSSEnvironmentVariable_SafeAreaInsetBottom));
}

TEST_F(StyleEnvironmentVariablesTest, RecordUseCounter_SafeAreaInsetLeft) {
  InitializeTestPageWithVariableNamed(GetFrame(), "safe-area-inset-left");

  EXPECT_TRUE(UseCounter::IsCounted(GetDocument(),
                                    WebFeature::kCSSEnvironmentVariable));
  EXPECT_TRUE(UseCounter::IsCounted(
      GetDocument(), WebFeature::kCSSEnvironmentVariable_SafeAreaInsetLeft));
}

TEST_F(StyleEnvironmentVariablesTest, RecordUseCounter_SafeAreaInsetRight) {
  InitializeTestPageWithVariableNamed(GetFrame(), "safe-area-inset-right");

  EXPECT_TRUE(UseCounter::IsCounted(GetDocument(),
                                    WebFeature::kCSSEnvironmentVariable));
  EXPECT_TRUE(UseCounter::IsCounted(
      GetDocument(), WebFeature::kCSSEnvironmentVariable_SafeAreaInsetRight));
}

TEST_F(StyleEnvironmentVariablesTest, RecordUseCounter_SafeAreaInsetTop) {
  InitializeTestPageWithVariableNamed(GetFrame(), "safe-area-inset-top");

  EXPECT_TRUE(UseCounter::IsCounted(GetDocument(),
                                    WebFeature::kCSSEnvironmentVariable));
  EXPECT_TRUE(UseCounter::IsCounted(
      GetDocument(), WebFeature::kCSSEnvironmentVariable_SafeAreaInsetTop));
}

}  // namespace blink
