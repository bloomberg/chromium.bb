/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrOpsTask_DEFINED
#define GrOpsTask_DEFINED

#include "include/core/SkMatrix.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkStrokeRec.h"
#include "include/core/SkTypes.h"
#include "include/gpu/GrRecordingContext.h"
#include "include/private/SkColorData.h"
#include "include/private/SkTArray.h"
#include "include/private/SkTDArray.h"
#include "src/core/SkArenaAlloc.h"
#include "src/core/SkClipStack.h"
#include "src/core/SkStringUtils.h"
#include "src/core/SkTLazy.h"
#include "src/gpu/GrAppliedClip.h"
#include "src/gpu/GrPathRendering.h"
#include "src/gpu/GrPrimitiveProcessor.h"
#include "src/gpu/GrRenderTask.h"
#include "src/gpu/ops/GrDrawOp.h"
#include "src/gpu/ops/GrOp.h"

class GrAuditTrail;
class GrCaps;
class GrClearOp;
class GrGpuBuffer;
class GrRenderTargetProxy;

class GrOpsTask : public GrRenderTask {
private:
    using DstProxyView = GrXferProcessor::DstProxyView;

public:
    // The Arenas must outlive the GrOpsTask, either by preserving the context that owns
    // the pool, or by moving the pool to the DDL that takes over the GrOpsTask.
    GrOpsTask(GrDrawingManager*, GrRecordingContext::Arenas, GrSurfaceProxyView, GrAuditTrail*);
    ~GrOpsTask() override;

    GrOpsTask* asOpsTask() override { return this; }

    bool isEmpty() const { return fOpChains.empty(); }

    /**
     * Empties the draw buffer of any queued up draws.
     */
    void endFlush(GrDrawingManager*) override;

    void onPrePrepare(GrRecordingContext*) override;
    /**
     * Together these two functions flush all queued up draws to GrCommandBuffer. The return value
     * of executeOps() indicates whether any commands were actually issued to the GPU.
     */
    void onPrepare(GrOpFlushState* flushState) override;
    bool onExecute(GrOpFlushState* flushState) override;

    void addSampledTexture(GrSurfaceProxy* proxy) {
        // This function takes a GrSurfaceProxy because all subsequent uses of the proxy do not
        // require the specifics of GrTextureProxy, so this avoids a number of unnecessary virtual
        // asTextureProxy() calls. However, sampling the proxy implicitly requires that the proxy
        // be a texture. Eventually, when proxies are a unified type with flags, this can just
        // assert that capability.
        SkASSERT(proxy->asTextureProxy());
        fSampledProxies.push_back(proxy);
    }

    void addOp(GrDrawingManager* drawingMgr, GrOp::Owner op,
               GrTextureResolveManager textureResolveManager, const GrCaps& caps) {
        auto addDependency = [ drawingMgr, textureResolveManager, &caps, this ] (
                GrSurfaceProxy* p, GrMipmapped mipmapped) {
            this->addDependency(drawingMgr, p, mipmapped, textureResolveManager, caps);
        };

        op->visitProxies(addDependency);

        this->recordOp(std::move(op), GrProcessorSet::EmptySetAnalysis(), nullptr, nullptr, caps);
    }

    void addDrawOp(GrDrawingManager* drawingMgr, GrOp::Owner op,
                   const GrProcessorSet::Analysis& processorAnalysis,
                   GrAppliedClip&& clip, const DstProxyView& dstProxyView,
                   GrTextureResolveManager textureResolveManager, const GrCaps& caps) {
        auto addDependency = [ drawingMgr, textureResolveManager, &caps, this ] (
                GrSurfaceProxy* p, GrMipmapped mipmapped) {
            this->addSampledTexture(p);
            this->addDependency(drawingMgr, p, mipmapped, textureResolveManager, caps);
        };

        op->visitProxies(addDependency);
        clip.visitProxies(addDependency);
        if (dstProxyView.proxy()) {
            if (GrDstSampleTypeUsesTexture(dstProxyView.dstSampleType())) {
                this->addSampledTexture(dstProxyView.proxy());
            }
            addDependency(dstProxyView.proxy(), GrMipmapped::kNo);
            if (this->target(0).proxy() == dstProxyView.proxy()) {
                // Since we are sampling and drawing to the same surface we will need to use
                // texture barriers.
                SkASSERT(GrDstSampleTypeDirectlySamplesDst(dstProxyView.dstSampleType()));
                fRenderPassXferBarriers |= GrXferBarrierFlags::kTexture;
            }
            SkASSERT(dstProxyView.dstSampleType() != GrDstSampleType::kAsInputAttachment ||
                     dstProxyView.offset().isZero());
        }

        if (processorAnalysis.usesNonCoherentHWBlending()) {
            fRenderPassXferBarriers |= GrXferBarrierFlags::kBlend;
        }

        this->recordOp(std::move(op), processorAnalysis, clip.doesClip() ? &clip : nullptr,
                       &dstProxyView, caps);
    }

    void discard();

#ifdef SK_DEBUG
    int numClips() const override { return fNumClips; }
    void visitProxies_debugOnly(const GrOp::VisitProxyFunc&) const override;
#endif

#if GR_TEST_UTILS
    void dump(bool printDependencies) const override;
    const char* name() const final { return "Ops"; }
    int numOpChains() const { return fOpChains.count(); }
    const GrOp* getChain(int index) const { return fOpChains[index].head(); }
#endif

private:
    bool isNoOp() const {
        // TODO: GrLoadOp::kDiscard (i.e., storing a discard) should also be grounds for skipping
        // execution. We currently don't because of Vulkan. See http://skbug.com/9373.
        //
        // TODO: We should also consider stencil load/store here. We get away with it for now
        // because we never discard stencil buffers.
        return fOpChains.empty() && GrLoadOp::kLoad == fColorLoadOp;
    }

    void deleteOps();

    enum class StencilContent {
        kDontCare,
        kUserBitsCleared,  // User bits: cleared
                           // Clip bit: don't care (Ganesh always pre-clears the clip bit.)
        kPreserved
    };

    // Lets the caller specify what the content of the stencil buffer should be at the beginning
    // of the render pass.
    //
    // When requesting kClear: Tilers will load the stencil buffer with a "clear" op; non-tilers
    // will clear the stencil on first load, and then preserve it on subsequent loads. (Preserving
    // works because renderTargetContexts are required to leave the user bits in a cleared state
    // once finished.)
    //
    // NOTE: initialContent must not be kClear if caps.performStencilClearsAsDraws() is true.
    void setInitialStencilContent(StencilContent initialContent) {
        fInitialStencilContent = initialContent;
    }

    // If a renderTargetContext splits its opsTask, it uses this method to guarantee stencil values
    // get preserved across its split tasks.
    void setMustPreserveStencil() { fMustPreserveStencil = true; }

    // Must only be called if native color buffer clearing is enabled.
    void setColorLoadOp(GrLoadOp op, const SkPMColor4f& color);
    // Sets the clear color to transparent black
    void setColorLoadOp(GrLoadOp op) {
        static const SkPMColor4f kDefaultClearColor = {0.f, 0.f, 0.f, 0.f};
        this->setColorLoadOp(op, kDefaultClearColor);
    }

    enum class CanDiscardPreviousOps : bool {
        kYes = true,
        kNo = false
    };

    // Perform book-keeping for a fullscreen clear, regardless of how the clear is implemented later
    // (i.e. setColorLoadOp(), adding a ClearOp, or adding a GrFillRectOp that covers the device).
    // Returns true if the clear can be converted into a load op (barring device caps).
    bool resetForFullscreenClear(CanDiscardPreviousOps);

    class OpChain {
    public:
        OpChain(GrOp::Owner, GrProcessorSet::Analysis, GrAppliedClip*, const DstProxyView*);
        ~OpChain() {
            // The ops are stored in a GrMemoryPool and must be explicitly deleted via the pool.
            SkASSERT(fList.empty());
        }

        OpChain(const OpChain&) = delete;
        OpChain& operator=(const OpChain&) = delete;
        OpChain(OpChain&&) = default;
        OpChain& operator=(OpChain&&) = default;

        void visitProxies(const GrOp::VisitProxyFunc&) const;

        GrOp* head() const { return fList.head(); }

        GrAppliedClip* appliedClip() const { return fAppliedClip; }
        const DstProxyView& dstProxyView() const { return fDstProxyView; }
        const SkRect& bounds() const { return fBounds; }

        // Deletes all the ops in the chain.
        void deleteOps();

        // Attempts to move the ops from the passed chain to this chain at the head. Also attempts
        // to merge ops between the chains. Upon success the passed chain is empty.
        // Fails when the chains aren't of the same op type, have different clips or dst proxies.
        bool prependChain(OpChain*, const GrCaps&, GrRecordingContext::Arenas*, GrAuditTrail*);

        // Attempts to add 'op' to this chain either by merging or adding to the tail. Returns
        // 'op' to the caller upon failure, otherwise null. Fails when the op and chain aren't of
        // the same op type, have different clips or dst proxies.
        GrOp::Owner appendOp(GrOp::Owner op, GrProcessorSet::Analysis,
                             const DstProxyView*, const GrAppliedClip*, const GrCaps&,
                             GrRecordingContext::Arenas*, GrAuditTrail*);

        void setSkipExecuteFlag() { fSkipExecute = true; }
        bool shouldExecute() const {
            return SkToBool(this->head()) && !fSkipExecute;
        }

    private:
        class List {
        public:
            List() = default;
            List(GrOp::Owner);
            List(List&&);
            List& operator=(List&& that);

            bool empty() const { return !SkToBool(fHead); }
            GrOp* head() const { return fHead.get(); }
            GrOp* tail() const { return fTail; }

            GrOp::Owner popHead();
            GrOp::Owner removeOp(GrOp* op);
            void pushHead(GrOp::Owner op);
            void pushTail(GrOp::Owner);

            void validate() const;

        private:
            GrOp::Owner fHead{nullptr};
            GrOp* fTail{nullptr};
        };

        void validate() const;

        bool tryConcat(List*, GrProcessorSet::Analysis, const DstProxyView&, const GrAppliedClip*,
                       const SkRect& bounds, const GrCaps&, GrRecordingContext::Arenas*,
                       GrAuditTrail*);
        static List DoConcat(List, List, const GrCaps&, GrRecordingContext::Arenas*, GrAuditTrail*);

        List fList;
        GrProcessorSet::Analysis fProcessorAnalysis;
        DstProxyView fDstProxyView;
        GrAppliedClip* fAppliedClip;
        SkRect fBounds;

        // We set this flag to true if any of the ops' proxies fail to instantiate so that we know
        // not to try and draw the op.
        bool fSkipExecute = false;
    };


    bool onIsUsed(GrSurfaceProxy*) const override;

    void handleInternalAllocationFailure() override;

    void gatherProxyIntervals(GrResourceAllocator*) const override;

    void recordOp(GrOp::Owner, GrProcessorSet::Analysis, GrAppliedClip*,
                  const DstProxyView*, const GrCaps& caps);

    void forwardCombine(const GrCaps&);

    ExpectedOutcome onMakeClosed(const GrCaps& caps, SkIRect* targetUpdateBounds) override;

    friend class OpsTaskTestingAccess;
    friend class GrRenderTargetContextPriv; // for stencil clip state. TODO: this is invasive

    // The RTC and OpsTask have to work together to handle buffer clears. In most cases, buffer
    // clearing can be done natively, in which case the op list's load ops are sufficient. In other
    // cases, draw ops must be used, which makes the RTC the best place for those decisions. This,
    // however, requires that the RTC be able to coordinate with the op list to achieve similar ends
    friend class GrRenderTargetContext;

    // This is a backpointer to the Arenas that holds the memory for this GrOpsTask's ops. In the
    // DDL case, the Arenas must have been detached from the original recording context and moved
    // into the owning DDL.
    GrRecordingContext::Arenas fArenas;
    GrAuditTrail*              fAuditTrail;

    GrLoadOp fColorLoadOp = GrLoadOp::kLoad;
    SkPMColor4f fLoadClearColor = SK_PMColor4fTRANSPARENT;
    StencilContent fInitialStencilContent = StencilContent::kDontCare;
    bool fMustPreserveStencil = false;

    uint32_t fLastClipStackGenID = SK_InvalidUniqueID;
    SkIRect fLastDevClipBounds;
    int fLastClipNumAnalyticElements;

    GrXferBarrierFlags fRenderPassXferBarriers = GrXferBarrierFlags::kNone;

    // For ops/opsTask we have mean: 5 stdDev: 28
    SkSTArray<25, OpChain> fOpChains;

    // MDB TODO: 4096 for the first allocation of the clip space will be huge overkill.
    // Gather statistics to determine the correct size.
    SkArenaAllocWithReset fClipAllocator{4096};
    SkDEBUGCODE(int fNumClips;)

    // TODO: We could look into this being a set if we find we're adding a lot of duplicates that is
    // causing slow downs.
    SkTArray<GrSurfaceProxy*, true> fSampledProxies;

    SkRect fTotalBounds = SkRect::MakeEmpty();
    SkIRect fClippedContentBounds = SkIRect::MakeEmpty();
};

#endif
