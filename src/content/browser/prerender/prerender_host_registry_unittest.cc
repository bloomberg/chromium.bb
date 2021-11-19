// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host_registry.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/speculation_rules/speculation_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_commit_deferring_condition.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/load_flags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

blink::mojom::SpeculationCandidatePtr CreatePrerenderCandidate(
    const GURL& url) {
  auto candidate = blink::mojom::SpeculationCandidate::New();
  candidate->action = blink::mojom::SpeculationAction::kPrerender;
  candidate->url = url;
  candidate->referrer = blink::mojom::Referrer::New();
  return candidate;
}

void SendCandidate(const GURL& url,
                   mojo::Remote<blink::mojom::SpeculationHost>& remote) {
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(CreatePrerenderCandidate(url));
  remote->UpdateSpeculationCandidates(std::move(candidates));
  remote.FlushForTesting();
}

PrerenderAttributes GeneratePrerenderAttributes(const GURL& url,
                                                RenderFrameHostImpl* rfh) {
  return PrerenderAttributes(url, PrerenderTriggerType::kSpeculationRule,
                             Referrer(), rfh->GetLastCommittedOrigin(),
                             rfh->GetLastCommittedURL(),
                             rfh->GetProcess()->GetID(), rfh->GetFrameToken(),
                             rfh->GetPageUkmSourceId());
}

// This definition is needed because this constant is odr-used in gtest macros.
// https://en.cppreference.com/w/cpp/language/static#Constant_static_members
const int kNoFrameTreeNodeId = RenderFrameHost::kNoFrameTreeNodeId;

// TODO(nhiroki): Merge this into TestNavigationObserver for code
// simplification.
class ActivationObserver : public PrerenderHost::Observer {
 public:
  // PrerenderHost::Observer implementations.
  void OnActivated() override { was_activated_ = true; }
  void OnHostDestroyed() override {
    was_host_destroyed_ = true;
    if (quit_closure_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(quit_closure_));
    }
  }

  void WaitUntilHostDestroyed() {
    if (was_host_destroyed_)
      return;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool was_activated() const { return was_activated_; }

 private:
  base::OnceClosure quit_closure_;
  bool was_activated_ = false;
  bool was_host_destroyed_ = false;
};

std::unique_ptr<NavigationSimulatorImpl> CreateActivation(
    const GURL& prerendering_url,
    WebContentsImpl& web_contents) {
  std::unique_ptr<NavigationSimulatorImpl> navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(
          prerendering_url, web_contents.GetMainFrame());
  navigation->SetReferrer(blink::mojom::Referrer::New(
      web_contents.GetMainFrame()->GetLastCommittedURL(),
      network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
  return navigation;
}

void ActivatePrerenderedPage(const GURL& prerendering_url,
                             WebContentsImpl& web_contents) {
  // Make sure the page for `prerendering_url` has been prerendered.
  PrerenderHostRegistry* registry = web_contents.GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(prerendering_url);
  EXPECT_TRUE(prerender_host);
  int prerender_host_id = prerender_host->frame_tree_node_id();

  ActivationObserver activation_observer;
  prerender_host->AddObserver(&activation_observer);

  // Activate the prerendered page.
  std::unique_ptr<NavigationSimulatorImpl> navigation =
      CreateActivation(prerendering_url, web_contents);
  navigation->Commit();
  activation_observer.WaitUntilHostDestroyed();

  EXPECT_EQ(web_contents.GetMainFrame()->GetLastCommittedURL(),
            prerendering_url);

  EXPECT_TRUE(activation_observer.was_activated());
  EXPECT_EQ(registry->FindReservedHostById(prerender_host_id), nullptr);
}

// Finish a prerendering navigation that was already started with
// CreateAndStartHost().
void CommitPrerenderNavigation(PrerenderHost& host) {
  // Normally we could use EmbeddedTestServer to provide a response, but these
  // tests use RenderViewHostImplTestHarness so the load goes through a
  // TestNavigationURLLoader which we don't have access to in order to
  // complete. Use NavigationSimulator to finish the navigation.
  FrameTreeNode* ftn = FrameTreeNode::From(host.GetPrerenderedMainFrameHost());
  std::unique_ptr<NavigationSimulator> sim =
      NavigationSimulatorImpl::CreateFromPendingInFrame(ftn);
  sim->Commit();
  EXPECT_TRUE(host.is_ready_for_activation());
}

class PrerenderWebContentsDelegate : public WebContentsDelegate {
 public:
  PrerenderWebContentsDelegate() = default;

  bool IsPrerender2Supported() override { return true; }
};

class PrerenderHostRegistryTest : public RenderViewHostImplTestHarness {
 public:
  PrerenderHostRegistryTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kPrerender2},
        // Disable the memory requirement of Prerender2 so the test can run on
        // any bot.
        {blink::features::kPrerender2MemoryControls});
  }
  ~PrerenderHostRegistryTest() override = default;

  std::unique_ptr<TestWebContents> CreateWebContents(const GURL& url) {
    std::unique_ptr<TestWebContents> web_contents(TestWebContents::Create(
        GetBrowserContext(), SiteInstanceImpl::Create(GetBrowserContext())));
    web_contents->SetDelegate(&web_contents_delegate_);
    web_contents->NavigateAndCommit(url);
    return web_contents;
  }

  RenderFrameHostImpl* NavigatePrimaryPage(TestWebContents* web_contents,
                                           const GURL& dest_url) {
    std::unique_ptr<NavigationSimulatorImpl> navigation =
        NavigationSimulatorImpl::CreateRendererInitiated(
            dest_url, web_contents->GetMainFrame());
    navigation->SetTransition(ui::PAGE_TRANSITION_LINK);
    navigation->Start();
    navigation->Commit();
    RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
    EXPECT_EQ(render_frame_host->GetLastCommittedURL(), dest_url);
    return render_frame_host;
  }

  // Helper method to test the navigation param matching logic which allows a
  // prerender host to be used in a potential activation navigation only if its
  // params match the potential activation navigation params. Use setup_callback
  // to set the parameters. Returns true if the host was selected as a
  // potential candidate for activation, and false otherwise.
  bool CheckIsActivatedForParams(
      base::OnceCallback<void(NavigationSimulatorImpl*)> setup_callback)
      WARN_UNUSED_RESULT {
    const GURL kOriginalUrl("https://example.com/");

    std::unique_ptr<TestWebContents> web_contents =
        CreateWebContents(kOriginalUrl);
    RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();

    const GURL kPrerenderingUrl("https://example.com/next");

    PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
    registry->CreateAndStartHost(
        GeneratePrerenderAttributes(kPrerenderingUrl, render_frame_host),
        web_contents.get());
    PrerenderHost* prerender_host =
        registry->FindHostByUrlForTesting(kPrerenderingUrl);
    CommitPrerenderNavigation(*prerender_host);

    std::unique_ptr<NavigationSimulatorImpl> navigation =
        NavigationSimulatorImpl::CreateRendererInitiated(kPrerenderingUrl,
                                                         render_frame_host);
    // Set a default referrer policy that matches the initial prerender
    // navigation.
    // TODO(falken): Fix NavigationSimulatorImpl to do this itself.
    navigation->SetReferrer(blink::mojom::Referrer::New(
        web_contents->GetMainFrame()->GetLastCommittedURL(),
        network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));

    // Change a parameter to differentiate the activation request from the
    // prerendering request.
    std::move(setup_callback).Run(navigation.get());
    navigation->Start();
    NavigationRequest* navigation_request = navigation->GetNavigationHandle();
    // Use is_potentially_prerendered_page_activation_for_testing() instead of
    // IsPrerenderedPageActivation() because the NavigationSimulator does not
    // proceed past CommitDeferringConditions on potential activations,
    // so IsPrerenderedPageActivation() will fail with a CHECK because
    // prerender_frame_tree_node_id_ is not populated.
    // TODO(https://crbug.com/1239220): Fix NavigationSimulator to wait for
    // commit deferring conditions as it does throttles.
    return navigation_request
        ->is_potentially_prerendered_page_activation_for_testing();
  }

  // Helper method to perform a prerender activation that includes specialized
  // handling or setup on the initial prerender navigation via the
  // setup_callback parameter.
  void SetupPrerenderAndCommit(
      base::OnceCallback<void(NavigationSimulatorImpl*)> setup_callback) {
    const GURL kOriginalUrl("https://example.com/");

    TestWebContents* wc = static_cast<TestWebContents*>(web_contents());
    wc->NavigateAndCommit(kOriginalUrl);
    RenderFrameHostImpl* render_frame_host = wc->GetMainFrame();
    ASSERT_TRUE(render_frame_host);

    const GURL kPrerenderingUrl("https://example.com/next");

    PrerenderHostRegistry* registry = wc->GetPrerenderHostRegistry();
    const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
        GeneratePrerenderAttributes(kPrerenderingUrl, render_frame_host), wc);
    ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
    PrerenderHost* prerender_host =
        registry->FindNonReservedHostById(prerender_frame_tree_node_id);

    // Complete the initial prerender navigation.
    FrameTreeNode* ftn =
        FrameTreeNode::From(prerender_host->GetPrerenderedMainFrameHost());
    std::unique_ptr<NavigationSimulatorImpl> sim =
        NavigationSimulatorImpl::CreateFromPendingInFrame(ftn);
    std::move(setup_callback).Run(sim.get());
    sim->Commit();
    EXPECT_TRUE(prerender_host->is_ready_for_activation());

    // Activate the prerendered page.
    ActivatePrerenderedPage(kPrerenderingUrl, *wc);
  }

  void ExpectUniqueSampleOfFinalStatus(PrerenderHost::FinalStatus status) {
    histogram_tester_.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus", status, 1);
  }

  void ExpectBucketCountOfFinalStatus(PrerenderHost::FinalStatus status) {
    histogram_tester_.ExpectBucketCount(
        "Prerender.Experimental.PrerenderHostFinalStatus", status, 1);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  PrerenderWebContentsDelegate web_contents_delegate_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PrerenderHostRegistryTest, CreateAndStartHost) {
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, render_frame_host),
      web_contents.get());
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host);

  ActivatePrerenderedPage(kPrerenderingUrl, *web_contents);
}

TEST_F(PrerenderHostRegistryTest, CreateAndStartHostForSameURL) {
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int frame_tree_node_id1 = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, render_frame_host),
      web_contents.get());
  PrerenderHost* prerender_host1 =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);

  // Start the prerender host for the same URL. This second host should be
  // ignored, and the first host should still be findable.
  const int frame_tree_node_id2 = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, render_frame_host),
      web_contents.get());
  EXPECT_EQ(frame_tree_node_id1, frame_tree_node_id2);
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl),
            prerender_host1);
  CommitPrerenderNavigation(*prerender_host1);

  ActivatePrerenderedPage(kPrerenderingUrl, *web_contents);
}

// Tests that PrerenderHostRegistry limits the number of started prerenders
// to 1.
TEST_F(PrerenderHostRegistryTest, NumberLimit_Activation) {
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  // After the first prerender page was activated, PrerenderHostRegistry can
  // start prerendering a new one.
  const GURL kPrerenderingUrl1("https://example.com/next1");
  const GURL kPrerenderingUrl2("https://example.com/next2");

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  int frame_tree_node_id1 = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl1, render_frame_host),
      web_contents.get());
  int frame_tree_node_id2 = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl2, render_frame_host),
      web_contents.get());
  ExpectUniqueSampleOfFinalStatus(
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded);

  // PrerenderHostRegistry should only start prerendering for kPrerenderingUrl1.
  EXPECT_NE(frame_tree_node_id1, kNoFrameTreeNodeId);
  EXPECT_EQ(frame_tree_node_id2, kNoFrameTreeNodeId);

  // Activate the first prerender.
  PrerenderHost* prerender_host1 =
      registry->FindHostByUrlForTesting(kPrerenderingUrl1);
  CommitPrerenderNavigation(*prerender_host1);
  ActivatePrerenderedPage(kPrerenderingUrl1, *web_contents);

  // After the first prerender page was activated, PrerenderHostRegistry can
  // start prerendering a new one.
  frame_tree_node_id2 = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl2, render_frame_host),
      web_contents.get());
  EXPECT_NE(frame_tree_node_id2, kNoFrameTreeNodeId);
  ExpectBucketCountOfFinalStatus(
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded);
}

// Tests that PrerenderHostRegistry limits the number of started prerenders
// to 1, and new candidates can be processed after the initiator page navigates
// to a new same-origin page.
TEST_F(PrerenderHostRegistryTest, NumberLimit_SameOriginNavigateAway) {
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  mojo::Remote<blink::mojom::SpeculationHost> remote1;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote1.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(remote1.is_connected());
  const GURL kPrerenderingUrl1("https://example.com/next1");
  const GURL kPrerenderingUrl2("https://example.com/next2");
  SendCandidate(kPrerenderingUrl1, remote1);
  SendCandidate(kPrerenderingUrl2, remote1);
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  // PrerenderHostRegistry should only start prerendering for kPrerenderingUrl1.
  ASSERT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  ASSERT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);
  ExpectUniqueSampleOfFinalStatus(
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded);

  // The initiator document navigates away.
  render_frame_host = NavigatePrimaryPage(
      web_contents.get(), GURL("https://example.com/elsewhere"));
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);

  // After the initiator page navigates away, the started prerendering should be
  // cancelled, and PrerenderHostRegistry can start prerendering a new one.
  mojo::Remote<blink::mojom::SpeculationHost> remote2;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote2.BindNewPipeAndPassReceiver());
  SendCandidate(kPrerenderingUrl2, remote2);

  EXPECT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);
  ExpectBucketCountOfFinalStatus(
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded);
}

// Tests that PrerenderHostRegistry limits the number of started prerenders
// to 1, and new candidates can be processed after the initiator page navigates
// to a new cross-origin page.
TEST_F(PrerenderHostRegistryTest, NumberLimit_CrossOriginNavigateAway) {
  const GURL kOriginalUrl("https://example.com/");

  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  mojo::Remote<blink::mojom::SpeculationHost> remote1;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote1.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(remote1.is_connected());
  const GURL kPrerenderingUrl1("https://example.com/next1");
  const GURL kPrerenderingUrl2("https://example.com/next2");
  SendCandidate(kPrerenderingUrl1, remote1);
  SendCandidate(kPrerenderingUrl2, remote1);
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  // PrerenderHostRegistry should only start prerendering for kPrerenderingUrl1.
  ASSERT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  ASSERT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);
  ExpectUniqueSampleOfFinalStatus(
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded);

  // The initiator document navigates away to a cross-origin page.
  render_frame_host =
      NavigatePrimaryPage(web_contents.get(), GURL("https://example.org/"));
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);

  // After the initiator page navigates away, the started prerendering should be
  // cancelled, and PrerenderHostRegistry can start prerendering a new one.
  mojo::Remote<blink::mojom::SpeculationHost> remote2;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote2.BindNewPipeAndPassReceiver());
  const GURL kPrerenderingUrl3("https://example.org/next1");
  SendCandidate(kPrerenderingUrl3, remote2);
  EXPECT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl3), nullptr);
  ExpectBucketCountOfFinalStatus(
      PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded);
}

TEST_F(PrerenderHostRegistryTest,
       ReserveHostToActivateBeforeReadyForActivation) {
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, render_frame_host),
      web_contents.get());
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);
  FrameTreeNode* ftn =
      FrameTreeNode::From(prerender_host->GetPrerenderedMainFrameHost());
  std::unique_ptr<NavigationSimulatorImpl> sim =
      NavigationSimulatorImpl::CreateFromPendingInFrame(ftn);
  // Ensure that navigation in prerendering frame tree does not commit and
  // PrerenderHost doesn't become ready for activation.
  sim->SetAutoAdvance(false);

  EXPECT_FALSE(prerender_host->is_ready_for_activation());

  ActivationObserver activation_observer;
  prerender_host->AddObserver(&activation_observer);

  // Start activation.
  std::unique_ptr<NavigationSimulatorImpl> navigation =
      CreateActivation(kPrerenderingUrl, *web_contents);
  navigation->Start();

  // Wait until PrerenderCommitDeferringCondition runs.
  // TODO(nhiroki): Avoid using base::RunUntilIdle() and instead use some
  // explicit signal of the running condition.
  base::RunLoop().RunUntilIdle();

  // The activation should be deferred by PrerenderCommitDeferringCondition
  // until the main frame navigation in the prerendering frame tree finishes.
  NavigationRequest* navigation_request = navigation->GetNavigationHandle();
  EXPECT_TRUE(
      navigation_request->IsCommitDeferringConditionDeferredForTesting());
  EXPECT_FALSE(activation_observer.was_activated());
  EXPECT_EQ(web_contents->GetMainFrame()->GetLastCommittedURL(), kOriginalUrl);

  // Finish the main frame navigation.
  sim->Commit();

  // Finish the activation.
  activation_observer.WaitUntilHostDestroyed();
  EXPECT_TRUE(activation_observer.was_activated());
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
  EXPECT_EQ(web_contents->GetMainFrame()->GetLastCommittedURL(),
            kPrerenderingUrl);
}

TEST_F(PrerenderHostRegistryTest, CancelHost) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, render_frame_host),
      web_contents.get());
  EXPECT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  registry->CancelHost(prerender_frame_tree_node_id,
                       PrerenderHost::FinalStatus::kDestroyed);
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

// Test cancelling a prerender while a CommitDeferringCondition is running.
// This activation should fall back to a regular navigation.
TEST_F(PrerenderHostRegistryTest,
       CancelHostWhileCommitDeferringConditionIsRunning) {
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  // Start prerendering.
  const GURL kPrerenderingUrl("https://example.com/next");

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, render_frame_host),
      web_contents.get());
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host);

  ActivationObserver activation_observer;
  prerender_host->AddObserver(&activation_observer);

  // Now navigate the primary page to the prerendered URL so that we activate
  // the prerender. Use a CommitDeferringCondition to pause activation
  // before it completes.
  std::unique_ptr<NavigationSimulatorImpl> navigation;
  MockCommitDeferringConditionWrapper condition(/*is_ready_to_commit=*/false);
  {
    MockCommitDeferringConditionInstaller installer(condition.PassToDelegate());

    // Start trying to activate the prerendered page.
    navigation = CreateActivation(kPrerenderingUrl, *web_contents);
    navigation->Start();

    // Wait for the condition to pause the activation.
    condition.WaitUntilInvoked();
  }

  // The request should be deferred by the condition.
  NavigationRequest* navigation_request =
      static_cast<NavigationRequest*>(navigation->GetNavigationHandle());
  EXPECT_TRUE(
      navigation_request->IsCommitDeferringConditionDeferredForTesting());

  // The primary page should still be the original page.
  EXPECT_EQ(web_contents->GetLastCommittedURL(), kOriginalUrl);

  // Cancel the prerender while the CommitDeferringCondition is running.
  registry->CancelHost(prerender_frame_tree_node_id,
                       PrerenderHost::FinalStatus::kDestroyed);
  activation_observer.WaitUntilHostDestroyed();
  EXPECT_FALSE(activation_observer.was_activated());
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  // Resume the activation. This should fall back to a regular navigation.
  condition.CallResumeClosure();
  navigation->Commit();
  EXPECT_EQ(web_contents->GetMainFrame()->GetLastCommittedURL(),
            kPrerenderingUrl);
}

// Test cancelling a prerender and then starting a new prerender for the same
// URL while a CommitDeferringCondition is running. This activation should not
// reserve the second prerender and should fall back to a regular navigation.
TEST_F(PrerenderHostRegistryTest,
       CancelAndStartHostWhileCommitDeferringConditionIsRunning) {
  const GURL kOriginalUrl("https://example.com/");
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(kOriginalUrl);
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");

  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();

  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, render_frame_host),
      web_contents.get());
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host);

  ActivationObserver activation_observer;
  prerender_host->AddObserver(&activation_observer);

  // Now navigate the primary page to the prerendered URL so that we activate
  // the prerender. Use a CommitDeferringCondition to pause activation
  // before it completes.
  std::unique_ptr<NavigationSimulatorImpl> navigation;
  MockCommitDeferringConditionWrapper condition(/*is_ready_to_commit=*/false);
  {
    MockCommitDeferringConditionInstaller installer(condition.PassToDelegate());

    // Start trying to activate the prerendered page.
    navigation = CreateActivation(kPrerenderingUrl, *web_contents);
    navigation->Start();

    // Wait for the condition to pause the activation.
    condition.WaitUntilInvoked();
  }

  // The request should be deferred by the condition.
  NavigationRequest* navigation_request =
      static_cast<NavigationRequest*>(navigation->GetNavigationHandle());
  EXPECT_TRUE(
      navigation_request->IsCommitDeferringConditionDeferredForTesting());

  // The primary page should still be the original page.
  EXPECT_EQ(web_contents->GetLastCommittedURL(), kOriginalUrl);

  // Cancel the prerender while the CommitDeferringCondition is running.
  registry->CancelHost(prerender_frame_tree_node_id,
                       PrerenderHost::FinalStatus::kDestroyed);
  activation_observer.WaitUntilHostDestroyed();
  EXPECT_FALSE(activation_observer.was_activated());
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  // Start the second prerender for the same URL.
  const int prerender_frame_tree_node_id2 = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, render_frame_host),
      web_contents.get());
  ASSERT_NE(prerender_frame_tree_node_id2, kNoFrameTreeNodeId);
  PrerenderHost* prerender_host2 =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host2);

  EXPECT_NE(prerender_frame_tree_node_id, prerender_frame_tree_node_id2);

  // Resume the activation. This should not reserve the second prerender and
  // should fall back to a regular navigation.
  condition.CallResumeClosure();
  navigation->Commit();
  EXPECT_EQ(web_contents->GetMainFrame()->GetLastCommittedURL(),
            kPrerenderingUrl);

  // The second prerender should still exist.
  EXPECT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

TEST_F(PrerenderHostRegistryTest,
       DontStartPrerenderWhenTriggerIsAlreadyHidden) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  // The visibility state to be HIDDEN will cause prerendering not started.
  web_contents->WasHidden();

  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  RenderFrameHostImpl* initiator_rfh = web_contents->GetMainFrame();
  PrerenderHostRegistry* registry = web_contents->GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id = registry->CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl, initiator_rfh),
      web_contents.get());
  EXPECT_EQ(prerender_frame_tree_node_id, RenderFrameHost::kNoFrameTreeNodeId);
  PrerenderHost* prerender_host =
      registry->FindNonReservedHostById(prerender_frame_tree_node_id);
  EXPECT_EQ(prerender_host, nullptr);
  ExpectUniqueSampleOfFinalStatus(
      PrerenderHost::FinalStatus::kTriggerBackgrounded);
}

// -------------------------------------------------
// Activation navigation parameter matching unit tests.
// These tests change a parameter to differentiate the activation request from
// the prerendering request.

// A positive test to show that if the navigation params are equal then the
// prerender host is selected for activation.
TEST_F(PrerenderHostRegistryTest, SameInitialAndActivationParams) {
  EXPECT_TRUE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        // Do not change any params, so activation happens.
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_InitiatorFrameToken) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        const GURL kOriginalUrl("https://example.com/");
        navigation->SetInitiatorFrame(nullptr);
        navigation->set_initiator_origin(url::Origin::Create(kOriginalUrl));
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_Headers) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_request_headers("User-Agent: Test");
      })));
}

// Tests that the Purpose header is ignored when comparing request headers.
TEST_F(PrerenderHostRegistryTest, PurposeHeaderIsIgnoredForParamMatching) {
  EXPECT_TRUE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_request_headers("Purpose: Test");
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_LoadFlags) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_load_flags(net::LOAD_ONLY_FROM_CACHE);
      })));

  // If the potential activation request requires validation or bypass of the
  // browser cache, the prerendered page should not be activated.
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_load_flags(net::LOAD_VALIDATE_CACHE);
      })));
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_load_flags(net::LOAD_BYPASS_CACHE);
      })));
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_load_flags(net::LOAD_DISABLE_CACHE);
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_SkipServiceWorker) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_skip_service_worker(true);
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_MixedContentContextType) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_mixed_content_context_type(
            blink::mojom::MixedContentContextType::kNotMixedContent);
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_IsFormSubmission) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->SetIsFormSubmission(true);
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_SearchableFormUrl) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        const GURL kOriginalUrl("https://example.com/");
        navigation->set_searchable_form_url(kOriginalUrl);
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_SearchableFormEncoding) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_searchable_form_encoding("Test encoding");
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_InitiatorOrigin) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_initiator_origin(url::Origin());
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_ShouldCheckMainWorldCSP) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_should_check_main_world_csp(
            network::mojom::CSPDisposition::DO_NOT_CHECK);
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_Method) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->SetMethod("POST");
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_HrefTranslate) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_href_translate("test");
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_Transition) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->SetTransition(ui::PAGE_TRANSITION_FORM_SUBMIT);
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_RequestContextType) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_request_context_type(
            blink::mojom::RequestContextType::AUDIO);
      })));
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_ReferrerPolicy) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([&](NavigationSimulatorImpl* navigation) {
        navigation->SetReferrer(blink::mojom::Referrer::New(
            web_contents()->GetMainFrame()->GetLastCommittedURL(),
            network::mojom::ReferrerPolicy::kAlways));
      })));
}

// End navigation parameter matching tests ---------

// Begin replication state matching tests ----------

TEST_F(PrerenderHostRegistryTest, InsecureRequestPolicyIsSetWhilePrerendering) {
  SetupPrerenderAndCommit(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_insecure_request_policy(
            blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent);
      }));
  EXPECT_EQ(static_cast<TestWebContents*>(web_contents())
                ->GetMainFrame()
                ->frame_tree_node()
                ->current_replication_state()
                .insecure_request_policy,
            blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent);
}

TEST_F(PrerenderHostRegistryTest,
       InsecureNavigationsSetIsSetWhilePrerendering) {
  SetupPrerenderAndCommit(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        const std::vector<uint32_t> insecure_navigations = {1, 2};
        navigation->set_insecure_navigations_set(insecure_navigations);
      }));
  const std::vector<uint32_t> insecure_navigations = {1, 2};
  EXPECT_EQ(static_cast<TestWebContents*>(web_contents())
                ->GetMainFrame()
                ->frame_tree_node()
                ->current_replication_state()
                .insecure_navigations_set,
            insecure_navigations);
}

TEST_F(PrerenderHostRegistryTest,
       HasPotentiallyTrustworthyUniqueOriginIsSetWhilePrerendering) {
  SetupPrerenderAndCommit(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_has_potentially_trustworthy_unique_origin(true);
      }));
  EXPECT_TRUE(static_cast<TestWebContents*>(web_contents())
                  ->GetMainFrame()
                  ->frame_tree_node()
                  ->current_replication_state()
                  .has_potentially_trustworthy_unique_origin);
}

// End replication state matching tests ------------

}  // namespace
}  // namespace content
