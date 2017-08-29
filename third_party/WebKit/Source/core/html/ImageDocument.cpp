/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/ImageDocument.h"

#include <limits>
#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/RawDataDocumentParser.h"
#include "core/dom/events/EventListener.h"
#include "core/events/MouseEvent.h"
#include "core/frame/ContentSettingsClient.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/layout/LayoutObject.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/resource/ImageResource.h"
#include "core/page/Page.h"
#include "platform/PlatformChromeClient.h"
#include "platform/wtf/text/StringBuilder.h"

namespace {

// The base square size is set to 10 because it rounds nicely for both the
// minimum scale (0.1) and maximum scale (5.0).
const int kBaseCheckerSize = 10;

}  // namespace

namespace blink {

using namespace HTMLNames;

class ImageEventListener : public EventListener {
 public:
  static ImageEventListener* Create(ImageDocument* document) {
    return new ImageEventListener(document);
  }
  static const ImageEventListener* Cast(const EventListener* listener) {
    return listener->GetType() == kImageEventListenerType
               ? static_cast<const ImageEventListener*>(listener)
               : 0;
  }

  bool operator==(const EventListener& other) const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(doc_);
    EventListener::Trace(visitor);
  }

 private:
  ImageEventListener(ImageDocument* document)
      : EventListener(kImageEventListenerType), doc_(document) {}

  virtual void handleEvent(ExecutionContext*, Event*);

  Member<ImageDocument> doc_;
};

class ImageDocumentParser : public RawDataDocumentParser {
 public:
  static ImageDocumentParser* Create(ImageDocument* document) {
    return new ImageDocumentParser(document);
  }

  ImageDocument* GetDocument() const {
    return ToImageDocument(RawDataDocumentParser::GetDocument());
  }

 private:
  ImageDocumentParser(ImageDocument* document)
      : RawDataDocumentParser(document) {}

  void AppendBytes(const char*, size_t) override;
  virtual void Finish();
};

// --------

static float PageZoomFactor(const Document* document) {
  LocalFrame* frame = document->GetFrame();
  return frame ? frame->PageZoomFactor() : 1;
}

static String ImageTitle(const String& filename, const IntSize& size) {
  StringBuilder result;
  result.Append(filename);
  result.Append(" (");
  // FIXME: Localize numbers. Safari/OSX shows localized numbers with group
  // separaters. For example, "1,920x1,080".
  result.AppendNumber(size.Width());
  result.Append(static_cast<UChar>(0xD7));  // U+00D7 (multiplication sign)
  result.AppendNumber(size.Height());
  result.Append(')');
  return result.ToString();
}

static LayoutSize CachedImageSize(HTMLImageElement* element) {
  DCHECK(element->CachedImage());
  return element->CachedImage()->ImageSize(
      LayoutObject::ShouldRespectImageOrientation(element->GetLayoutObject()),
      1.0f);
}

void ImageDocumentParser::AppendBytes(const char* data, size_t length) {
  if (!length)
    return;

  LocalFrame* frame = GetDocument()->GetFrame();
  Settings* settings = frame->GetSettings();
  if (!frame->GetContentSettingsClient()->AllowImage(
          !settings || settings->GetImagesEnabled(), GetDocument()->Url()))
    return;

  if (GetDocument()->CachedImageResourceDeprecated()) {
    CHECK_LE(length, std::numeric_limits<unsigned>::max());
    // If decoding has already failed, there's no point in sending additional
    // data to the ImageResource.
    if (GetDocument()->CachedImageResourceDeprecated()->GetStatus() !=
        ResourceStatus::kDecodeError)
      GetDocument()->CachedImageResourceDeprecated()->AppendData(data, length);
  }

  if (!IsDetached())
    GetDocument()->ImageUpdated();
}

void ImageDocumentParser::Finish() {
  if (!IsStopped() && GetDocument()->ImageElement() &&
      GetDocument()->CachedImageResourceDeprecated()) {
    // TODO(hiroshige): Use ImageResourceContent instead of ImageResource.
    ImageResource* cached_image =
        GetDocument()->CachedImageResourceDeprecated();
    DocumentLoader* loader = GetDocument()->Loader();
    cached_image->SetResponse(loader->GetResponse());
    cached_image->Finish(loader->GetTiming().ResponseEnd());

    // Report the natural image size in the page title, regardless of zoom
    // level.  At a zoom level of 1 the image is guaranteed to have an integer
    // size.
    IntSize size =
        FlooredIntSize(CachedImageSize(GetDocument()->ImageElement()));
    if (size.Width()) {
      // Compute the title, we use the decoded filename of the resource, falling
      // back on the (decoded) hostname if there is no path.
      String file_name =
          DecodeURLEscapeSequences(GetDocument()->Url().LastPathComponent());
      if (file_name.IsEmpty())
        file_name = GetDocument()->Url().Host();
      GetDocument()->setTitle(ImageTitle(file_name, size));
      if (IsDetached())
        return;
    }

    GetDocument()->ImageUpdated();
    GetDocument()->ImageLoaded();
  }

  if (!IsDetached())
    GetDocument()->FinishedParsing();
}

// --------

ImageDocument::ImageDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer, kImageDocumentClass),
      div_element_(nullptr),
      image_element_(nullptr),
      image_size_is_known_(false),
      did_shrink_image_(false),
      should_shrink_image_(ShouldShrinkToFit()),
      image_is_loaded_(false),
      style_checker_size_(0),
      style_mouse_cursor_mode_(kDefault),
      shrink_to_fit_mode_(GetFrame()->GetSettings()->GetViewportEnabled()
                              ? kViewport
                              : kDesktop) {
  SetCompatibilityMode(kQuirksMode);
  LockCompatibilityMode();
  UseCounter::Count(*this, WebFeature::kImageDocument);
  if (!IsInMainFrame())
    UseCounter::Count(*this, WebFeature::kImageDocumentInFrame);
}

DocumentParser* ImageDocument::CreateParser() {
  return ImageDocumentParser::Create(this);
}

void ImageDocument::CreateDocumentStructure() {
  HTMLHtmlElement* root_element = HTMLHtmlElement::Create(*this);
  AppendChild(root_element);
  root_element->InsertedByParser();

  if (IsStopped())
    return;  // runScriptsAtDocumentElementAvailable can detach the frame.

  HTMLHeadElement* head = HTMLHeadElement::Create(*this);
  HTMLMetaElement* meta = HTMLMetaElement::Create(*this);
  meta->setAttribute(nameAttr, "viewport");
  meta->setAttribute(contentAttr, "width=device-width, minimum-scale=0.1");
  head->AppendChild(meta);

  HTMLBodyElement* body = HTMLBodyElement::Create(*this);

  if (ShouldShrinkToFit()) {
    // Display the image prominently centered in the frame.
    body->setAttribute(styleAttr, "margin: 0px; background: #0e0e0e;");

    // See w3c example on how to center an element:
    // https://www.w3.org/Style/Examples/007/center.en.html
    div_element_ = HTMLDivElement::Create(*this);
    div_element_->setAttribute(styleAttr,
                               "display: flex;"
                               "flex-direction: column;"
                               "justify-content: center;"
                               "align-items: center;"
                               "min-height: min-content;"
                               "min-width: min-content;"
                               "height: 100%;"
                               "width: 100%;");
    HTMLContentElement* content = HTMLContentElement::Create(*this);
    div_element_->AppendChild(content);

    ShadowRoot& shadow_root = body->EnsureUserAgentShadowRoot();
    shadow_root.AppendChild(div_element_);
  } else {
    body->setAttribute(styleAttr, "margin: 0px;");
  }

  WillInsertBody();

  image_element_ = HTMLImageElement::Create(*this);
  UpdateImageStyle();
  image_element_->SetLoadingImageDocument();
  image_element_->SetSrc(Url().GetString());
  body->AppendChild(image_element_.Get());
  if (Loader() && image_element_->CachedImageResourceForImageDocument()) {
    image_element_->CachedImageResourceForImageDocument()->ResponseReceived(
        Loader()->GetResponse(), nullptr);
  }

  if (ShouldShrinkToFit()) {
    // Add event listeners
    EventListener* listener = ImageEventListener::Create(this);
    if (LocalDOMWindow* dom_window = this->domWindow())
      dom_window->addEventListener(EventTypeNames::resize, listener, false);

    if (shrink_to_fit_mode_ == kDesktop) {
      image_element_->addEventListener(EventTypeNames::click, listener, false);
    } else if (shrink_to_fit_mode_ == kViewport) {
      image_element_->addEventListener(EventTypeNames::touchend, listener,
                                       false);
      image_element_->addEventListener(EventTypeNames::touchcancel, listener,
                                       false);
    }
  }

  root_element->AppendChild(head);
  root_element->AppendChild(body);
}

float ImageDocument::Scale() const {
  DCHECK_EQ(shrink_to_fit_mode_, kDesktop);
  if (!image_element_ || image_element_->GetDocument() != this)
    return 1.0f;

  LocalFrameView* view = GetFrame()->View();
  if (!view)
    return 1.0f;

  DCHECK(image_element_->CachedImage());
  const float zoom = PageZoomFactor(this);
  LayoutSize image_size = image_element_->CachedImage()->ImageSize(
      LayoutObject::ShouldRespectImageOrientation(
          image_element_->GetLayoutObject()),
      zoom);

  // We want to pretend the viewport is larger when the user has zoomed the
  // page in (but not when the zoom is coming from device scale).
  const float manual_zoom =
      zoom / view->GetChromeClient()->WindowToViewportScalar(1.f);
  float width_scale =
      view->Width() * manual_zoom / image_size.Width().ToFloat();
  float height_scale =
      view->Height() * manual_zoom / image_size.Height().ToFloat();

  return std::min(width_scale, height_scale);
}

void ImageDocument::ResizeImageToFit() {
  DCHECK_EQ(shrink_to_fit_mode_, kDesktop);
  if (!image_element_ || image_element_->GetDocument() != this)
    return;

  LayoutSize image_size = CachedImageSize(image_element_);

  const float scale = this->Scale();
  image_element_->setWidth(static_cast<int>(image_size.Width() * scale));
  image_element_->setHeight(static_cast<int>(image_size.Height() * scale));

  UpdateImageStyle();
}

void ImageDocument::ImageClicked(int x, int y) {
  DCHECK_EQ(shrink_to_fit_mode_, kDesktop);

  if (!image_size_is_known_ || ImageFitsInWindow())
    return;

  should_shrink_image_ = !should_shrink_image_;

  if (should_shrink_image_) {
    WindowSizeChanged();
  } else {
    // Adjust the coordinates to account for the fact that the image was
    // centered on the screen.
    float image_x = x - image_element_->OffsetLeft();
    float image_y = y - image_element_->OffsetTop();

    RestoreImageSize();

    UpdateStyleAndLayout();

    double scale = this->Scale();

    float scroll_x =
        image_x / scale - static_cast<float>(GetFrame()->View()->Width()) / 2;
    float scroll_y =
        image_y / scale - static_cast<float>(GetFrame()->View()->Height()) / 2;

    GetFrame()->View()->LayoutViewportScrollableArea()->SetScrollOffset(
        ScrollOffset(scroll_x, scroll_y), kProgrammaticScroll);
  }
}

void ImageDocument::ImageLoaded() {
  image_is_loaded_ = true;

  if (ShouldShrinkToFit()) {
    // The checkerboard background needs to be inserted.
    UpdateImageStyle();
  }
}

void ImageDocument::UpdateImageStyle() {
  StringBuilder image_style;
  image_style.Append("-webkit-user-select: none;");

  if (ShouldShrinkToFit()) {
    if (shrink_to_fit_mode_ == kViewport)
      image_style.Append("max-width: 100%;");

    // Once the image has fully loaded, it is displayed atop a checkerboard to
    // show transparency more faithfully.  The pattern is generated via CSS.
    if (image_is_loaded_) {
      int new_checker_size = kBaseCheckerSize;
      MouseCursorMode new_cursor_mode = kDefault;

      if (shrink_to_fit_mode_ == kViewport) {
        double scale;

        if (HasFinishedParsing()) {
          // To ensure the checker pattern is visible for large images, the
          // checker size is dynamically adjusted to account for how much the
          // page is currently being scaled.
          scale = GetFrame()->GetPage()->GetVisualViewport().Scale();
        } else {
          // The checker pattern is initialized based on how large the image is
          // relative to the viewport.
          int viewport_width =
              GetFrame()->GetPage()->GetVisualViewport().Size().Width();
          scale = viewport_width / static_cast<double>(CalculateDivWidth());
        }

        new_checker_size = std::round(std::max(1.0, new_checker_size / scale));
      } else {
        // In desktop mode, the user can click on the image to zoom in or out.
        DCHECK_EQ(shrink_to_fit_mode_, kDesktop);
        if (ImageFitsInWindow()) {
          new_cursor_mode = kDefault;
        } else {
          new_cursor_mode = should_shrink_image_ ? kZoomIn : kZoomOut;
        }
      }

      // The only things that can differ between updates are checker size and
      // the type of cursor being displayed.
      if (new_checker_size == style_checker_size_ &&
          new_cursor_mode == style_mouse_cursor_mode_) {
        return;
      }
      style_checker_size_ = new_checker_size;
      style_mouse_cursor_mode_ = new_cursor_mode;

      image_style.Append("background-position: 0px 0px, ");
      image_style.Append(AtomicString::Number(style_checker_size_));
      image_style.Append("px ");
      image_style.Append(AtomicString::Number(style_checker_size_));
      image_style.Append("px;");

      int tile_size = style_checker_size_ * 2;
      image_style.Append("background-size: ");
      image_style.Append(AtomicString::Number(tile_size));
      image_style.Append("px ");
      image_style.Append(AtomicString::Number(tile_size));
      image_style.Append("px;");

      // Generating the checkerboard pattern this way is not exactly cheap.
      // If rasterization performance becomes an issue, we could look at using
      // a cheaper shader (e.g. pre-generate a scaled tile + base64-encode +
      // inline dataURI => single bitmap shader).
      image_style.Append(
          "background-image:"
          "linear-gradient(45deg, #eee 25%, transparent 25%, transparent 75%, "
          "#eee 75%, #eee 100%),"
          "linear-gradient(45deg, #eee 25%, white 25%, white 75%, "
          "#eee 75%, #eee 100%);");

      if (shrink_to_fit_mode_ == kDesktop) {
        if (style_mouse_cursor_mode_ == kZoomIn)
          image_style.Append("cursor: zoom-in;");
        else if (style_mouse_cursor_mode_ == kZoomOut)
          image_style.Append("cursor: zoom-out;");
      }
    }
  }

  image_element_->setAttribute(styleAttr, image_style.ToAtomicString());
}

void ImageDocument::ImageUpdated() {
  DCHECK(image_element_);

  if (image_size_is_known_)
    return;

  UpdateStyleAndLayoutTree();
  if (!image_element_->CachedImage() ||
      image_element_->CachedImage()
          ->ImageSize(LayoutObject::ShouldRespectImageOrientation(
                          image_element_->GetLayoutObject()),
                      PageZoomFactor(this))
          .IsEmpty())
    return;

  image_size_is_known_ = true;

  if (ShouldShrinkToFit()) {
    // Force resizing of the image
    WindowSizeChanged();
  }
}

void ImageDocument::RestoreImageSize() {
  DCHECK_EQ(shrink_to_fit_mode_, kDesktop);

  if (!image_element_ || !image_size_is_known_ ||
      image_element_->GetDocument() != this)
    return;

  DCHECK(image_element_->CachedImage());
  LayoutSize image_size = CachedImageSize(image_element_);
  image_element_->setWidth(image_size.Width().ToInt());
  image_element_->setHeight(image_size.Height().ToInt());
  UpdateImageStyle();

  did_shrink_image_ = false;
}

bool ImageDocument::ImageFitsInWindow() const {
  DCHECK_EQ(shrink_to_fit_mode_, kDesktop);
  return this->Scale() >= 1;
}

int ImageDocument::CalculateDivWidth() {
  // Zooming in and out of an image being displayed within a viewport is done
  // by changing the page scale factor of the page instead of changing the
  // size of the image.  The size of the image is set so that:
  // * Images wider than the viewport take the full width of the screen.
  // * Images taller than the viewport are initially aligned with the top of
  //   of the frame.
  // * Images smaller in either dimension are centered along that axis.
  LayoutSize image_size = CachedImageSize(image_element_);
  int viewport_width =
      GetFrame()->GetPage()->GetVisualViewport().Size().Width();

  // For huge images, minimum-scale=0.1 is still too big on small screens.
  // Set the <div> width so that the image will shrink to fit the width of the
  // screen when the scale is minimum.
  int max_width = std::min(image_size.Width().ToInt(), viewport_width * 10);
  return std::max(viewport_width, max_width);
}

void ImageDocument::WindowSizeChanged() {
  if (!image_element_ || !image_size_is_known_ ||
      image_element_->GetDocument() != this)
    return;

  if (shrink_to_fit_mode_ == kViewport) {
    LayoutSize image_size = CachedImageSize(image_element_);
    int div_width = CalculateDivWidth();
    div_element_->SetInlineStyleProperty(CSSPropertyWidth, div_width,
                                         CSSPrimitiveValue::UnitType::kPixels);

    // Explicitly set the height of the <div> containing the <img> so that it
    // can display the full image without shrinking it, allowing a full-width
    // reading mode for normal-width-huge-height images.
    float viewport_aspect_ratio =
        GetFrame()->GetPage()->GetVisualViewport().Size().AspectRatio();
    int div_height =
        std::max(image_size.Height().ToInt(),
                 static_cast<int>(div_width / viewport_aspect_ratio));
    div_element_->SetInlineStyleProperty(CSSPropertyHeight, div_height,
                                         CSSPrimitiveValue::UnitType::kPixels);
    return;
  }

  bool fits_in_window = ImageFitsInWindow();

  // If the image has been explicitly zoomed in, restore the cursor if the image
  // fits and set it to a zoom out cursor if the image doesn't fit
  if (!should_shrink_image_) {
    UpdateImageStyle();
    return;
  }

  if (did_shrink_image_) {
    // If the window has been resized so that the image fits, restore the image
    // size otherwise update the restored image size.
    if (fits_in_window)
      RestoreImageSize();
    else
      ResizeImageToFit();
  } else {
    // If the image isn't resized but needs to be, then resize it.
    if (!fits_in_window) {
      ResizeImageToFit();
      did_shrink_image_ = true;
    }
  }
}

ImageResourceContent* ImageDocument::CachedImage() {
  if (!image_element_) {
    CreateDocumentStructure();
    if (IsStopped()) {
      image_element_ = nullptr;
      return nullptr;
    }
  }

  return image_element_->CachedImage();
}

ImageResource* ImageDocument::CachedImageResourceDeprecated() {
  if (!image_element_) {
    CreateDocumentStructure();
    if (IsStopped()) {
      image_element_ = nullptr;
      return nullptr;
    }
  }

  return image_element_->CachedImageResourceForImageDocument();
}

bool ImageDocument::ShouldShrinkToFit() const {
  // WebView automatically resizes to match the contents, causing an infinite
  // loop as the contents then resize to match the window. To prevent this,
  // disallow images from shrinking to fit for WebViews.
  bool is_wrap_content_web_view =
      GetPage() ? GetPage()->GetSettings().GetForceZeroLayoutHeight() : false;
  return GetFrame()->IsMainFrame() && !is_wrap_content_web_view;
}

DEFINE_TRACE(ImageDocument) {
  visitor->Trace(div_element_);
  visitor->Trace(image_element_);
  HTMLDocument::Trace(visitor);
}

// --------

void ImageEventListener::handleEvent(ExecutionContext*, Event* event) {
  if (event->type() == EventTypeNames::resize) {
    doc_->WindowSizeChanged();
  } else if (event->type() == EventTypeNames::click && event->IsMouseEvent()) {
    MouseEvent* mouse_event = ToMouseEvent(event);
    doc_->ImageClicked(mouse_event->x(), mouse_event->y());
  } else if ((event->type() == EventTypeNames::touchend ||
              event->type() == EventTypeNames::touchcancel) &&
             event->IsTouchEvent()) {
    doc_->UpdateImageStyle();
  }
}

bool ImageEventListener::operator==(const EventListener& listener) const {
  if (const ImageEventListener* image_event_listener =
          ImageEventListener::Cast(&listener))
    return doc_ == image_event_listener->doc_;
  return false;
}

}  // namespace blink
