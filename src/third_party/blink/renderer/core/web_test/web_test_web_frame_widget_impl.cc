// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/web_test/web_test_web_frame_widget_impl.h"

#include "content/web_test/renderer/event_sender.h"
#include "content/web_test/renderer/test_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_supplement.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

namespace blink {

WebFrameWidget* FrameWidgetTestHelper::CreateTestWebFrameWidget(
    base::PassKey<WebLocalFrame> pass_key,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const viz::FrameSinkId& frame_sink_id,
    bool hidden,
    bool never_composited,
    bool is_for_child_local_root,
    bool is_for_nested_main_frame,
    content::TestRunner* test_runner) {
  return MakeGarbageCollected<WebTestWebFrameWidgetImpl>(
      pass_key, std::move(frame_widget_host), std::move(frame_widget),
      std::move(widget_host), std::move(widget), std::move(task_runner),
      frame_sink_id, hidden, never_composited, is_for_child_local_root,
      is_for_nested_main_frame, test_runner);
}

WebTestWebFrameWidgetImpl::WebTestWebFrameWidgetImpl(
    base::PassKey<WebLocalFrame> pass_key,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const viz::FrameSinkId& frame_sink_id,
    bool hidden,
    bool never_composited,
    bool is_for_child_local_root,
    bool is_for_nested_main_frame,
    content::TestRunner* test_runner)
    : WebFrameWidgetImpl(pass_key,
                         std::move(frame_widget_host),
                         std::move(frame_widget),
                         std::move(widget_host),
                         std::move(widget),
                         std::move(task_runner),
                         frame_sink_id,
                         hidden,
                         never_composited,
                         is_for_child_local_root,
                         is_for_nested_main_frame),
      test_runner_(test_runner) {}

WebTestWebFrameWidgetImpl::~WebTestWebFrameWidgetImpl() = default;

void WebTestWebFrameWidgetImpl::BindLocalRoot(WebLocalFrame& local_root) {
  WebFrameWidgetImpl::BindLocalRoot(local_root);
  // We need to initialize EventSender after the binding of the local root
  // as the EventSender constructor accesses LocalRoot and that is not
  // set until BindLocalRoot is called.
  event_sender_ = std::make_unique<content::EventSender>(this, test_runner_);
}

void WebTestWebFrameWidgetImpl::WillBeginMainFrame() {
  // WillBeginMainFrame occurs before we run BeginMainFrame() in the base
  // class, which will change states. TestFinished() wants to grab the current
  // state.
  GetTestRunner()->FinishTestIfReady();

  WebFrameWidgetImpl::WillBeginMainFrame();
}

void WebTestWebFrameWidgetImpl::ScheduleAnimation() {
  if (GetTestRunner()->TestIsRunning())
    ScheduleAnimationInternal(GetTestRunner()->animation_requires_raster());
}

void WebTestWebFrameWidgetImpl::ScheduleAnimationForWebTests() {
  // Single threaded web tests must explicitly schedule commits.
  //
  // Pass true for |do_raster| to ensure the compositor is actually run, rather
  // than just doing the main frame animate step. That way we know it will
  // submit a frame and later trigger the presentation callback in order to make
  // progress in the test.
  if (GetTestRunner()->TestIsRunning())
    ScheduleAnimationInternal(/*do_raster=*/true);
}

void WebTestWebFrameWidgetImpl::UpdateAllLifecyclePhasesAndComposite(
    base::OnceClosure callback) {
  LayerTreeHost()->RequestPresentationTimeForNextFrame(WTF::Bind(
      [](base::OnceClosure callback, const gfx::PresentationFeedback&) {
        std::move(callback).Run();
      },
      std::move(callback)));
  LayerTreeHost()->SetNeedsCommitWithForcedRedraw();
  ScheduleAnimationForWebTests();
}

void WebTestWebFrameWidgetImpl::DisableEndDocumentTransition() {
  DocumentTransitionSupplement::documentTransition(
      *LocalRootImpl()->GetFrame()->GetDocument())
      ->DisableEndTransition();
}

void WebTestWebFrameWidgetImpl::ScheduleAnimationInternal(bool do_raster) {
  // When using threaded compositing, have the WeFrameWidgetImpl normally
  // schedule a request for a frame, as we use the compositor's scheduler.
  if (scheduler::WebThreadScheduler::CompositorThreadScheduler()) {
    WebFrameWidgetImpl::ScheduleAnimation();
    return;
  }

  // If an animation already scheduled we'll make it composite, otherwise we'll
  // schedule another animation step with composite now.
  composite_requested_ |= do_raster;

  if (!animation_scheduled_) {
    animation_scheduled_ = true;

    WebLocalFrame* frame = LocalRoot();

    frame->GetTaskRunner(TaskType::kInternalTest)
        ->PostDelayedTask(FROM_HERE,
                          WTF::Bind(&WebTestWebFrameWidgetImpl::AnimateNow,
                                    WrapWeakPersistent(this)),
                          base::Milliseconds(1));
  }
}

void WebTestWebFrameWidgetImpl::StartDragging(const WebDragData& data,
                                              DragOperationsMask mask,
                                              const SkBitmap& drag_image,
                                              const gfx::Point& image_offset) {
  doing_drag_and_drop_ = true;
  GetTestRunner()->SetDragImage(drag_image);

  // When running a test, we need to fake a drag drop operation otherwise
  // Windows waits for real mouse events to know when the drag is over.
  event_sender_->DoDragDrop(data, mask);
}

FrameWidgetTestHelper*
WebTestWebFrameWidgetImpl::GetFrameWidgetTestHelperForTesting() {
  return this;
}

void WebTestWebFrameWidgetImpl::Reset() {
  event_sender_->Reset();
  // Ends any synthetic gestures started in |event_sender_|.
  FlushInputProcessedCallback();

  // Reset state in the  base class.
  ClearEditCommands();

  SetDeviceScaleFactorForTesting(0);
  ReleaseMouseLockAndPointerCaptureForTesting();

  // These things are only modified/valid for the main frame's widget.
  if (ForMainFrame()) {
    ResetZoomLevelForTesting();

    SetMainFrameOverlayColor(SK_ColorTRANSPARENT);
    SetTextZoomFactor(1);
  }
}

content::EventSender* WebTestWebFrameWidgetImpl::GetEventSender() {
  return event_sender_.get();
}

void WebTestWebFrameWidgetImpl::SynchronouslyCompositeAfterTest() {
  // We could DCHECK(!GetTestRunner()->TestIsRunning()) except that frames in
  // other processes than the main frame do not hear when the test ends.

  // This would be very weird and prevent us from producing pixels.
  DCHECK(!in_synchronous_composite_);

  SynchronouslyComposite(/*do_raster=*/true);
}

content::TestRunner* WebTestWebFrameWidgetImpl::GetTestRunner() {
  return test_runner_;
}

// static
void WebTestWebFrameWidgetImpl::DoComposite(cc::LayerTreeHost* layer_tree_host,
                                            bool do_raster) {
  // Ensure that there is damage so that the compositor submits, and the display
  // compositor draws this frame.
  if (do_raster) {
    layer_tree_host->SetNeedsCommitWithForcedRedraw();
  }

  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), do_raster);
}

void WebTestWebFrameWidgetImpl::SynchronouslyComposite(bool do_raster) {
  if (!LocalRootImpl()->ViewImpl()->does_composite())
    return;
  DCHECK(!LayerTreeHost()->GetSettings().single_thread_proxy_scheduler);

  if (!LayerTreeHost()->IsVisible())
    return;

  if (base::FeatureList::IsEnabled(
          blink::features::kNoForcedFrameUpdatesForWebTests) &&
      LayerTreeHost()->MainFrameUpdatesAreDeferred()) {
    return;
  }

  if (in_synchronous_composite_) {
    // Web tests can use a nested message loop to pump frames while inside a
    // frame, but the compositor does not support this. In this case, we only
    // run blink's lifecycle updates.
    UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
    return;
  }

  in_synchronous_composite_ = true;

  // DoComposite() can detach the frame.
  DoComposite(LayerTreeHost(), do_raster);
  if (!LocalRoot())
    return;

  in_synchronous_composite_ = false;

  // If this widget is for the main frame, we also composite the current
  // PagePopup afterward.
  //
  // TODO(danakj): This means that an OOPIF's popup, which is attached to a
  // WebView without a main frame, would have no opportunity to execute this
  // method call.
  if (ForMainFrame()) {
    WebViewImpl* view = LocalRootImpl()->ViewImpl();
    if (WebPagePopupImpl* popup = view->GetPagePopup()) {
      DoComposite(popup->LayerTreeHostForTesting(), do_raster);
    }
  }
}

void WebTestWebFrameWidgetImpl::AnimateNow() {
  // If we have been Closed but not destroyed yet, return early.
  if (!LocalRootImpl()) {
    return;
  }
  bool do_raster = composite_requested_;
  animation_scheduled_ = false;
  composite_requested_ = false;
  // Composite may destroy |this|, so don't use it afterward.
  SynchronouslyComposite(do_raster);
}

void WebTestWebFrameWidgetImpl::RequestDecode(
    const PaintImage& image,
    base::OnceCallback<void(bool)> callback) {
  WebFrameWidgetImpl::RequestDecode(image, std::move(callback));

  // In web tests the request does not actually cause a commit, because the
  // compositor is scheduled by the test runner to avoid flakiness. So for this
  // case we must request a main frame.
  ScheduleAnimationForWebTests();
}

}  // namespace blink
