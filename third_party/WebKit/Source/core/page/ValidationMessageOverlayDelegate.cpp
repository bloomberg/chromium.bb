// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/ValidationMessageOverlayDelegate.h"

#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/loader/EmptyClients.h"
#include "core/page/Page.h"
#include "platform/graphics/paint/CullRect.h"

namespace blink {

// ChromeClient for an internal page of ValidationMessageOverlayDelegate.
class ValidationMessageChromeClient : public EmptyChromeClient {
 public:
  explicit ValidationMessageChromeClient(ChromeClient& main_chrome_client,
                                         PageOverlay& overlay)
      : main_chrome_client_(main_chrome_client), overlay_(overlay) {}

  DEFINE_INLINE_TRACE() {
    visitor->Trace(main_chrome_client_);
    EmptyChromeClient::Trace(visitor);
  }

  void InvalidateRect(const IntRect&) override { overlay_.Update(); }

  void ScheduleAnimation(const PlatformFrameView* frame_view) override {
    main_chrome_client_->ScheduleAnimation(frame_view);
  }

 private:
  Member<ChromeClient> main_chrome_client_;
  PageOverlay& overlay_;
};

inline ValidationMessageOverlayDelegate::ValidationMessageOverlayDelegate(
    Page& page,
    const Element& anchor,
    const String& message,
    TextDirection message_dir,
    const String& sub_message,
    TextDirection sub_message_dir)
    : main_page_(page) {}

std::unique_ptr<ValidationMessageOverlayDelegate>
ValidationMessageOverlayDelegate::Create(Page& page,
                                         const Element& anchor,
                                         const String& message,
                                         TextDirection message_dir,
                                         const String& sub_message,
                                         TextDirection sub_message_dir) {
  return WTF::WrapUnique(new ValidationMessageOverlayDelegate(
      page, anchor, message, message_dir, sub_message, sub_message_dir));
}

ValidationMessageOverlayDelegate::~ValidationMessageOverlayDelegate() {
  if (page_)
    page_->WillBeDestroyed();
}

LocalFrameView& ValidationMessageOverlayDelegate::FrameView() const {
  DCHECK(page_)
      << "Do not call FrameView() before the first call of EnsurePage()";
  return *ToLocalFrame(page_->MainFrame())->View();
}

void ValidationMessageOverlayDelegate::PaintPageOverlay(
    const PageOverlay& overlay,
    GraphicsContext& context,
    const WebSize& view_size) const {
  const_cast<ValidationMessageOverlayDelegate*>(this)->UpdateFrameViewState(
      overlay, view_size);
  LocalFrameView& view = FrameView();
  view.Paint(context, CullRect(IntRect(0, 0, view.Width(), view.Height())));
}

void ValidationMessageOverlayDelegate::UpdateFrameViewState(
    const PageOverlay& overlay,
    const IntSize& view_size) {
  EnsurePage(overlay, view_size);
  if (FrameView().Size() != view_size) {
    FrameView().Resize(view_size);
    page_->GetVisualViewport().SetSize(view_size);
  }
  FrameView().UpdateAllLifecyclePhases();
}

void ValidationMessageOverlayDelegate::EnsurePage(const PageOverlay& overlay,
                                                  const IntSize& view_size) {
  if (page_)
    return;
  // TODO(tkent): Can we share code with WebPagePopupImpl and
  // InspectorOverlayAgent?
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  chrome_client_ = new ValidationMessageChromeClient(
      main_page_->GetChromeClient(), const_cast<PageOverlay&>(overlay));
  page_clients.chrome_client = chrome_client_;
  Settings& main_settings = main_page_->GetSettings();
  page_ = Page::Create(page_clients);
  page_->GetSettings().SetMinimumFontSize(main_settings.GetMinimumFontSize());
  page_->GetSettings().SetMinimumLogicalFontSize(
      main_settings.GetMinimumLogicalFontSize());
  page_->GetSettings().SetAcceleratedCompositingEnabled(false);

  LocalFrame* frame =
      LocalFrame::Create(EmptyLocalFrameClient::Create(), *page_, nullptr);
  frame->SetView(LocalFrameView::Create(*frame, view_size));
  frame->Init();
  frame->View()->SetCanHaveScrollbars(false);
  frame->View()->SetBaseBackgroundColor(Color::kTransparent);
  page_->GetVisualViewport().SetSize(view_size);

  RefPtr<SharedBuffer> data = SharedBuffer::Create();
  // TODO(tkent): Pour HTML into |data|.
  frame->Loader().Load(
      FrameLoadRequest(nullptr, ResourceRequest(BlankURL()),
                       SubstituteData(data, "text/html", "UTF-8", KURL(),
                                      kForceSynchronousLoad)));
}

}  // namespace blink
