// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"

#include "base/bind.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Not;

// Convenience matcher for list rules that sub-matches on their URLs.
class ListRuleMatcher {
 public:
  explicit ListRuleMatcher(::testing::Matcher<const Vector<KURL>&> url_matcher)
      : url_matcher_(std::move(url_matcher)) {}

  bool MatchAndExplain(const Member<SpeculationRule>& rule,
                       ::testing::MatchResultListener* listener) const {
    return MatchAndExplain(*rule, listener);
  }

  bool MatchAndExplain(const SpeculationRule& rule,
                       ::testing::MatchResultListener* listener) const {
    ::testing::StringMatchResultListener inner_listener;
    const bool matches =
        url_matcher_.MatchAndExplain(rule.urls(), &inner_listener);
    std::string inner_explanation = inner_listener.str();
    if (!inner_explanation.empty())
      *listener << "whose URLs " << inner_explanation;
    return matches;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "is a list rule whose URLs ";
    url_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "is not list rule whose URLs ";
    url_matcher_.DescribeTo(os);
  }

 private:
  ::testing::Matcher<const Vector<KURL>&> url_matcher_;
};

template <typename... Matchers>
auto MatchesListOfURLs(Matchers&&... matchers) {
  return ::testing::MakePolymorphicMatcher(
      ListRuleMatcher(ElementsAre(std::forward<Matchers>(matchers)...)));
}

MATCHER(RequiresAnonymousClientIPWhenCrossOrigin,
        negation ? "doesn't require anonymous client IP when cross origin"
                 : "requires anonymous client IP when cross origin") {
  return arg->requires_anonymous_client_ip_when_cross_origin();
}

class SpeculationRuleSetTest : public ::testing::Test {
 private:
  ScopedSpeculationRulesPrefetchProxyForTest enable_prefetch_{true};
  ScopedPrerender2ForTest enable_prerender2_{true};
};

TEST_F(SpeculationRuleSetTest, Empty) {
  auto* rule_set =
      SpeculationRuleSet::ParseInline("{}", KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, SimplePrefetchRule) {
  auto* rule_set = SpeculationRuleSet::ParseInline(
      R"({
        "prefetch": [{
          "source": "list",
          "urls": ["https://example.com/index2.html"]
        }]
      })",
      KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(
      rule_set->prefetch_rules(),
      ElementsAre(MatchesListOfURLs("https://example.com/index2.html")));
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, SimplePrerenderRule) {
  auto* rule_set = SpeculationRuleSet::ParseInline(
      R"({
        "prerender": [{
          "source": "list",
          "urls": ["https://example.com/index2.html"]
        }]
      })",
      KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(
      rule_set->prerender_rules(),
      ElementsAre(MatchesListOfURLs("https://example.com/index2.html")));
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, SimplePrefetchWithSubresourcesRule) {
  auto* rule_set = SpeculationRuleSet::ParseInline(
      R"({
        "prefetch_with_subresources": [{
          "source": "list",
          "urls": ["https://example.com/index2.html"]
        }]
      })",
      KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(
      rule_set->prefetch_with_subresources_rules(),
      ElementsAre(MatchesListOfURLs("https://example.com/index2.html")));
  EXPECT_THAT(rule_set->prerender_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, ResolvesURLs) {
  auto* rule_set = SpeculationRuleSet::ParseInline(
      R"({
        "prefetch": [{
          "source": "list",
          "urls": [
            "bar",
            "/baz",
            "//example.org/",
            "http://example.net/"
          ]
        }]
      })",
      KURL("https://example.com/foo/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(rule_set->prefetch_rules(),
              ElementsAre(MatchesListOfURLs(
                  "https://example.com/foo/bar", "https://example.com/baz",
                  "https://example.org/", "http://example.net/")));
}

TEST_F(SpeculationRuleSetTest, RequiresAnonymousClientIPWhenCrossOrigin) {
  auto* rule_set = SpeculationRuleSet::ParseInline(
      R"({
        "prefetch": [{
          "source": "list",
          "urls": ["//example.net/anonymous.html"],
          "requires": ["anonymous-client-ip-when-cross-origin"]
        }, {
          "source": "list",
          "urls": ["//example.net/direct.html"]
        }]
      })",
      KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(
      rule_set->prefetch_rules(),
      ElementsAre(AllOf(MatchesListOfURLs("https://example.net/anonymous.html"),
                        RequiresAnonymousClientIPWhenCrossOrigin()),
                  AllOf(MatchesListOfURLs("https://example.net/direct.html"),
                        Not(RequiresAnonymousClientIPWhenCrossOrigin()))));
}

TEST_F(SpeculationRuleSetTest, RejectsInvalidJSON) {
  auto* rule_set =
      SpeculationRuleSet::ParseInline("[invalid]", KURL("https://example.com"));
  EXPECT_FALSE(rule_set);
}

TEST_F(SpeculationRuleSetTest, IgnoresUnknownOrDifferentlyTypedTopLevelKeys) {
  auto* rule_set = SpeculationRuleSet::ParseInline(
      R"({
        "unrecognized_key": true,
        "prefetch": 42,
        "prefetch_with_subresources": false
      })",
      KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, DropUnrecognizedRules) {
  auto* rule_set = SpeculationRuleSet::ParseInline(
      R"({"prefetch": [)"

      // A rule that doesn't elaborate on its source.
      R"({"urls": ["no-source.html"]},)"

      // A rule with an unrecognized source.
      R"({"source": "magic-8-ball", "urls": ["no-source.html"]},)"

      // A list rule with no "urls" key.
      R"({"source": "list"},)"

      // A list rule where some URL is not a string.
      R"({"source": "list", "urls": [42]},)"

      // A rule with an unrecognized requirement.
      R"({"source": "list", "urls": ["/"], "requires": ["more-vespene-gas"]},)"

      // Invalid URLs within a list rule should be discarded.
      // This includes totally invalid ones and ones with unacceptable schemes.
      R"({"source": "list",
          "urls": [
            "valid.html", "mailto:alice@example.com", "http://@:"
           ]
         }]})",
      KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(rule_set->prefetch_rules(),
              ElementsAre(MatchesListOfURLs("https://example.com/valid.html")));
}

TEST_F(SpeculationRuleSetTest, PropagatesToDocument) {
  // A <script> with a case-insensitive type match should be propagated to the
  // document.
  // TODO(jbroman): Should we need to enable script? Should that be bypassed?
  DummyPageHolder page_holder;
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);
  Document& document = page_holder.GetDocument();
  HTMLScriptElement* script =
      MakeGarbageCollected<HTMLScriptElement>(document, CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, "SpEcUlAtIoNrUlEs");
  script->setText(
      R"({"prefetch": [
           {"source": "list", "urls": ["https://example.com/foo"]}
         ],
         "prerender": [
           {"source": "list", "urls": ["https://example.com/bar"]}
         ]
         })");
  document.head()->appendChild(script);

  auto* supplement = DocumentSpeculationRules::FromIfExists(document);
  ASSERT_TRUE(supplement);
  ASSERT_EQ(supplement->rule_sets().size(), 1u);
  SpeculationRuleSet* rule_set = supplement->rule_sets()[0];
  EXPECT_THAT(rule_set->prefetch_rules(),
              ElementsAre(MatchesListOfURLs("https://example.com/foo")));
  EXPECT_THAT(rule_set->prerender_rules(),
              ElementsAre(MatchesListOfURLs("https://example.com/bar")));
}

class StubSpeculationHost : public mojom::blink::SpeculationHost {
 public:
  using Candidates = Vector<mojom::blink::SpeculationCandidatePtr>;

  const Candidates& candidates() const { return candidates_; }
  void SetDoneClosure(base::OnceClosure done) {
    done_closure_ = std::move(done);
  }

  void BindUnsafe(mojo::ScopedMessagePipeHandle handle) {
    Bind(mojo::PendingReceiver<SpeculationHost>(std::move(handle)));
  }

  void Bind(mojo::PendingReceiver<SpeculationHost> receiver) {
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(base::BindOnce(
        &StubSpeculationHost::OnConnectionLost, base::Unretained(this)));
  }

  void UpdateSpeculationCandidates(Candidates candidates) override {
    candidates_ = std::move(candidates);
    if (done_closure_)
      std::move(done_closure_).Run();
  }

  void OnConnectionLost() {
    if (done_closure_)
      std::move(done_closure_).Run();
  }

 private:
  mojo::Receiver<SpeculationHost> receiver_{this};
  Vector<mojom::blink::SpeculationCandidatePtr> candidates_;
  base::OnceClosure done_closure_;
};

// This function adds a speculationrules script to the given page, and simulates
// the process of sending the parsed candidates to the browser.
void PropagateRulesToStubSpeculationHost(DummyPageHolder& page_holder,
                                         StubSpeculationHost& speculation_host,
                                         const String& speculation_script) {
  // A <script> with a case-insensitive type match should be propagated to the
  // browser via Mojo.
  // TODO(jbroman): Should we need to enable script? Should that be bypassed?
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);
  page_holder.GetFrame()
      .DomWindow()
      ->GetBrowserInterfaceBroker()
      .SetBinderForTesting(
          mojom::blink::SpeculationHost::Name_,
          WTF::BindRepeating(&StubSpeculationHost::BindUnsafe,
                             WTF::Unretained(&speculation_host)));

  base::RunLoop run_loop;
  speculation_host.SetDoneClosure(run_loop.QuitClosure());

  Document& document = page_holder.GetDocument();
  HTMLScriptElement* script =
      MakeGarbageCollected<HTMLScriptElement>(document, CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, "SpEcUlAtIoNrUlEs");
  script->setText(speculation_script);
  document.head()->appendChild(script);

  run_loop.Run();

  page_holder.GetFrame()
      .DomWindow()
      ->GetBrowserInterfaceBroker()
      .SetBinderForTesting(mojom::blink::SpeculationHost::Name_, {});
}

TEST_F(SpeculationRuleSetTest, PropagatesAllRulesToBrowser) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  const String speculation_script =
      R"({"prefetch_with_subresources": [
           {"source": "list",
            "urls": ["https://example.com/foo", "https://example.com/bar"],
            "requires": ["anonymous-client-ip-when-cross-origin"]}
         ],
          "prerender": [
           {"source": "list", "urls": ["https://example.com/prerender"]}
         ]
         })";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);

  const auto& candidates = speculation_host.candidates();
  ASSERT_EQ(candidates.size(), 3u);
  {
    const auto& candidate = candidates[0];
    EXPECT_EQ(candidate->action,
              mojom::blink::SpeculationAction::kPrefetchWithSubresources);
    EXPECT_EQ(candidate->url, "https://example.com/foo");
    EXPECT_TRUE(candidate->requires_anonymous_client_ip_when_cross_origin);
  }
  {
    const auto& candidate = candidates[1];
    EXPECT_EQ(candidate->action,
              mojom::blink::SpeculationAction::kPrefetchWithSubresources);
    EXPECT_EQ(candidate->url, "https://example.com/bar");
    EXPECT_TRUE(candidate->requires_anonymous_client_ip_when_cross_origin);
  }
  {
    const auto& candidate = candidates[2];
    EXPECT_EQ(candidate->action, mojom::blink::SpeculationAction::kPrerender);
    EXPECT_EQ(candidate->url, "https://example.com/prerender");
  }
}

// Tests that prefetch rules are ignored unless SpeculationRulesPrefetchProxy
// is enabled.
TEST_F(SpeculationRuleSetTest, PrerenderIgnorePrefetchRules) {
  // Overwrite the kSpeculationRulesPrefetchProxy flag.
  ScopedSpeculationRulesPrefetchProxyForTest enable_prefetch{false};

  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  const String speculation_script =
      R"({"prefetch_with_subresources": [
           {"source": "list",
            "urls": ["https://example.com/foo", "https://example.com/bar"],
            "requires": ["anonymous-client-ip-when-cross-origin"]}
         ],
          "prerender": [
           {"source": "list", "urls": ["https://example.com/prerender"]}
         ]
         })";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);

  const auto& candidates = speculation_host.candidates();
  EXPECT_EQ(candidates.size(), 1u);
  EXPECT_FALSE(base::ranges::any_of(candidates, [](const auto& candidate) {
    return candidate->action ==
           mojom::blink::SpeculationAction::kPrefetchWithSubresources;
  }));
}

// Tests that prerender rules are ignored unless Prerender2 is enabled.
TEST_F(SpeculationRuleSetTest, PrefetchIgnorePrerenderRules) {
  // Overwrite the kPrerender2 flag.
  ScopedPrerender2ForTest enable_prerender{false};

  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  const String speculation_script =
      R"({"prefetch_with_subresources": [
           {"source": "list",
            "urls": ["https://example.com/foo", "https://example.com/bar"],
            "requires": ["anonymous-client-ip-when-cross-origin"]}
         ],
          "prerender": [
           {"source": "list", "urls": ["https://example.com/prerender"]}
         ]
         })";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);

  const auto& candidates = speculation_host.candidates();
  EXPECT_EQ(candidates.size(), 2u);
  EXPECT_FALSE(base::ranges::any_of(candidates, [](const auto& candidate) {
    return candidate->action == mojom::blink::SpeculationAction::kPrerender;
  }));
}

// Tests that the presence of a speculationrules script is recorded.
TEST_F(SpeculationRuleSetTest, UseCounter) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);
  EXPECT_FALSE(
      page_holder.GetDocument().IsUseCounted(WebFeature::kSpeculationRules));

  const String speculation_script =
      R"({"prefetch": [{"source": "list", "urls": ["/foo"]}]})";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  EXPECT_TRUE(
      page_holder.GetDocument().IsUseCounted(WebFeature::kSpeculationRules));
}

}  // namespace
}  // namespace blink
