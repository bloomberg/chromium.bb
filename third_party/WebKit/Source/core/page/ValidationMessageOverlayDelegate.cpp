// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/ValidationMessageOverlayDelegate.h"

#include "core/dom/Element.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/layout/LayoutView.h"
#include "core/loader/EmptyClients.h"
#include "core/page/Page.h"
#include "core/page/PagePopupClient.h"
#include "platform/LayoutTestSupport.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/text/PlatformLocale.h"

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

  float WindowToViewportScalar(const float scalar_value) const override {
    return main_chrome_client_->WindowToViewportScalar(scalar_value);
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
    : main_page_(page),
      anchor_(anchor),
      message_(message),
      sub_message_(sub_message),
      message_dir_(message_dir),
      sub_message_dir_(sub_message_dir) {}

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
  AdjustBubblePosition(view_size);

  // This manual invalidation is necessary to avoid a DCHECK failure in
  // FindVisualRectNeedingUpdateScopeBase::CheckVisualRect().
  FrameView().GetLayoutView()->SetMayNeedPaintInvalidationSubtree();

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
  WriteDocument(data.Get());
  float zoom_factor = anchor_->GetDocument().GetFrame()->PageZoomFactor();
  frame->SetPageZoomFactor(zoom_factor);
  // Propagate deprecated DSF for platforms without use-zoom-for-dsf.
  page_->SetDeviceScaleFactorDeprecated(
      main_page_->DeviceScaleFactorDeprecated());
  frame->Loader().Load(
      FrameLoadRequest(nullptr, ResourceRequest(BlankURL()),
                       SubstituteData(data, "text/html", "UTF-8", KURL(),
                                      kForceSynchronousLoad)));

  Element& container = GetElementById("container");
  if (LayoutTestSupport::IsRunningLayoutTest())
    container.SetInlineStyleProperty(CSSPropertyTransition, "none");
  // Get the size to decide position later.
  FrameView().UpdateAllLifecyclePhases();
  bubble_size_ = container.VisibleBoundsInVisualViewport().Size();
  // Add one because the content sometimes exceeds the exact width due to
  // rounding errors.
  bubble_size_.Expand(1, 0);
  container.SetInlineStyleProperty(CSSPropertyMinWidth,
                                   bubble_size_.Width() / zoom_factor,
                                   CSSPrimitiveValue::UnitType::kPixels);
  container.setAttribute(HTMLNames::classAttr, "shown-initially");
  FrameView().UpdateAllLifecyclePhases();
}

void ValidationMessageOverlayDelegate::WriteDocument(SharedBuffer* data) {
  DCHECK(data);
  PagePopupClient::AddString("<!DOCTYPE html><html><head><style>", data);
  data->Append(Platform::Current()->GetDataResource("validation_bubble.css"));
  PagePopupClient::AddString("</style></head>", data);
  PagePopupClient::AddString(
      Locale::DefaultLocale().IsRTL() ? "<body dir=rtl>" : "<body dir=ltr>",
      data);
  PagePopupClient::AddString(
      "<div id=container>"
      "<div id=outer-arrow-top></div>"
      "<div id=inner-arrow-top></div>"
      "<div id=spacer-top></div>"
      "<main id=bubble-body>",
      data);
  data->Append(Platform::Current()->GetDataResource("input_alert.svg"));
  PagePopupClient::AddString(message_dir_ == TextDirection::kLtr
                                 ? "<div dir=ltr id=main-message>"
                                 : "<div dir=rtl id=main-message>",
                             data);
  PagePopupClient::AddHTMLString(message_, data);
  PagePopupClient::AddString(sub_message_dir_ == TextDirection::kLtr
                                 ? "</div><div dir=ltr id=sub-message>"
                                 : "</div><div dir=rtl id=sub-message>",
                             data);
  PagePopupClient::AddHTMLString(sub_message_, data);
  PagePopupClient::AddString(
      "</div></main>"
      "<div id=outer-arrow-bottom></div>"
      "<div id=inner-arrow-bottom></div>"
      "<div id=spacer-bottom></div>"
      "</div></body></html>\n",
      data);
}

Element& ValidationMessageOverlayDelegate::GetElementById(
    const AtomicString& id) const {
  Element* element =
      ToLocalFrame(page_->MainFrame())->GetDocument()->getElementById(id);
  DCHECK(element) << "No element with id=" << id
                  << ". Failed to load the document?";
  return *element;
}

void ValidationMessageOverlayDelegate::AdjustBubblePosition(
    const IntSize& view_size) {
  float zoom_factor = ToLocalFrame(page_->MainFrame())->PageZoomFactor();
  IntRect anchor_rect = anchor_->VisibleBoundsInVisualViewport();
  bool show_bottom_arrow = false;
  double bubble_y = anchor_rect.MaxY();
  if (view_size.Height() - anchor_rect.MaxY() < bubble_size_.Height()) {
    bubble_y = anchor_rect.Y() - bubble_size_.Height();
    show_bottom_arrow = true;
  }
  double bubble_x =
      anchor_rect.X() + anchor_rect.Width() / 2 - bubble_size_.Width() / 2;
  if (bubble_x < 0)
    bubble_x = 0;
  else if (bubble_x + bubble_size_.Width() > view_size.Width())
    bubble_x = view_size.Width() - bubble_size_.Width();

  Element& container = GetElementById("container");
  container.SetInlineStyleProperty(CSSPropertyLeft, bubble_x / zoom_factor,
                                   CSSPrimitiveValue::UnitType::kPixels);
  container.SetInlineStyleProperty(CSSPropertyTop, bubble_y / zoom_factor,
                                   CSSPrimitiveValue::UnitType::kPixels);
  if (show_bottom_arrow) {
    container.setAttribute(HTMLNames::classAttr, "shown-fully bottom-arrow");
    container.SetInlineStyleProperty(CSSPropertyTransformOrigin,
                                     "center bottom");
  } else {
    container.setAttribute(HTMLNames::classAttr, "shown-fully");
    container.SetInlineStyleProperty(CSSPropertyTransformOrigin, "center top");
  }

  // Should match to --arrow-size in validation_bubble.css.
  const int kArrowSize = 8;
  const int kArrowMargin = 10;
  const int kMinArrowAnchorX = kArrowSize + kArrowMargin;
  double max_arrow_anchor_x =
      bubble_size_.Width() - (kArrowSize + kArrowMargin) * zoom_factor;
  double arrow_anchor_x;
  const int kOffsetToAnchorRect = 8;
  double anchor_rect_center = anchor_rect.X() + anchor_rect.Width() / 2;
  if (!Locale::DefaultLocale().IsRTL()) {
    double anchor_rect_left =
        anchor_rect.X() + kOffsetToAnchorRect * zoom_factor;
    if (anchor_rect_left > anchor_rect_center)
      anchor_rect_left = anchor_rect_center;

    arrow_anchor_x = kMinArrowAnchorX * zoom_factor;
    if (bubble_x + arrow_anchor_x < anchor_rect_left) {
      arrow_anchor_x = anchor_rect_left - bubble_x;
      if (arrow_anchor_x > max_arrow_anchor_x)
        arrow_anchor_x = max_arrow_anchor_x;
    }
  } else {
    double anchor_rect_right =
        anchor_rect.MaxX() - kOffsetToAnchorRect * zoom_factor;
    if (anchor_rect_right < anchor_rect_center)
      anchor_rect_right = anchor_rect_center;

    arrow_anchor_x = max_arrow_anchor_x;
    if (bubble_x + arrow_anchor_x > anchor_rect_right) {
      arrow_anchor_x = anchor_rect_right - bubble_x;
      if (arrow_anchor_x < kMinArrowAnchorX * zoom_factor)
        arrow_anchor_x = kMinArrowAnchorX * zoom_factor;
    }
  }
  double arrow_x = arrow_anchor_x / zoom_factor - kArrowSize;
  if (show_bottom_arrow) {
    GetElementById("outer-arrow-bottom")
        .SetInlineStyleProperty(CSSPropertyLeft, arrow_x,
                                CSSPrimitiveValue::UnitType::kPixels);
    GetElementById("inner-arrow-bottom")
        .SetInlineStyleProperty(CSSPropertyLeft, arrow_x,
                                CSSPrimitiveValue::UnitType::kPixels);
  } else {
    GetElementById("outer-arrow-top")
        .SetInlineStyleProperty(CSSPropertyLeft, arrow_x,
                                CSSPrimitiveValue::UnitType::kPixels);
    GetElementById("inner-arrow-top")
        .SetInlineStyleProperty(CSSPropertyLeft, arrow_x,
                                CSSPrimitiveValue::UnitType::kPixels);
  }
}

}  // namespace blink
