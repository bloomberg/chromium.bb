/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/svg/graphics/SVGImage.h"

#include "core/animation/AnimationTimeline.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/style/ComputedStyle.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/paint/FloatClipRecorder.h"
#include "core/paint/TransformRecorder.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGFEImageElement.h"
#include "core/svg/SVGImageElement.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/animation/SMILTimeContainer.h"
#include "core/svg/graphics/SVGImageChromeClient.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/LengthFunctions.h"
#include "platform/TraceEvent.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "wtf/PassRefPtr.h"

namespace blink {

SVGImage::SVGImage(ImageObserver* observer)
    : Image(observer)
{
}

SVGImage::~SVGImage()
{
    if (m_page) {
        // Store m_page in a local variable, clearing m_page, so that SVGImageChromeClient knows we're destructed.
        OwnPtrWillBeRawPtr<Page> currentPage = m_page.release();
        // Break both the loader and view references to the frame
        currentPage->willBeDestroyed();
    }

    // Verify that page teardown destroyed the Chrome
    ASSERT(!m_chromeClient || !m_chromeClient->image());
}

LayoutRect SVGImage::visualRect() const
{
    // TODO(chrishtr): fix this.
    return LayoutRect();
}

bool SVGImage::isInSVGImage(const Node* node)
{
    ASSERT(node);

    Page* page = node->document().page();
    if (!page)
        return false;

    return page->chromeClient().isSVGImageChromeClient();
}

bool SVGImage::currentFrameHasSingleSecurityOrigin() const
{
    if (!m_page)
        return true;

    LocalFrame* frame = toLocalFrame(m_page->mainFrame());

    RELEASE_ASSERT(frame->document()->loadEventFinished());

    SVGSVGElement* rootElement = frame->document()->accessSVGExtensions().rootElement();
    if (!rootElement)
        return true;

    // Don't allow foreignObject elements or images that are not known to be
    // single-origin since these can leak cross-origin information.
    for (Node* node = rootElement; node; node = FlatTreeTraversal::next(*node)) {
        if (isSVGForeignObjectElement(*node))
            return false;
        if (isSVGImageElement(*node)) {
            if (!toSVGImageElement(*node).currentFrameHasSingleSecurityOrigin())
                return false;
        } else if (isSVGFEImageElement(*node)) {
            if (!toSVGFEImageElement(*node).currentFrameHasSingleSecurityOrigin())
                return false;
        }
    }

    // Because SVG image rendering disallows external resources and links, these
    // images effectively are restricted to a single security origin.
    return true;
}

static SVGSVGElement* svgRootElement(Page* page)
{
    if (!page)
        return nullptr;
    LocalFrame* frame = toLocalFrame(page->mainFrame());
    return frame->document()->accessSVGExtensions().rootElement();
}

IntSize SVGImage::containerSize() const
{
    SVGSVGElement* rootElement = svgRootElement(m_page.get());
    if (!rootElement)
        return IntSize();

    LayoutSVGRoot* layoutObject = toLayoutSVGRoot(rootElement->layoutObject());
    if (!layoutObject)
        return IntSize();

    // If a container size is available it has precedence.
    IntSize containerSize = layoutObject->containerSize();
    if (!containerSize.isEmpty())
        return containerSize;

    // Assure that a container size is always given for a non-identity zoom level.
    ASSERT(layoutObject->style()->effectiveZoom() == 1);

    LayoutBox::IntrinsicSizingInfo intrinsicSizingInfo;
    layoutObject->computeIntrinsicSizingInfo(intrinsicSizingInfo);

    if (intrinsicSizingInfo.size.isEmpty() && !intrinsicSizingInfo.aspectRatio.isEmpty()) {
        if (!intrinsicSizingInfo.size.width() && intrinsicSizingInfo.size.height()) {
            intrinsicSizingInfo.size.setWidth(
                intrinsicSizingInfo.size.height() * intrinsicSizingInfo.aspectRatio.width() / intrinsicSizingInfo.aspectRatio.height());
        } else if (intrinsicSizingInfo.size.width() && !intrinsicSizingInfo.size.height()) {
            intrinsicSizingInfo.size.setHeight(
                intrinsicSizingInfo.size.width() * intrinsicSizingInfo.aspectRatio.height() / intrinsicSizingInfo.aspectRatio.width());
        }
    }

    // TODO(davve): In order to maintain aspect ratio the intrinsic
    // size is faked from the viewBox as a last resort. This may cause
    // unwanted side effects. Preferably we should be able to signal
    // the intrinsic ratio in another way.
    if (intrinsicSizingInfo.size.isEmpty())
        intrinsicSizingInfo.size = rootElement->currentViewBoxRect().size();

    if (!intrinsicSizingInfo.size.isEmpty())
        return expandedIntSize(intrinsicSizingInfo.size);

    // As last resort, use CSS replaced element fallback size.
    return IntSize(300, 150);
}

void SVGImage::drawForContainer(SkCanvas* canvas, const SkPaint& paint, const FloatSize containerSize, float zoom, const FloatRect& dstRect,
    const FloatRect& srcRect, const KURL& url)
{
    if (!m_page)
        return;

    // Temporarily disable the image observer to prevent changeInRect() calls due re-laying out the image.
    ImageObserverDisabler imageObserverDisabler(this);

    IntSize roundedContainerSize = roundedIntSize(containerSize);

    if (SVGSVGElement* rootElement = svgRootElement(m_page.get())) {
        if (LayoutSVGRoot* layoutObject = toLayoutSVGRoot(rootElement->layoutObject()))
            layoutObject->setContainerSize(roundedContainerSize);
    }

    FloatRect scaledSrc = srcRect;
    scaledSrc.scale(1 / zoom);

    // Compensate for the container size rounding by adjusting the source rect.
    FloatSize adjustedSrcSize = scaledSrc.size();
    adjustedSrcSize.scale(roundedContainerSize.width() / containerSize.width(), roundedContainerSize.height() / containerSize.height());
    scaledSrc.setSize(adjustedSrcSize);

    drawInternal(canvas, paint, dstRect, scaledSrc, DoNotRespectImageOrientation, ClampImageToSourceRect, url);
}

PassRefPtr<SkImage> SVGImage::imageForCurrentFrame()
{
    return imageForCurrentFrameForContainer(KURL());
}

void SVGImage::drawPatternForContainer(GraphicsContext& context, const FloatSize containerSize,
    float zoom, const FloatRect& srcRect, const FloatSize& tileScale, const FloatPoint& phase,
    SkXfermode::Mode compositeOp, const FloatRect& dstRect,
    const FloatSize& repeatSpacing, const KURL& url)
{
    // Tile adjusted for scaling/stretch.
    FloatRect tile(srcRect);
    tile.scale(tileScale.width(), tileScale.height());

    // Expand the tile to account for repeat spacing.
    FloatRect spacedTile(tile);
    spacedTile.expand(FloatSize(repeatSpacing));

    SkPictureBuilder patternPicture(spacedTile, nullptr, &context);
    if (!DrawingRecorder::useCachedDrawingIfPossible(patternPicture.context(), *this, DisplayItem::Type::SVGImage)) {
        DrawingRecorder patternPictureRecorder(patternPicture.context(), *this, DisplayItem::Type::SVGImage, spacedTile);
        // When generating an expanded tile, make sure we don't draw into the spacing area.
        if (tile != spacedTile)
            patternPicture.context().clip(tile);
        SkPaint paint;
        drawForContainer(patternPicture.context().canvas(), paint, containerSize, zoom, tile, srcRect, url);
    }
    RefPtr<const SkPicture> tilePicture = patternPicture.endRecording();

    SkMatrix patternTransform;
    patternTransform.setTranslate(phase.x() + spacedTile.x(), phase.y() + spacedTile.y());
    RefPtr<SkShader> patternShader = adoptRef(SkShader::CreatePictureShader(
        tilePicture.get(), SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode,
        &patternTransform, nullptr));

    SkPaint paint;
    paint.setShader(patternShader.get());
    paint.setXfermodeMode(compositeOp);
    paint.setColorFilter(context.colorFilter());
    context.drawRect(dstRect, paint);
}

PassRefPtr<SkImage> SVGImage::imageForCurrentFrameForContainer(const KURL& url)
{
    if (!m_page)
        return nullptr;

    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(width(), height());
    drawForContainer(canvas, SkPaint(), FloatSize(size()), 1, rect(), rect(), url);
    RefPtr<SkPicture> picture = adoptRef(recorder.endRecording());

    return adoptRef(
        SkImage::NewFromPicture(picture.get(), SkISize::Make(width(), height()), nullptr, nullptr));
}

static bool drawNeedsLayer(const SkPaint& paint)
{
    if (SkColorGetA(paint.getColor()) < 255)
        return true;

    SkXfermode::Mode xfermode;
    if (SkXfermode::AsMode(paint.getXfermode(), &xfermode)) {
        if (xfermode != SkXfermode::kSrcOver_Mode)
            return true;
    }

    return false;
}

void SVGImage::draw(SkCanvas* canvas, const SkPaint& paint, const FloatRect& dstRect, const FloatRect& srcRect,
    RespectImageOrientationEnum shouldRespectImageOrientation, ImageClampingMode clampMode)
{
    if (!m_page)
        return;

    drawInternal(canvas, paint, dstRect, srcRect, shouldRespectImageOrientation, clampMode, KURL());
}

void SVGImage::drawInternal(SkCanvas* canvas, const SkPaint& paint, const FloatRect& dstRect, const FloatRect& srcRect,
    RespectImageOrientationEnum, ImageClampingMode, const KURL& url)
{
    FrameView* view = frameView();
    view->resize(containerSize());

    // Always call processUrlFragment, even if the url is empty, because
    // there may have been a previous url/fragment that needs to be reset.
    view->processUrlFragment(url);

    SkPictureBuilder imagePicture(dstRect);
    {
        ClipRecorder clipRecorder(imagePicture.context(), *this, DisplayItem::ClipNodeImage, LayoutRect(enclosingIntRect(dstRect)));

        // We can only draw the entire frame, clipped to the rect we want. So compute where the top left
        // of the image would be if we were drawing without clipping, and translate accordingly.
        FloatSize scale(dstRect.width() / srcRect.width(), dstRect.height() / srcRect.height());
        FloatSize topLeftOffset(srcRect.location().x() * scale.width(), srcRect.location().y() * scale.height());
        FloatPoint destOffset = dstRect.location() - topLeftOffset;
        AffineTransform transform = AffineTransform::translation(destOffset.x(), destOffset.y());
        transform.scale(scale.width(), scale.height());
        TransformRecorder transformRecorder(imagePicture.context(), *this, transform);

        view->updateAllLifecyclePhases();
        view->paint(imagePicture.context(), CullRect(enclosingIntRect(srcRect)));
        ASSERT(!view->needsLayout());
    }

    {
        SkAutoCanvasRestore ar(canvas, false);
        if (drawNeedsLayer(paint)) {
            SkRect layerRect = dstRect;
            canvas->saveLayer(&layerRect, &paint);
        }
        RefPtr<const SkPicture> recording = imagePicture.endRecording();
        canvas->drawPicture(recording.get());
    }

    if (imageObserver())
        imageObserver()->didDraw(this);

    // Start any (SMIL) animations if needed. This will restart or continue
    // animations if preceded by calls to resetAnimation or stopAnimation
    // respectively.
    startAnimation();
}

LayoutBox* SVGImage::embeddedContentBox() const
{
    SVGSVGElement* rootElement = svgRootElement(m_page.get());
    if (!rootElement)
        return nullptr;
    return toLayoutBox(rootElement->layoutObject());
}

FrameView* SVGImage::frameView() const
{
    if (!m_page)
        return nullptr;

    return toLocalFrame(m_page->mainFrame())->view();
}

void SVGImage::computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    SVGSVGElement* rootElement = svgRootElement(m_page.get());
    if (!rootElement)
        return;

    intrinsicWidth = rootElement->intrinsicWidth();
    intrinsicHeight = rootElement->intrinsicHeight();
    if (rootElement->preserveAspectRatio()->currentValue()->align() == SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE)
        return;

    intrinsicRatio = rootElement->viewBox()->currentValue()->value().size();
    if (intrinsicRatio.isEmpty() && intrinsicWidth.isFixed() && intrinsicHeight.isFixed())
        intrinsicRatio = FloatSize(floatValueForLength(intrinsicWidth, 0), floatValueForLength(intrinsicHeight, 0));
}

// FIXME: support CatchUpAnimation = CatchUp.
void SVGImage::startAnimation(CatchUpAnimation)
{
    SVGSVGElement* rootElement = svgRootElement(m_page.get());
    if (!rootElement || !rootElement->animationsPaused())
        return;
    rootElement->unpauseAnimations();
}

void SVGImage::stopAnimation()
{
    SVGSVGElement* rootElement = svgRootElement(m_page.get());
    if (!rootElement)
        return;
    rootElement->pauseAnimations();
}

void SVGImage::resetAnimation()
{
    SVGSVGElement* rootElement = svgRootElement(m_page.get());
    if (!rootElement)
        return;
    rootElement->pauseAnimations();
    rootElement->setCurrentTime(0);
}

bool SVGImage::hasAnimations() const
{
    SVGSVGElement* rootElement = svgRootElement(m_page.get());
    if (!rootElement)
        return false;
    return rootElement->timeContainer()->hasAnimations() || toLocalFrame(m_page->mainFrame())->document()->timeline().hasPendingUpdates();
}

void SVGImage::advanceAnimationForTesting()
{
    if (SVGSVGElement* rootElement = svgRootElement(m_page.get())) {
        rootElement->timeContainer()->advanceFrameForTesting();

        // The following triggers animation updates which can issue a new draw
        // but will not permanently change the animation timeline.
        // TODO(pdr): Actually advance the document timeline so CSS animations
        // can be properly tested.
        rootElement->document().page()->animator().serviceScriptedAnimations(rootElement->getCurrentTime());
        imageObserver()->animationAdvanced(this);
    }
}

void SVGImage::updateUseCounters(Document& document) const
{
    if (SVGSVGElement* rootElement = svgRootElement(m_page.get())) {
        if (rootElement->timeContainer()->hasAnimations())
            UseCounter::countDeprecation(document, UseCounter::SVGSMILAnimationInImageRegardlessOfCache);
    }
}

bool SVGImage::dataChanged(bool allDataReceived)
{
    TRACE_EVENT0("blink", "SVGImage::dataChanged");

    // Don't do anything if is an empty image.
    if (!data()->size())
        return true;

    if (allDataReceived) {
        // SVGImage will fire events (and the default C++ handlers run) but doesn't
        // actually allow script to run so it's fine to call into it. We allow this
        // since it means an SVG data url can synchronously load like other image
        // types.
        EventDispatchForbiddenScope::AllowUserAgentEvents allowUserAgentEvents;

        DEFINE_STATIC_LOCAL(OwnPtrWillBePersistent<FrameLoaderClient>, dummyFrameLoaderClient, (EmptyFrameLoaderClient::create()));

        if (m_page) {
            toLocalFrame(m_page->mainFrame())->loader().load(FrameLoadRequest(0, blankURL(), SubstituteData(data(), AtomicString("image/svg+xml", AtomicString::ConstructFromLiteral),
                AtomicString("UTF-8", AtomicString::ConstructFromLiteral), KURL(), ForceSynchronousLoad)));
            return true;
        }

        Page::PageClients pageClients;
        fillWithEmptyClients(pageClients);
        m_chromeClient = SVGImageChromeClient::create(this);
        pageClients.chromeClient = m_chromeClient.get();

        // FIXME: If this SVG ends up loading itself, we might leak the world.
        // The Cache code does not know about ImageResources holding Frames and
        // won't know to break the cycle.
        // This will become an issue when SVGImage will be able to load other
        // SVGImage objects, but we're safe now, because SVGImage can only be
        // loaded by a top-level document.
        OwnPtrWillBeRawPtr<Page> page;
        {
            TRACE_EVENT0("blink", "SVGImage::dataChanged::createPage");
            page = Page::create(pageClients);
            page->settings().setScriptEnabled(false);
            page->settings().setPluginsEnabled(false);
            page->settings().setAcceleratedCompositingEnabled(false);

            // Because this page is detached, it can't get default font settings
            // from the embedder. Copy over font settings so we have sensible
            // defaults. These settings are fixed and will not update if changed.
            if (!Page::ordinaryPages().isEmpty()) {
                Settings& defaultSettings = (*Page::ordinaryPages().begin())->settings();
                page->settings().genericFontFamilySettings() = defaultSettings.genericFontFamilySettings();
                page->settings().setMinimumFontSize(defaultSettings.minimumFontSize());
                page->settings().setMinimumLogicalFontSize(defaultSettings.minimumLogicalFontSize());
                page->settings().setDefaultFontSize(defaultSettings.defaultFontSize());
                page->settings().setDefaultFixedFontSize(defaultSettings.defaultFixedFontSize());
            }
        }

        RefPtrWillBeRawPtr<LocalFrame> frame = nullptr;
        {
            TRACE_EVENT0("blink", "SVGImage::dataChanged::createFrame");
            frame = LocalFrame::create(dummyFrameLoaderClient.get(), &page->frameHost(), 0);
            frame->setView(FrameView::create(frame.get()));
            frame->init();
        }

        FrameLoader& loader = frame->loader();
        loader.forceSandboxFlags(SandboxAll);

        frame->view()->setScrollbarsSuppressed(true);
        frame->view()->setCanHaveScrollbars(false); // SVG Images will always synthesize a viewBox, if it's not available, and thus never see scrollbars.
        frame->view()->setTransparent(true); // SVG Images are transparent.

        m_page = page.release();

        TRACE_EVENT0("blink", "SVGImage::dataChanged::load");
        loader.load(FrameLoadRequest(0, blankURL(), SubstituteData(data(), AtomicString("image/svg+xml", AtomicString::ConstructFromLiteral),
            AtomicString("UTF-8", AtomicString::ConstructFromLiteral), KURL(), ForceSynchronousLoad)));

        // Set the intrinsic size before a container size is available.
        m_intrinsicSize = containerSize();
    }

    return m_page;
}

String SVGImage::filenameExtension() const
{
    return "svg";
}

} // namespace blink
