/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/Document.h"

#include <memory>
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/NodeWithIndex.h"
#include "core/dom/SynchronousMutationObserver.h"
#include "core/dom/Text.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/page/Page.h"
#include "core/page/ValidationMessageClient.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class DocumentTest : public ::testing::Test {
 protected:
  void SetUp() override;

  void TearDown() override { ThreadState::current()->collectAllGarbage(); }

  Document& document() const { return m_dummyPageHolder->document(); }
  Page& page() const { return m_dummyPageHolder->page(); }

  void setHtmlInnerHTML(const char*);

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

void DocumentTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

void DocumentTest::setHtmlInnerHTML(const char* htmlContent) {
  document().documentElement()->setInnerHTML(String::fromUTF8(htmlContent));
  document().view()->updateAllLifecyclePhases();
}

namespace {

class TestSynchronousMutationObserver
    : public GarbageCollectedFinalized<TestSynchronousMutationObserver>,
      public SynchronousMutationObserver {
  USING_GARBAGE_COLLECTED_MIXIN(TestSynchronousMutationObserver);

 public:
  struct MergeTextNodesRecord : GarbageCollected<MergeTextNodesRecord> {
    Member<const Text> m_node;
    Member<Node> m_nodeToBeRemoved;
    unsigned m_offset = 0;

    MergeTextNodesRecord(const Text* node,
                         const NodeWithIndex& nodeWithIndex,
                         unsigned offset)
        : m_node(node),
          m_nodeToBeRemoved(nodeWithIndex.node()),
          m_offset(offset) {}

    DEFINE_INLINE_TRACE() {
      visitor->trace(m_node);
      visitor->trace(m_nodeToBeRemoved);
    }
  };

  struct UpdateCharacterDataRecord
      : GarbageCollected<UpdateCharacterDataRecord> {
    Member<CharacterData> m_node;
    unsigned m_offset = 0;
    unsigned m_oldLength = 0;
    unsigned m_newLength = 0;

    UpdateCharacterDataRecord(CharacterData* node,
                              unsigned offset,
                              unsigned oldLength,
                              unsigned newLength)
        : m_node(node),
          m_offset(offset),
          m_oldLength(oldLength),
          m_newLength(newLength) {}

    DEFINE_INLINE_TRACE() { visitor->trace(m_node); }
  };

  TestSynchronousMutationObserver(Document&);
  virtual ~TestSynchronousMutationObserver() = default;

  int countContextDestroyedCalled() const {
    return m_contextDestroyedCalledCounter;
  }

  const HeapVector<Member<const ContainerNode>>& childrenChangedNodes() const {
    return m_childrenChangedNodes;
  }

  const HeapVector<Member<MergeTextNodesRecord>>& mergeTextNodesRecords()
      const {
    return m_mergeTextNodesRecords;
  }

  const HeapVector<Member<const Node>>& moveTreeToNewDocumentNodes() const {
    return m_moveTreeToNewDocumentNodes;
  }

  const HeapVector<Member<ContainerNode>>& removedChildrenNodes() const {
    return m_removedChildrenNodes;
  }

  const HeapVector<Member<Node>>& removedNodes() const {
    return m_removedNodes;
  }

  const HeapVector<Member<const Text>>& splitTextNodes() const {
    return m_splitTextNodes;
  }

  const HeapVector<Member<UpdateCharacterDataRecord>>&
  updatedCharacterDataRecords() const {
    return m_updatedCharacterDataRecords;
  }

  DECLARE_TRACE();

 private:
  // Implement |SynchronousMutationObserver| member functions.
  void contextDestroyed(Document*) final;
  void didChangeChildren(const ContainerNode&) final;
  void didMergeTextNodes(const Text&, const NodeWithIndex&, unsigned) final;
  void didMoveTreeToNewDocument(const Node& root) final;
  void didSplitTextNode(const Text&) final;
  void didUpdateCharacterData(CharacterData*,
                              unsigned offset,
                              unsigned oldLength,
                              unsigned newLength) final;
  void nodeChildrenWillBeRemoved(ContainerNode&) final;
  void nodeWillBeRemoved(Node&) final;

  int m_contextDestroyedCalledCounter = 0;
  HeapVector<Member<const ContainerNode>> m_childrenChangedNodes;
  HeapVector<Member<MergeTextNodesRecord>> m_mergeTextNodesRecords;
  HeapVector<Member<const Node>> m_moveTreeToNewDocumentNodes;
  HeapVector<Member<ContainerNode>> m_removedChildrenNodes;
  HeapVector<Member<Node>> m_removedNodes;
  HeapVector<Member<const Text>> m_splitTextNodes;
  HeapVector<Member<UpdateCharacterDataRecord>> m_updatedCharacterDataRecords;

  DISALLOW_COPY_AND_ASSIGN(TestSynchronousMutationObserver);
};

TestSynchronousMutationObserver::TestSynchronousMutationObserver(
    Document& document) {
  setContext(&document);
}

void TestSynchronousMutationObserver::contextDestroyed(Document*) {
  ++m_contextDestroyedCalledCounter;
}

void TestSynchronousMutationObserver::didChangeChildren(
    const ContainerNode& container) {
  m_childrenChangedNodes.push_back(&container);
}

void TestSynchronousMutationObserver::didMergeTextNodes(
    const Text& node,
    const NodeWithIndex& nodeWithIndex,
    unsigned offset) {
  m_mergeTextNodesRecords.push_back(
      new MergeTextNodesRecord(&node, nodeWithIndex, offset));
}

void TestSynchronousMutationObserver::didMoveTreeToNewDocument(
    const Node& root) {
  m_moveTreeToNewDocumentNodes.push_back(&root);
}

void TestSynchronousMutationObserver::didSplitTextNode(const Text& node) {
  m_splitTextNodes.push_back(&node);
}

void TestSynchronousMutationObserver::didUpdateCharacterData(
    CharacterData* characterData,
    unsigned offset,
    unsigned oldLength,
    unsigned newLength) {
  m_updatedCharacterDataRecords.push_back(new UpdateCharacterDataRecord(
      characterData, offset, oldLength, newLength));
}

void TestSynchronousMutationObserver::nodeChildrenWillBeRemoved(
    ContainerNode& container) {
  m_removedChildrenNodes.push_back(&container);
}

void TestSynchronousMutationObserver::nodeWillBeRemoved(Node& node) {
  m_removedNodes.push_back(&node);
}

DEFINE_TRACE(TestSynchronousMutationObserver) {
  visitor->trace(m_childrenChangedNodes);
  visitor->trace(m_mergeTextNodesRecords);
  visitor->trace(m_moveTreeToNewDocumentNodes);
  visitor->trace(m_removedChildrenNodes);
  visitor->trace(m_removedNodes);
  visitor->trace(m_splitTextNodes);
  visitor->trace(m_updatedCharacterDataRecords);
  SynchronousMutationObserver::trace(visitor);
}

class MockValidationMessageClient
    : public GarbageCollectedFinalized<MockValidationMessageClient>,
      public ValidationMessageClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockValidationMessageClient);

 public:
  MockValidationMessageClient() { reset(); }
  void reset() {
    showValidationMessageWasCalled = false;
    willUnloadDocumentWasCalled = false;
    documentDetachedWasCalled = false;
  }
  bool showValidationMessageWasCalled;
  bool willUnloadDocumentWasCalled;
  bool documentDetachedWasCalled;

  // ValidationMessageClient functions.
  void showValidationMessage(const Element& anchor,
                             const String& mainMessage,
                             TextDirection,
                             const String& subMessage,
                             TextDirection) override {
    showValidationMessageWasCalled = true;
  }
  void hideValidationMessage(const Element& anchor) override {}
  bool isValidationMessageVisible(const Element& anchor) override {
    return true;
  }
  void willUnloadDocument(const Document&) override {
    willUnloadDocumentWasCalled = true;
  }
  void documentDetached(const Document&) override {
    documentDetachedWasCalled = true;
  }
  void willBeDestroyed() override {}

  // DEFINE_INLINE_VIRTUAL_TRACE() { ValidationMessageClient::trace(visitor); }
};

}  // anonymous namespace

// This tests that we properly resize and re-layout pages for printing in the
// presence of media queries effecting elements in a subtree layout boundary
TEST_F(DocumentTest, PrintRelayout) {
  setHtmlInnerHTML(
      "<style>"
      "    div {"
      "        width: 100px;"
      "        height: 100px;"
      "        overflow: hidden;"
      "    }"
      "    span {"
      "        width: 50px;"
      "        height: 50px;"
      "    }"
      "    @media screen {"
      "        span {"
      "            width: 20px;"
      "        }"
      "    }"
      "</style>"
      "<p><div><span></span></div></p>");
  FloatSize pageSize(400, 400);
  float maximumShrinkRatio = 1.6;

  document().frame()->setPrinting(true, pageSize, pageSize, maximumShrinkRatio);
  EXPECT_EQ(document().documentElement()->offsetWidth(), 400);
  document().frame()->setPrinting(false, FloatSize(), FloatSize(), 0);
  EXPECT_EQ(document().documentElement()->offsetWidth(), 800);
}

// This test checks that Documunt::linkManifest() returns a value conform to the
// specification.
TEST_F(DocumentTest, LinkManifest) {
  // Test the default result.
  EXPECT_EQ(0, document().linkManifest());

  // Check that we use the first manifest with <link rel=manifest>
  HTMLLinkElement* link = HTMLLinkElement::create(document(), false);
  link->setAttribute(blink::HTMLNames::relAttr, "manifest");
  link->setAttribute(blink::HTMLNames::hrefAttr, "foo.json");
  document().head()->appendChild(link);
  EXPECT_EQ(link, document().linkManifest());

  HTMLLinkElement* link2 = HTMLLinkElement::create(document(), false);
  link2->setAttribute(blink::HTMLNames::relAttr, "manifest");
  link2->setAttribute(blink::HTMLNames::hrefAttr, "bar.json");
  document().head()->insertBefore(link2, link);
  EXPECT_EQ(link2, document().linkManifest());
  document().head()->appendChild(link2);
  EXPECT_EQ(link, document().linkManifest());

  // Check that crazy URLs are accepted.
  link->setAttribute(blink::HTMLNames::hrefAttr, "http:foo.json");
  EXPECT_EQ(link, document().linkManifest());

  // Check that empty URLs are accepted.
  link->setAttribute(blink::HTMLNames::hrefAttr, "");
  EXPECT_EQ(link, document().linkManifest());

  // Check that URLs from different origins are accepted.
  link->setAttribute(blink::HTMLNames::hrefAttr,
                     "http://example.org/manifest.json");
  EXPECT_EQ(link, document().linkManifest());
  link->setAttribute(blink::HTMLNames::hrefAttr,
                     "http://foo.example.org/manifest.json");
  EXPECT_EQ(link, document().linkManifest());
  link->setAttribute(blink::HTMLNames::hrefAttr,
                     "http://foo.bar/manifest.json");
  EXPECT_EQ(link, document().linkManifest());

  // More than one token in @rel is accepted.
  link->setAttribute(blink::HTMLNames::relAttr, "foo bar manifest");
  EXPECT_EQ(link, document().linkManifest());

  // Such as spaces around the token.
  link->setAttribute(blink::HTMLNames::relAttr, " manifest ");
  EXPECT_EQ(link, document().linkManifest());

  // Check that rel=manifest actually matters.
  link->setAttribute(blink::HTMLNames::relAttr, "");
  EXPECT_EQ(link2, document().linkManifest());
  link->setAttribute(blink::HTMLNames::relAttr, "manifest");

  // Check that link outside of the <head> are ignored.
  document().head()->removeChild(link);
  document().head()->removeChild(link2);
  EXPECT_EQ(0, document().linkManifest());
  document().body()->appendChild(link);
  EXPECT_EQ(0, document().linkManifest());
  document().head()->appendChild(link);
  document().head()->appendChild(link2);

  // Check that some attribute values do not have an effect.
  link->setAttribute(blink::HTMLNames::crossoriginAttr, "use-credentials");
  EXPECT_EQ(link, document().linkManifest());
  link->setAttribute(blink::HTMLNames::hreflangAttr, "klingon");
  EXPECT_EQ(link, document().linkManifest());
  link->setAttribute(blink::HTMLNames::typeAttr, "image/gif");
  EXPECT_EQ(link, document().linkManifest());
  link->setAttribute(blink::HTMLNames::sizesAttr, "16x16");
  EXPECT_EQ(link, document().linkManifest());
  link->setAttribute(blink::HTMLNames::mediaAttr, "print");
  EXPECT_EQ(link, document().linkManifest());
}

TEST_F(DocumentTest, referrerPolicyParsing) {
  EXPECT_EQ(ReferrerPolicyDefault, document().getReferrerPolicy());

  struct TestCase {
    const char* policy;
    ReferrerPolicy expected;
    bool isLegacy;
  } tests[] = {
      {"", ReferrerPolicyDefault, false},
      // Test that invalid policy values are ignored.
      {"not-a-real-policy", ReferrerPolicyDefault, false},
      {"not-a-real-policy,also-not-a-real-policy", ReferrerPolicyDefault,
       false},
      {"not-a-real-policy,unsafe-url", ReferrerPolicyAlways, false},
      {"unsafe-url,not-a-real-policy", ReferrerPolicyAlways, false},
      // Test parsing each of the policy values.
      {"always", ReferrerPolicyAlways, true},
      {"default", ReferrerPolicyNoReferrerWhenDowngrade, true},
      {"never", ReferrerPolicyNever, true},
      {"no-referrer", ReferrerPolicyNever, false},
      {"default", ReferrerPolicyNoReferrerWhenDowngrade, true},
      {"no-referrer-when-downgrade", ReferrerPolicyNoReferrerWhenDowngrade,
       false},
      {"origin", ReferrerPolicyOrigin, false},
      {"origin-when-crossorigin", ReferrerPolicyOriginWhenCrossOrigin, true},
      {"origin-when-cross-origin", ReferrerPolicyOriginWhenCrossOrigin, false},
      {"unsafe-url", ReferrerPolicyAlways},
  };

  for (auto test : tests) {
    document().setReferrerPolicy(ReferrerPolicyDefault);
    if (test.isLegacy) {
      // Legacy keyword support must be explicitly enabled for the policy to
      // parse successfully.
      document().parseAndSetReferrerPolicy(test.policy);
      EXPECT_EQ(ReferrerPolicyDefault, document().getReferrerPolicy());
      document().parseAndSetReferrerPolicy(test.policy, true);
    } else {
      document().parseAndSetReferrerPolicy(test.policy);
    }
    EXPECT_EQ(test.expected, document().getReferrerPolicy()) << test.policy;
  }
}

TEST_F(DocumentTest, OutgoingReferrer) {
  document().setURL(KURL(KURL(), "https://www.example.com/hoge#fuga?piyo"));
  document().setSecurityOrigin(
      SecurityOrigin::create(KURL(KURL(), "https://www.example.com/")));
  EXPECT_EQ("https://www.example.com/hoge", document().outgoingReferrer());
}

TEST_F(DocumentTest, OutgoingReferrerWithUniqueOrigin) {
  document().setURL(KURL(KURL(), "https://www.example.com/hoge#fuga?piyo"));
  document().setSecurityOrigin(SecurityOrigin::createUnique());
  EXPECT_EQ(String(), document().outgoingReferrer());
}

TEST_F(DocumentTest, StyleVersion) {
  setHtmlInnerHTML(
      "<style>"
      "    .a * { color: green }"
      "    .b .c { color: green }"
      "</style>"
      "<div id='x'><span class='c'></span></div>");

  Element* element = document().getElementById("x");
  EXPECT_TRUE(element);

  uint64_t previousStyleVersion = document().styleVersion();
  element->setAttribute(blink::HTMLNames::classAttr, "notfound");
  EXPECT_EQ(previousStyleVersion, document().styleVersion());

  document().view()->updateAllLifecyclePhases();

  previousStyleVersion = document().styleVersion();
  element->setAttribute(blink::HTMLNames::classAttr, "a");
  EXPECT_NE(previousStyleVersion, document().styleVersion());

  document().view()->updateAllLifecyclePhases();

  previousStyleVersion = document().styleVersion();
  element->setAttribute(blink::HTMLNames::classAttr, "a b");
  EXPECT_NE(previousStyleVersion, document().styleVersion());
}

TEST_F(DocumentTest, EnforceSandboxFlags) {
  RefPtr<SecurityOrigin> origin =
      SecurityOrigin::createFromString("http://example.test");
  document().setSecurityOrigin(origin);
  SandboxFlags mask = SandboxNavigation;
  document().enforceSandboxFlags(mask);
  EXPECT_EQ(origin, document().getSecurityOrigin());
  EXPECT_FALSE(document().getSecurityOrigin()->isPotentiallyTrustworthy());

  mask |= SandboxOrigin;
  document().enforceSandboxFlags(mask);
  EXPECT_TRUE(document().getSecurityOrigin()->isUnique());
  EXPECT_FALSE(document().getSecurityOrigin()->isPotentiallyTrustworthy());

  // A unique origin does not bypass secure context checks unless it
  // is also potentially trustworthy.
  SchemeRegistry::registerURLSchemeBypassingSecureContextCheck(
      "very-special-scheme");
  origin =
      SecurityOrigin::createFromString("very-special-scheme://example.test");
  document().setSecurityOrigin(origin);
  document().enforceSandboxFlags(mask);
  EXPECT_TRUE(document().getSecurityOrigin()->isUnique());
  EXPECT_FALSE(document().getSecurityOrigin()->isPotentiallyTrustworthy());

  SchemeRegistry::registerURLSchemeAsSecure("very-special-scheme");
  document().setSecurityOrigin(origin);
  document().enforceSandboxFlags(mask);
  EXPECT_TRUE(document().getSecurityOrigin()->isUnique());
  EXPECT_TRUE(document().getSecurityOrigin()->isPotentiallyTrustworthy());

  origin = SecurityOrigin::createFromString("https://example.test");
  document().setSecurityOrigin(origin);
  document().enforceSandboxFlags(mask);
  EXPECT_TRUE(document().getSecurityOrigin()->isUnique());
  EXPECT_TRUE(document().getSecurityOrigin()->isPotentiallyTrustworthy());
}

TEST_F(DocumentTest, SynchronousMutationNotifier) {
  auto& observer = *new TestSynchronousMutationObserver(document());

  EXPECT_EQ(document(), observer.lifecycleContext());
  EXPECT_EQ(0, observer.countContextDestroyedCalled());

  Element* divNode = document().createElement("div");
  document().body()->appendChild(divNode);

  Element* boldNode = document().createElement("b");
  divNode->appendChild(boldNode);

  Element* italicNode = document().createElement("i");
  divNode->appendChild(italicNode);

  Node* textNode = document().createTextNode("0123456789");
  boldNode->appendChild(textNode);
  EXPECT_TRUE(observer.removedNodes().isEmpty());

  textNode->remove();
  ASSERT_EQ(1u, observer.removedNodes().size());
  EXPECT_EQ(textNode, observer.removedNodes()[0]);

  divNode->removeChildren();
  EXPECT_EQ(1u, observer.removedNodes().size())
      << "ContainerNode::removeChildren() doesn't call nodeWillBeRemoved()";
  ASSERT_EQ(1u, observer.removedChildrenNodes().size());
  EXPECT_EQ(divNode, observer.removedChildrenNodes()[0]);

  document().shutdown();
  EXPECT_EQ(nullptr, observer.lifecycleContext());
  EXPECT_EQ(1, observer.countContextDestroyedCalled());
}

TEST_F(DocumentTest, SynchronousMutationNotifieAppendChild) {
  auto& observer = *new TestSynchronousMutationObserver(document());
  document().body()->appendChild(document().createTextNode("a123456789"));
  ASSERT_EQ(1u, observer.childrenChangedNodes().size());
  EXPECT_EQ(document().body(), observer.childrenChangedNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifieInsertBefore) {
  auto& observer = *new TestSynchronousMutationObserver(document());
  document().documentElement()->insertBefore(
      document().createTextNode("a123456789"), document().body());
  ASSERT_EQ(1u, observer.childrenChangedNodes().size());
  EXPECT_EQ(document().documentElement(), observer.childrenChangedNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifierMergeTextNodes) {
  auto& observer = *new TestSynchronousMutationObserver(document());

  Text* mergeSampleA = document().createTextNode("a123456789");
  document().body()->appendChild(mergeSampleA);

  Text* mergeSampleB = document().createTextNode("b123456789");
  document().body()->appendChild(mergeSampleB);

  EXPECT_EQ(0u, observer.mergeTextNodesRecords().size());
  document().body()->normalize();

  ASSERT_EQ(1u, observer.mergeTextNodesRecords().size());
  EXPECT_EQ(mergeSampleA, observer.mergeTextNodesRecords()[0]->m_node);
  EXPECT_EQ(mergeSampleB,
            observer.mergeTextNodesRecords()[0]->m_nodeToBeRemoved);
  EXPECT_EQ(10u, observer.mergeTextNodesRecords()[0]->m_offset);
}

TEST_F(DocumentTest, SynchronousMutationNotifierMoveTreeToNewDocument) {
  auto& observer = *new TestSynchronousMutationObserver(document());

  Node* moveSample = document().createElement("div");
  moveSample->appendChild(document().createTextNode("a123"));
  moveSample->appendChild(document().createTextNode("b456"));
  document().body()->appendChild(moveSample);

  Document& anotherDocument = *Document::create();
  anotherDocument.appendChild(moveSample);

  EXPECT_EQ(1u, observer.moveTreeToNewDocumentNodes().size());
  EXPECT_EQ(moveSample, observer.moveTreeToNewDocumentNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifieRemoveChild) {
  auto& observer = *new TestSynchronousMutationObserver(document());
  document().documentElement()->removeChild(document().body());
  ASSERT_EQ(1u, observer.childrenChangedNodes().size());
  EXPECT_EQ(document().documentElement(), observer.childrenChangedNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifieReplaceChild) {
  auto& observer = *new TestSynchronousMutationObserver(document());
  Element* const replacedNode = document().body();
  document().documentElement()->replaceChild(document().createElement("div"),
                                             document().body());
  ASSERT_EQ(2u, observer.childrenChangedNodes().size());
  EXPECT_EQ(document().documentElement(), observer.childrenChangedNodes()[0]);
  EXPECT_EQ(document().documentElement(), observer.childrenChangedNodes()[1]);

  ASSERT_EQ(1u, observer.removedNodes().size());
  EXPECT_EQ(replacedNode, observer.removedNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifierSplitTextNode) {
  V8TestingScope scope;
  auto& observer = *new TestSynchronousMutationObserver(document());

  Text* splitSample = document().createTextNode("0123456789");
  document().body()->appendChild(splitSample);

  splitSample->splitText(4, ASSERT_NO_EXCEPTION);
  ASSERT_EQ(1u, observer.splitTextNodes().size());
  EXPECT_EQ(splitSample, observer.splitTextNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifierUpdateCharacterData) {
  auto& observer = *new TestSynchronousMutationObserver(document());

  Text* appendSample = document().createTextNode("a123456789");
  document().body()->appendChild(appendSample);

  Text* deleteSample = document().createTextNode("b123456789");
  document().body()->appendChild(deleteSample);

  Text* insertSample = document().createTextNode("c123456789");
  document().body()->appendChild(insertSample);

  Text* replaceSample = document().createTextNode("c123456789");
  document().body()->appendChild(replaceSample);

  EXPECT_EQ(0u, observer.updatedCharacterDataRecords().size());

  appendSample->appendData("abc");
  ASSERT_EQ(1u, observer.updatedCharacterDataRecords().size());
  EXPECT_EQ(appendSample, observer.updatedCharacterDataRecords()[0]->m_node);
  EXPECT_EQ(10u, observer.updatedCharacterDataRecords()[0]->m_offset);
  EXPECT_EQ(0u, observer.updatedCharacterDataRecords()[0]->m_oldLength);
  EXPECT_EQ(3u, observer.updatedCharacterDataRecords()[0]->m_newLength);

  deleteSample->deleteData(3, 4, ASSERT_NO_EXCEPTION);
  ASSERT_EQ(2u, observer.updatedCharacterDataRecords().size());
  EXPECT_EQ(deleteSample, observer.updatedCharacterDataRecords()[1]->m_node);
  EXPECT_EQ(3u, observer.updatedCharacterDataRecords()[1]->m_offset);
  EXPECT_EQ(4u, observer.updatedCharacterDataRecords()[1]->m_oldLength);
  EXPECT_EQ(0u, observer.updatedCharacterDataRecords()[1]->m_newLength);

  insertSample->insertData(3, "def", ASSERT_NO_EXCEPTION);
  ASSERT_EQ(3u, observer.updatedCharacterDataRecords().size());
  EXPECT_EQ(insertSample, observer.updatedCharacterDataRecords()[2]->m_node);
  EXPECT_EQ(3u, observer.updatedCharacterDataRecords()[2]->m_offset);
  EXPECT_EQ(0u, observer.updatedCharacterDataRecords()[2]->m_oldLength);
  EXPECT_EQ(3u, observer.updatedCharacterDataRecords()[2]->m_newLength);

  replaceSample->replaceData(6, 4, "ghi", ASSERT_NO_EXCEPTION);
  ASSERT_EQ(4u, observer.updatedCharacterDataRecords().size());
  EXPECT_EQ(replaceSample, observer.updatedCharacterDataRecords()[3]->m_node);
  EXPECT_EQ(6u, observer.updatedCharacterDataRecords()[3]->m_offset);
  EXPECT_EQ(4u, observer.updatedCharacterDataRecords()[3]->m_oldLength);
  EXPECT_EQ(3u, observer.updatedCharacterDataRecords()[3]->m_newLength);
}

// This tests that meta-theme-color can be found correctly
TEST_F(DocumentTest, ThemeColor) {
  {
    setHtmlInnerHTML(
        "<meta name=\"theme-color\" content=\"#00ff00\">"
        "<body>");
    EXPECT_EQ(Color(0, 255, 0), document().themeColor())
        << "Theme color should be bright green.";
  }

  {
    setHtmlInnerHTML(
        "<body>"
        "<meta name=\"theme-color\" content=\"#00ff00\">");
    EXPECT_EQ(Color(0, 255, 0), document().themeColor())
        << "Theme color should be bright green.";
  }
}

TEST_F(DocumentTest, ValidationMessageCleanup) {
  ValidationMessageClient* originalClient = &page().validationMessageClient();
  MockValidationMessageClient* mockClient = new MockValidationMessageClient();
  document().settings()->setScriptEnabled(true);
  page().setValidationMessageClient(mockClient);
  // implicitOpen()-implicitClose() makes Document.loadEventFinished()
  // true. It's necessary to kick unload process.
  document().implicitOpen(ForceSynchronousParsing);
  document().implicitClose();
  document().appendChild(document().createElement("html"));
  setHtmlInnerHTML("<body><input required></body>");
  Element* script = document().createElement("script");
  script->setTextContent(
      "window.onunload = function() {"
      "document.querySelector('input').reportValidity(); };");
  document().body()->appendChild(script);
  HTMLInputElement* input = toHTMLInputElement(document().body()->firstChild());
  DVLOG(0) << document().body()->outerHTML();

  // Sanity check.
  input->reportValidity();
  EXPECT_TRUE(mockClient->showValidationMessageWasCalled);
  mockClient->reset();

  // prepareForCommit() unloads the document, and shutdown.
  document().frame()->prepareForCommit();
  EXPECT_TRUE(mockClient->willUnloadDocumentWasCalled);
  EXPECT_TRUE(mockClient->documentDetachedWasCalled);
  // Unload handler tried to show a validation message, but it should fail.
  EXPECT_FALSE(mockClient->showValidationMessageWasCalled);

  page().setValidationMessageClient(originalClient);
}

}  // namespace blink
