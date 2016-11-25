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

#include "core/dom/SynchronousMutationObserver.h"
#include "core/dom/Text.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

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
  void contextDestroyed() final;
  void didSplitTextNode(const Text&) final;
  void didUpdateCharacterData(CharacterData*,
                              unsigned offset,
                              unsigned oldLength,
                              unsigned newLength) final;
  void nodeChildrenWillBeRemoved(ContainerNode&) final;
  void nodeWillBeRemoved(Node&) final;

  int m_contextDestroyedCalledCounter = 0;
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

void TestSynchronousMutationObserver::contextDestroyed() {
  ++m_contextDestroyedCalledCounter;
}

void TestSynchronousMutationObserver::didSplitTextNode(const Text& node) {
  m_splitTextNodes.append(&node);
}

void TestSynchronousMutationObserver::didUpdateCharacterData(
    CharacterData* characterData,
    unsigned offset,
    unsigned oldLength,
    unsigned newLength) {
  m_updatedCharacterDataRecords.append(new UpdateCharacterDataRecord(
      characterData, offset, oldLength, newLength));
}

void TestSynchronousMutationObserver::nodeChildrenWillBeRemoved(
    ContainerNode& container) {
  m_removedChildrenNodes.append(&container);
}

void TestSynchronousMutationObserver::nodeWillBeRemoved(Node& node) {
  m_removedNodes.append(&node);
}

DEFINE_TRACE(TestSynchronousMutationObserver) {
  visitor->trace(m_removedChildrenNodes);
  visitor->trace(m_removedNodes);
  visitor->trace(m_splitTextNodes);
  visitor->trace(m_updatedCharacterDataRecords);
  SynchronousMutationObserver::trace(visitor);
}

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

  EXPECT_EQ(observer.lifecycleContext(), document());
  EXPECT_EQ(observer.countContextDestroyedCalled(), 0);

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
  ASSERT_EQ(observer.removedNodes().size(), 1u);
  EXPECT_EQ(textNode, observer.removedNodes()[0]);

  divNode->removeChildren();
  EXPECT_EQ(observer.removedNodes().size(), 1u)
      << "ContainerNode::removeChildren() doesn't call nodeWillBeRemoved()";
  ASSERT_EQ(observer.removedChildrenNodes().size(), 1u);
  EXPECT_EQ(divNode, observer.removedChildrenNodes()[0]);

  document().shutdown();
  EXPECT_EQ(observer.lifecycleContext(), nullptr);
  EXPECT_EQ(observer.countContextDestroyedCalled(), 1);
}

TEST_F(DocumentTest, SynchronousMutationNotifierSplitTextNode) {
  auto& observer = *new TestSynchronousMutationObserver(document());

  Text* splitSample = document().createTextNode("0123456789");
  document().body()->appendChild(splitSample);

  splitSample->splitText(4, ASSERT_NO_EXCEPTION);
  ASSERT_EQ(observer.splitTextNodes().size(), 1u);
  EXPECT_EQ(observer.splitTextNodes()[0], splitSample);
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

  EXPECT_EQ(observer.updatedCharacterDataRecords().size(), 0u);

  appendSample->appendData("abc");
  ASSERT_EQ(observer.updatedCharacterDataRecords().size(), 1u);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[0]->m_node, appendSample);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[0]->m_offset, 10u);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[0]->m_oldLength, 0u);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[0]->m_newLength, 3u);

  deleteSample->deleteData(3, 4, ASSERT_NO_EXCEPTION);
  ASSERT_EQ(observer.updatedCharacterDataRecords().size(), 2u);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[1]->m_node, deleteSample);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[1]->m_offset, 3u);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[1]->m_oldLength, 4u);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[1]->m_newLength, 0u);

  insertSample->insertData(3, "def", ASSERT_NO_EXCEPTION);
  ASSERT_EQ(observer.updatedCharacterDataRecords().size(), 3u);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[2]->m_node, insertSample);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[2]->m_offset, 3u);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[2]->m_oldLength, 0u);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[2]->m_newLength, 3u);

  replaceSample->replaceData(6, 4, "ghi", ASSERT_NO_EXCEPTION);
  ASSERT_EQ(observer.updatedCharacterDataRecords().size(), 4u);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[3]->m_node, replaceSample);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[3]->m_offset, 6u);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[3]->m_oldLength, 4u);
  EXPECT_EQ(observer.updatedCharacterDataRecords()[3]->m_newLength, 3u);
}

}  // namespace blink
