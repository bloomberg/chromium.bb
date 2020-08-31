/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/GrClipStackClip.h"

#include "include/private/SkTo.h"
#include "src/core/SkClipOpPriv.h"
#include "src/core/SkTaskGroup.h"
#include "src/core/SkTraceEvent.h"
#include "src/gpu/GrAppliedClip.h"
#include "src/gpu/GrContextPriv.h"
#include "src/gpu/GrDeferredProxyUploader.h"
#include "src/gpu/GrDrawingManager.h"
#include "src/gpu/GrFixedClip.h"
#include "src/gpu/GrGpuResourcePriv.h"
#include "src/gpu/GrProxyProvider.h"
#include "src/gpu/GrRecordingContextPriv.h"
#include "src/gpu/GrRenderTargetContextPriv.h"
#include "src/gpu/GrSWMaskHelper.h"
#include "src/gpu/GrStencilAttachment.h"
#include "src/gpu/GrStyle.h"
#include "src/gpu/GrTextureProxy.h"
#include "src/gpu/effects/GrConvexPolyEffect.h"
#include "src/gpu/effects/GrRRectEffect.h"
#include "src/gpu/effects/generated/GrDeviceSpaceEffect.h"
#include "src/gpu/geometry/GrStyledShape.h"

typedef SkClipStack::Element Element;
typedef GrReducedClip::InitialState InitialState;
typedef GrReducedClip::ElementList ElementList;

const char GrClipStackClip::kMaskTestTag[] = "clip_mask";

bool GrClipStackClip::quickContains(const SkRect& rect) const {
    if (!fStack || fStack->isWideOpen()) {
        return true;
    }
    return fStack->quickContains(rect);
}

bool GrClipStackClip::quickContains(const SkRRect& rrect) const {
    if (!fStack || fStack->isWideOpen()) {
        return true;
    }
    return fStack->quickContains(rrect);
}

bool GrClipStackClip::isRRect(const SkRect& origRTBounds, SkRRect* rr, GrAA* aa) const {
    if (!fStack) {
        return false;
    }
    const SkRect* rtBounds = &origRTBounds;
    bool isAA;
    if (fStack->isRRect(*rtBounds, rr, &isAA)) {
        *aa = GrAA(isAA);
        return true;
    }
    return false;
}

SkIRect GrClipStackClip::getConservativeBounds(int width, int height) const {
    if (fStack) {
        SkRect devBounds;
        fStack->getConservativeBounds(0, 0, width, height, &devBounds);
        return devBounds.roundOut();
    } else {
        return this->GrClip::getConservativeBounds(width, height);
    }
}

////////////////////////////////////////////////////////////////////////////////
// set up the draw state to enable the aa clipping mask.
static std::unique_ptr<GrFragmentProcessor> create_fp_for_mask(GrSurfaceProxyView mask,
                                                               const SkIRect& devBound,
                                                               const GrCaps& caps) {
    GrSamplerState samplerState(GrSamplerState::WrapMode::kClampToBorder,
                                GrSamplerState::Filter::kNearest);
    auto m = SkMatrix::MakeTrans(-devBound.fLeft, -devBound.fTop);
    auto subset = SkRect::Make(devBound.size());
    // We scissor to devBounds. The mask's texel centers are aligned to device space
    // pixel centers. Hence this domain of texture coordinates.
    auto domain = subset.makeInset(0.5, 0.5);
    auto fp = GrTextureEffect::MakeSubset(std::move(mask), kPremul_SkAlphaType, m, samplerState,
                                          subset, domain, caps);
    return GrDeviceSpaceEffect::Make(std::move(fp));
}

// Does the path in 'element' require SW rendering? If so, return true (and,
// optionally, set 'prOut' to NULL. If not, return false (and, optionally, set
// 'prOut' to the non-SW path renderer that will do the job).
bool GrClipStackClip::PathNeedsSWRenderer(GrRecordingContext* context,
                                          const SkIRect& scissorRect,
                                          bool hasUserStencilSettings,
                                          const GrRenderTargetContext* renderTargetContext,
                                          const SkMatrix& viewMatrix,
                                          const Element* element,
                                          GrPathRenderer** prOut,
                                          bool needsStencil) {
    if (Element::DeviceSpaceType::kRect == element->getDeviceSpaceType()) {
        // rects can always be drawn directly w/o using the software path
        // TODO: skip rrects once we're drawing them directly.
        if (prOut) {
            *prOut = nullptr;
        }
        return false;
    } else {
        // We shouldn't get here with an empty clip element.
        SkASSERT(Element::DeviceSpaceType::kEmpty != element->getDeviceSpaceType());

        // the gpu alpha mask will draw the inverse paths as non-inverse to a temp buffer
        SkPath path;
        element->asDeviceSpacePath(&path);
        if (path.isInverseFillType()) {
            path.toggleInverseFillType();
        }

        // We only use this method when rendering coverage clip masks.
        SkASSERT(renderTargetContext->numSamples() <= 1);
        auto aaType = (element->isAA()) ? GrAAType::kCoverage : GrAAType::kNone;

        GrPathRendererChain::DrawType type =
                needsStencil ? GrPathRendererChain::DrawType::kStencilAndColor
                             : GrPathRendererChain::DrawType::kColor;

        GrStyledShape shape(path, GrStyle::SimpleFill());
        GrPathRenderer::CanDrawPathArgs canDrawArgs;
        canDrawArgs.fCaps = context->priv().caps();
        canDrawArgs.fProxy = renderTargetContext->asRenderTargetProxy();
        canDrawArgs.fClipConservativeBounds = &scissorRect;
        canDrawArgs.fViewMatrix = &viewMatrix;
        canDrawArgs.fShape = &shape;
        canDrawArgs.fPaint = nullptr;
        canDrawArgs.fAAType = aaType;
        SkASSERT(!renderTargetContext->wrapsVkSecondaryCB());
        canDrawArgs.fTargetIsWrappedVkSecondaryCB = false;
        canDrawArgs.fHasUserStencilSettings = hasUserStencilSettings;

        // the 'false' parameter disallows use of the SW path renderer
        GrPathRenderer* pr =
            context->priv().drawingManager()->getPathRenderer(canDrawArgs, false, type);
        if (prOut) {
            *prOut = pr;
        }
        return SkToBool(!pr);
    }
}

/*
 * This method traverses the clip stack to see if the GrSoftwarePathRenderer
 * will be used on any element. If so, it returns true to indicate that the
 * entire clip should be rendered in SW and then uploaded en masse to the gpu.
 */
bool GrClipStackClip::UseSWOnlyPath(GrRecordingContext* context,
                                    bool hasUserStencilSettings,
                                    const GrRenderTargetContext* renderTargetContext,
                                    const GrReducedClip& reducedClip) {
    // TODO: right now it appears that GPU clip masks are strictly slower than software. We may
    // want to revisit this assumption once we can test with render target sorting.
    return true;

    // TODO: generalize this function so that when
    // a clip gets complex enough it can just be done in SW regardless
    // of whether it would invoke the GrSoftwarePathRenderer.

    // If we're avoiding stencils, always use SW. This includes drawing into a wrapped vulkan
    // secondary command buffer which can't handle stencils.
    if (context->priv().caps()->avoidStencilBuffers() ||
        renderTargetContext->wrapsVkSecondaryCB()) {
        return true;
    }

    // Set the matrix so that rendered clip elements are transformed to mask space from clip
    // space.
    SkMatrix translate;
    translate.setTranslate(SkIntToScalar(-reducedClip.left()), SkIntToScalar(-reducedClip.top()));

    for (ElementList::Iter iter(reducedClip.maskElements()); iter.get(); iter.next()) {
        const Element* element = iter.get();

        SkClipOp op = element->getOp();
        bool invert = element->isInverseFilled();
        bool needsStencil = invert ||
                            kIntersect_SkClipOp == op || kReverseDifference_SkClipOp == op;

        if (PathNeedsSWRenderer(context, reducedClip.scissor(), hasUserStencilSettings,
                                renderTargetContext, translate, element, nullptr, needsStencil)) {
            return true;
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// sort out what kind of clip mask needs to be created: alpha, stencil,
// scissor, or entirely software
bool GrClipStackClip::apply(GrRecordingContext* context, GrRenderTargetContext* renderTargetContext,
                            bool useHWAA, bool hasUserStencilSettings, GrAppliedClip* out,
                            SkRect* bounds) const {
    SkRect devBounds = SkRect::MakeIWH(renderTargetContext->width(), renderTargetContext->height());
    if (!devBounds.intersect(*bounds)) {
        return false;
    }

    if (!fStack || fStack->isWideOpen()) {
        return true;
    }

    // An default count of 4 was chosen because of the common pattern in Blink of:
    //   isect RR
    //   diff  RR
    //   isect convex_poly
    //   isect convex_poly
    // when drawing rounded div borders.
    constexpr int kMaxAnalyticFPs = 4;

    int maxWindowRectangles = renderTargetContext->priv().maxWindowRectangles();
    int maxAnalyticFPs = kMaxAnalyticFPs;
    if (renderTargetContext->numSamples() > 1 || useHWAA || hasUserStencilSettings) {
        // Disable analytic clips when we have MSAA. In MSAA we never conflate coverage and opacity.
        maxAnalyticFPs = 0;
        // We disable MSAA when avoiding stencil.
        SkASSERT(!context->priv().caps()->avoidStencilBuffers());
    }
    auto* ccpr = context->priv().drawingManager()->getCoverageCountingPathRenderer();

    GrReducedClip reducedClip(*fStack, devBounds, context->priv().caps(),
                              maxWindowRectangles, maxAnalyticFPs, ccpr ? maxAnalyticFPs : 0);
    if (InitialState::kAllOut == reducedClip.initialState() &&
        reducedClip.maskElements().isEmpty()) {
        return false;
    }

    if (reducedClip.hasScissor() && !GrClip::IsInsideClip(reducedClip.scissor(), devBounds)) {
        out->hardClip().addScissor(reducedClip.scissor(), bounds);
    }

    if (!reducedClip.windowRectangles().empty()) {
        out->hardClip().addWindowRectangles(reducedClip.windowRectangles(),
                                            GrWindowRectsState::Mode::kExclusive);
    }

    if (!reducedClip.maskElements().isEmpty()) {
        if (!this->applyClipMask(context, renderTargetContext, reducedClip, hasUserStencilSettings,
                                 out)) {
            return false;
        }
    }

    // The opsTask ID must not be looked up until AFTER producing the clip mask (if any). That step
    // can cause a flush or otherwise change which opstask our draw is going into.
    uint32_t opsTaskID = renderTargetContext->getOpsTask()->uniqueID();
    if (auto clipFPs = reducedClip.finishAndDetachAnalyticFPs(ccpr, opsTaskID)) {
        out->addCoverageFP(std::move(clipFPs));
    }

    return true;
}

bool GrClipStackClip::applyClipMask(GrRecordingContext* context,
                                    GrRenderTargetContext* renderTargetContext,
                                    const GrReducedClip& reducedClip, bool hasUserStencilSettings,
                                    GrAppliedClip* out) const {
#ifdef SK_DEBUG
    SkASSERT(reducedClip.hasScissor());
    SkIRect rtIBounds = SkIRect::MakeWH(renderTargetContext->width(),
                                        renderTargetContext->height());
    const SkIRect& scissor = reducedClip.scissor();
    SkASSERT(rtIBounds.contains(scissor)); // Mask shouldn't be larger than the RT.
#endif

    // MIXED SAMPLES TODO: We may want to explore using the stencil buffer for AA clipping.
    if ((renderTargetContext->numSamples() <= 1 && reducedClip.maskRequiresAA()) ||
        context->priv().caps()->avoidStencilBuffers() ||
        renderTargetContext->wrapsVkSecondaryCB()) {
        GrSurfaceProxyView result;
        if (UseSWOnlyPath(context, hasUserStencilSettings, renderTargetContext, reducedClip)) {
            // The clip geometry is complex enough that it will be more efficient to create it
            // entirely in software
            result = this->createSoftwareClipMask(context, reducedClip, renderTargetContext);
        } else {
            result = this->createAlphaClipMask(context, reducedClip);
        }

        if (result) {
            // The mask's top left coord should be pinned to the rounded-out top left corner of
            // the clip's device space bounds.
            out->addCoverageFP(create_fp_for_mask(std::move(result), reducedClip.scissor(),
                                                  *context->priv().caps()));
            return true;
        }

        // If alpha or software clip mask creation fails, fall through to the stencil code paths,
        // unless stencils are disallowed.
        if (context->priv().caps()->avoidStencilBuffers() ||
            renderTargetContext->wrapsVkSecondaryCB()) {
            SkDebugf("WARNING: Clip mask requires stencil, but stencil unavailable. "
                     "Clip will be ignored.\n");
            return false;
        }
    }

    reducedClip.drawStencilClipMask(context, renderTargetContext);
    out->hardClip().addStencilClip(reducedClip.maskGenID());
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Create a 8-bit clip mask in alpha

static void create_clip_mask_key(uint32_t clipGenID, const SkIRect& bounds, int numAnalyticFPs,
                                 GrUniqueKey* key) {
    static const GrUniqueKey::Domain kDomain = GrUniqueKey::GenerateDomain();
    GrUniqueKey::Builder builder(key, kDomain, 4, GrClipStackClip::kMaskTestTag);
    builder[0] = clipGenID;
    // SkToS16 because image filters outset layers to a size indicated by the filter, which can
    // sometimes result in negative coordinates from device space.
    builder[1] = SkToS16(bounds.fLeft) | (SkToS16(bounds.fRight) << 16);
    builder[2] = SkToS16(bounds.fTop) | (SkToS16(bounds.fBottom) << 16);
    builder[3] = numAnalyticFPs;
}

static void add_invalidate_on_pop_message(GrRecordingContext* context,
                                          const SkClipStack& stack, uint32_t clipGenID,
                                          const GrUniqueKey& clipMaskKey) {
    GrProxyProvider* proxyProvider = context->priv().proxyProvider();

    SkClipStack::Iter iter(stack, SkClipStack::Iter::kTop_IterStart);
    while (const Element* element = iter.prev()) {
        if (element->getGenID() == clipGenID) {
            element->addResourceInvalidationMessage(proxyProvider, clipMaskKey);
            return;
        }
    }
    SkDEBUGFAIL("Gen ID was not found in stack.");
}

static constexpr auto kMaskOrigin = kTopLeft_GrSurfaceOrigin;

static GrSurfaceProxyView find_mask(GrProxyProvider* provider, const GrUniqueKey& key) {
    return provider->findCachedProxyWithColorTypeFallback(key, kMaskOrigin, GrColorType::kAlpha_8,
                                                          1);
}

GrSurfaceProxyView GrClipStackClip::createAlphaClipMask(GrRecordingContext* context,
                                                        const GrReducedClip& reducedClip) const {
    GrProxyProvider* proxyProvider = context->priv().proxyProvider();
    GrUniqueKey key;
    create_clip_mask_key(reducedClip.maskGenID(), reducedClip.scissor(),
                         reducedClip.numAnalyticFPs(), &key);

    if (auto cachedView = find_mask(context->priv().proxyProvider(), key)) {
        return cachedView;
    }

    auto rtc = GrRenderTargetContext::MakeWithFallback(
            context, GrColorType::kAlpha_8, nullptr, SkBackingFit::kApprox,
            {reducedClip.width(), reducedClip.height()}, 1, GrMipMapped::kNo, GrProtected::kNo,
            kMaskOrigin);
    if (!rtc) {
        return {};
    }

    if (!reducedClip.drawAlphaClipMask(rtc.get())) {
        return {};
    }

    GrSurfaceProxyView result = rtc->readSurfaceView();
    if (!result || !result.asTextureProxy()) {
        return {};
    }

    SkASSERT(result.origin() == kMaskOrigin);
    proxyProvider->assignUniqueKeyToProxy(key, result.asTextureProxy());
    add_invalidate_on_pop_message(context, *fStack, reducedClip.maskGenID(), key);

    return result;
}

namespace {

/**
 * Payload class for use with GrTDeferredProxyUploader. The clip mask code renders multiple
 * elements, each storing their own AA setting (and already transformed into device space). This
 * stores all of the information needed by the worker thread to draw all clip elements (see below,
 * in createSoftwareClipMask).
 */
class ClipMaskData {
public:
    ClipMaskData(const GrReducedClip& reducedClip)
            : fScissor(reducedClip.scissor())
            , fInitialState(reducedClip.initialState()) {
        for (ElementList::Iter iter(reducedClip.maskElements()); iter.get(); iter.next()) {
            fElements.addToTail(*iter.get());
        }
    }

    const SkIRect& scissor() const { return fScissor; }
    InitialState initialState() const { return fInitialState; }
    const ElementList& elements() const { return fElements; }

private:
    SkIRect fScissor;
    InitialState fInitialState;
    ElementList fElements;
};

}

static void draw_clip_elements_to_mask_helper(GrSWMaskHelper& helper, const ElementList& elements,
                                              const SkIRect& scissor, InitialState initialState) {
    // Set the matrix so that rendered clip elements are transformed to mask space from clip space.
    SkMatrix translate;
    translate.setTranslate(SkIntToScalar(-scissor.left()), SkIntToScalar(-scissor.top()));

    helper.clear(InitialState::kAllIn == initialState ? 0xFF : 0x00);

    for (ElementList::Iter iter(elements); iter.get(); iter.next()) {
        const Element* element = iter.get();
        SkClipOp op = element->getOp();
        GrAA aa = GrAA(element->isAA());

        if (kIntersect_SkClipOp == op || kReverseDifference_SkClipOp == op) {
            // Intersect and reverse difference require modifying pixels outside of the geometry
            // that is being "drawn". In both cases we erase all the pixels outside of the geometry
            // but leave the pixels inside the geometry alone. For reverse difference we invert all
            // the pixels before clearing the ones outside the geometry.
            if (kReverseDifference_SkClipOp == op) {
                SkRect temp = SkRect::Make(scissor);
                // invert the entire scene
                helper.drawRect(temp, translate, SkRegion::kXOR_Op, GrAA::kNo, 0xFF);
            }
            SkPath clipPath;
            element->asDeviceSpacePath(&clipPath);
            clipPath.toggleInverseFillType();
            helper.drawShape(GrShape(clipPath), translate, SkRegion::kReplace_Op, aa, 0x00);
            continue;
        }

        // The other ops (union, xor, diff) only affect pixels inside
        // the geometry so they can just be drawn normally
        if (Element::DeviceSpaceType::kRect == element->getDeviceSpaceType()) {
            helper.drawRect(element->getDeviceSpaceRect(), translate, (SkRegion::Op)op, aa, 0xFF);
        } else if (Element::DeviceSpaceType::kRRect == element->getDeviceSpaceType()) {
            helper.drawRRect(element->getDeviceSpaceRRect(), translate, (SkRegion::Op)op, aa, 0xFF);
        } else {
            SkPath path;
            element->asDeviceSpacePath(&path);
            helper.drawShape(GrShape(path), translate, (SkRegion::Op)op, aa, 0xFF);
        }
    }
}

GrSurfaceProxyView GrClipStackClip::createSoftwareClipMask(
        GrRecordingContext* context, const GrReducedClip& reducedClip,
        GrRenderTargetContext* renderTargetContext) const {
    GrUniqueKey key;
    create_clip_mask_key(reducedClip.maskGenID(), reducedClip.scissor(),
                         reducedClip.numAnalyticFPs(), &key);

    GrProxyProvider* proxyProvider = context->priv().proxyProvider();

    if (auto cachedView = find_mask(proxyProvider, key)) {
        return cachedView;
    }

    // The mask texture may be larger than necessary. We round out the clip bounds and pin the top
    // left corner of the resulting rect to the top left of the texture.
    SkIRect maskSpaceIBounds = SkIRect::MakeWH(reducedClip.width(), reducedClip.height());

    SkTaskGroup* taskGroup = nullptr;
    if (auto direct = context->priv().asDirectContext()) {
        taskGroup = direct->priv().getTaskGroup();
    }

    GrSurfaceProxyView view;
    if (taskGroup && renderTargetContext) {
        const GrCaps* caps = context->priv().caps();
        // Create our texture proxy
        GrBackendFormat format = caps->getDefaultBackendFormat(GrColorType::kAlpha_8,
                                                               GrRenderable::kNo);

        GrSwizzle swizzle = context->priv().caps()->getReadSwizzle(format, GrColorType::kAlpha_8);

        // MDB TODO: We're going to fill this proxy with an ASAP upload (which is out of order wrt
        // to ops), so it can't have any pending IO.
        auto proxy = proxyProvider->createProxy(format,
                                                maskSpaceIBounds.size(),
                                                GrRenderable::kNo,
                                                1,
                                                GrMipMapped::kNo,
                                                SkBackingFit::kApprox,
                                                SkBudgeted::kYes,
                                                GrProtected::kNo);

        auto uploader = std::make_unique<GrTDeferredProxyUploader<ClipMaskData>>(reducedClip);
        GrTDeferredProxyUploader<ClipMaskData>* uploaderRaw = uploader.get();
        auto drawAndUploadMask = [uploaderRaw, maskSpaceIBounds] {
            TRACE_EVENT0("skia.gpu", "Threaded SW Clip Mask Render");
            GrSWMaskHelper helper(uploaderRaw->getPixels());
            if (helper.init(maskSpaceIBounds)) {
                draw_clip_elements_to_mask_helper(helper, uploaderRaw->data().elements(),
                                                  uploaderRaw->data().scissor(),
                                                  uploaderRaw->data().initialState());
            } else {
                SkDEBUGFAIL("Unable to allocate SW clip mask.");
            }
            uploaderRaw->signalAndFreeData();
        };

        taskGroup->add(std::move(drawAndUploadMask));
        proxy->texPriv().setDeferredUploader(std::move(uploader));

        view = {std::move(proxy), kMaskOrigin, swizzle};
    } else {
        GrSWMaskHelper helper;
        if (!helper.init(maskSpaceIBounds)) {
            return {};
        }

        draw_clip_elements_to_mask_helper(helper, reducedClip.maskElements(), reducedClip.scissor(),
                                          reducedClip.initialState());

        view = helper.toTextureView(context, SkBackingFit::kApprox);
    }

    SkASSERT(view);
    SkASSERT(view.origin() == kMaskOrigin);
    proxyProvider->assignUniqueKeyToProxy(key, view.asTextureProxy());
    add_invalidate_on_pop_message(context, *fStack, reducedClip.maskGenID(), key);
    return view;
}
