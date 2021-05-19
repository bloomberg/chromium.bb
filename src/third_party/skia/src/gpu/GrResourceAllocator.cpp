/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/GrResourceAllocator.h"

#include "src/gpu/GrGpuResourcePriv.h"
#include "src/gpu/GrOpsTask.h"
#include "src/gpu/GrRenderTargetProxy.h"
#include "src/gpu/GrResourceProvider.h"
#include "src/gpu/GrSurfaceProxy.h"
#include "src/gpu/GrSurfaceProxyPriv.h"

#ifdef SK_DEBUG
#include <atomic>

uint32_t GrResourceAllocator::Interval::CreateUniqueID() {
    static std::atomic<uint32_t> nextID{1};
    uint32_t id;
    do {
        id = nextID.fetch_add(1, std::memory_order_relaxed);
    } while (id == SK_InvalidUniqueID);
    return id;
}

uint32_t GrResourceAllocator::Register::CreateUniqueID() {
    static std::atomic<uint32_t> nextID{1};
    uint32_t id;
    do {
        id = nextID.fetch_add(1, std::memory_order_relaxed);
    } while (id == SK_InvalidUniqueID);
    return id;
}
#endif

GrResourceAllocator::~GrResourceAllocator() {
    SkASSERT(fIntvlList.empty());
    SkASSERT(fActiveIntvls.empty());
    SkASSERT(!fIntvlHash.count());
}

void GrResourceAllocator::addInterval(GrSurfaceProxy* proxy, unsigned int start, unsigned int end,
                                      ActualUse actualUse
                                      SkDEBUGCODE(, bool isDirectDstRead)) {
    SkASSERT(start <= end);
    SkASSERT(!fAssigned);  // We shouldn't be adding any intervals after (or during) assignment

    if (proxy->canSkipResourceAllocator()) {
        return;
    }

    // If a proxy is read only it must refer to a texture with specific content that cannot be
    // recycled. We don't need to assign a texture to it and no other proxy can be instantiated
    // with the same texture.
    if (proxy->readOnly()) {
        if (proxy->isLazy() && !proxy->priv().doLazyInstantiation(fResourceProvider)) {
            fFailedInstantiation = true;
        } else {
            // Since we aren't going to add an interval we won't revisit this proxy in assign(). So
            // must already be instantiated or it must be a lazy proxy that we instantiated above.
            SkASSERT(proxy->isInstantiated());
        }
        return;
    }
    uint32_t proxyID = proxy->uniqueID().asUInt();
    if (Interval** intvlPtr = fIntvlHash.find(proxyID)) {
        // Revise the interval for an existing use
        Interval* intvl = *intvlPtr;
#ifdef SK_DEBUG
        if (0 == start && 0 == end) {
            // This interval is for the initial upload to a deferred proxy. Due to the vagaries
            // of how deferred proxies are collected they can appear as uploads multiple times
            // in a single opsTasks' list and as uploads in several opsTasks.
            SkASSERT(0 == intvl->start());
        } else if (isDirectDstRead) {
            // Direct reads from the render target itself should occur w/in the existing
            // interval
            SkASSERT(intvl->start() <= start && intvl->end() >= end);
        } else {
            SkASSERT(intvl->end() <= start && intvl->end() <= end);
        }
#endif
        if (ActualUse::kYes == actualUse) {
            intvl->addUse();
        }
        intvl->extendEnd(end);
        return;
    }
    Interval* newIntvl = fInternalAllocator.make<Interval>(proxy, start, end);

    if (ActualUse::kYes == actualUse) {
        newIntvl->addUse();
    }
    fIntvlList.insertByIncreasingStart(newIntvl);
    fIntvlHash.set(proxyID, newIntvl);
}

bool GrResourceAllocator::Register::isRecyclable(const GrCaps& caps,
                                                 GrSurfaceProxy* proxy,
                                                 int knownUseCount) const {
    if (!caps.reuseScratchTextures() && !proxy->asRenderTargetProxy()) {
        // Tragically, scratch texture reuse is totally disabled in this case.
        return false;
    }

    if (!this->scratchKey().isValid()) {
        return false; // no scratch key, no free pool
    }
    if (this->uniqueKey().isValid()) {
        return false; // rely on the resource cache to hold onto uniquely-keyed surfaces.
    }
    // If all the refs on the proxy are known to the resource allocator then no one
    // should be holding onto it outside of Ganesh.
    return !proxy->refCntGreaterThan(knownUseCount);
}

bool GrResourceAllocator::Register::instantiateSurface(GrSurfaceProxy* proxy,
                                                       GrResourceProvider* resourceProvider) {
    SkASSERT(!proxy->peekSurface());

    sk_sp<GrSurface> surface;
    if (const auto& uniqueKey = proxy->getUniqueKey(); uniqueKey.isValid()) {
        SkASSERT(uniqueKey == fOriginatingProxy->getUniqueKey());
        // First try to reattach to a cached surface if the proxy is uniquely keyed
        surface = resourceProvider->findByUniqueKey<GrSurface>(uniqueKey);
    }
    if (!surface) {
        if (proxy == fOriginatingProxy) {
            surface = proxy->priv().createSurface(resourceProvider);
        } else {
            surface = sk_ref_sp(fOriginatingProxy->peekSurface());
        }
    }
    if (!surface) {
        return false;
    }

    // Make surface budgeted if this proxy is budgeted.
    if (SkBudgeted::kYes == proxy->isBudgeted() &&
        GrBudgetedType::kBudgeted != surface->resourcePriv().budgetedType()) {
        // This gets the job done but isn't quite correct. It would be better to try to
        // match budgeted proxies w/ budgeted surfaces and unbudgeted w/ unbudgeted.
        surface->resourcePriv().makeBudgeted();
    }

    // Propagate the proxy unique key to the surface if we have one.
    if (const auto& uniqueKey = proxy->getUniqueKey(); uniqueKey.isValid()) {
        if (!surface->getUniqueKey().isValid()) {
            resourceProvider->assignUniqueKeyToResource(uniqueKey, surface.get());
        }
        SkASSERT(surface->getUniqueKey() == uniqueKey);
    }
    proxy->priv().assign(std::move(surface));
    return true;
}

GrResourceAllocator::Interval* GrResourceAllocator::IntervalList::popHead() {
    SkDEBUGCODE(this->validate());

    Interval* temp = fHead;
    if (temp) {
        fHead = temp->next();
        if (!fHead) {
            fTail = nullptr;
        }
        temp->setNext(nullptr);
    }

    SkDEBUGCODE(this->validate());
    return temp;
}

// TODO: fuse this with insertByIncreasingEnd
void GrResourceAllocator::IntervalList::insertByIncreasingStart(Interval* intvl) {
    SkDEBUGCODE(this->validate());
    SkASSERT(!intvl->next());

    if (!fHead) {
        // 14%
        fHead = fTail = intvl;
    } else if (intvl->start() <= fHead->start()) {
        // 3%
        intvl->setNext(fHead);
        fHead = intvl;
    } else if (fTail->start() <= intvl->start()) {
        // 83%
        fTail->setNext(intvl);
        fTail = intvl;
    } else {
        // almost never
        Interval* prev = fHead;
        Interval* next = prev->next();
        for (; intvl->start() > next->start(); prev = next, next = next->next()) {
        }

        SkASSERT(next);
        intvl->setNext(next);
        prev->setNext(intvl);
    }

    SkDEBUGCODE(this->validate());
}

// TODO: fuse this with insertByIncreasingStart
void GrResourceAllocator::IntervalList::insertByIncreasingEnd(Interval* intvl) {
    SkDEBUGCODE(this->validate());
    SkASSERT(!intvl->next());

    if (!fHead) {
        // 14%
        fHead = fTail = intvl;
    } else if (intvl->end() <= fHead->end()) {
        // 64%
        intvl->setNext(fHead);
        fHead = intvl;
    } else if (fTail->end() <= intvl->end()) {
        // 3%
        fTail->setNext(intvl);
        fTail = intvl;
    } else {
        // 19% but 81% of those land right after the list's head
        Interval* prev = fHead;
        Interval* next = prev->next();
        for (; intvl->end() > next->end(); prev = next, next = next->next()) {
        }

        SkASSERT(next);
        intvl->setNext(next);
        prev->setNext(intvl);
    }

    SkDEBUGCODE(this->validate());
}

#ifdef SK_DEBUG
void GrResourceAllocator::IntervalList::validate() const {
    SkASSERT(SkToBool(fHead) == SkToBool(fTail));

    Interval* prev = nullptr;
    for (Interval* cur = fHead; cur; prev = cur, cur = cur->next()) {
    }

    SkASSERT(fTail == prev);
}
#endif

// First try to reuse one of the recently allocated/used registers in the free pool.
GrResourceAllocator::Register* GrResourceAllocator::findOrCreateRegisterFor(GrSurfaceProxy* proxy) {
    // Handle uniquely keyed proxies
    if (const auto& uniqueKey = proxy->getUniqueKey(); uniqueKey.isValid()) {
        if (auto p = fUniqueKeyRegisters.find(uniqueKey)) {
            return *p;
        }
        // No need for a scratch key. These don't go in the free pool.
        Register* r = fInternalAllocator.make<Register>(proxy, GrScratchKey());
        fUniqueKeyRegisters.set(uniqueKey, r);
        return r;
    }

    // Then look in the free pool
    GrScratchKey scratchKey;
    proxy->priv().computeScratchKey(*fResourceProvider->caps(), &scratchKey);

    auto filter = [] (const Register* r) {
        return true;
    };
    if (Register* r = fFreePool.findAndRemove(scratchKey, filter)) {
        return r;
    }

    return fInternalAllocator.make<Register>(proxy, std::move(scratchKey));
}

// Remove any intervals that end before the current index. Add their registers
// to the free pool if possible.
void GrResourceAllocator::expire(unsigned int curIndex) {
    while (!fActiveIntvls.empty() && fActiveIntvls.peekHead()->end() < curIndex) {
        Interval* intvl = fActiveIntvls.popHead();
        SkASSERT(!intvl->next());

        Register* r = intvl->getRegister();
        if (r && r->isRecyclable(*fResourceProvider->caps(), intvl->proxy(), intvl->uses())) {
#if GR_ALLOCATION_SPEW
            SkDebugf("putting register %d back into pool\n", r->uniqueID());
#endif
            // TODO: fix this insertion so we get a more LRU-ish behavior
            fFreePool.insert(r->scratchKey(), r);
        }
        fFinishedIntvls.insertByIncreasingStart(intvl);
    }
}

bool GrResourceAllocator::assign() {
    fIntvlHash.reset(); // we don't need the interval hash anymore

    SkDEBUGCODE(fAssigned = true;)

    if (fIntvlList.empty()) {
        return !fFailedInstantiation;          // no resources to assign
    }

#if GR_ALLOCATION_SPEW
    SkDebugf("assigning %d ops\n", fNumOps);
    this->dumpIntervals();
#endif

    while (Interval* cur = fIntvlList.popHead()) {
        this->expire(cur->start());

        // Already-instantiated proxies and lazy proxies don't use registers.
        // No need to compute scratch keys (or CANT, in the case of fully-lazy).
        if (cur->proxy()->isInstantiated() || cur->proxy()->isLazy()) {
            fActiveIntvls.insertByIncreasingEnd(cur);

            continue;
        }

        Register* r = this->findOrCreateRegisterFor(cur->proxy());
#if GR_ALLOCATION_SPEW
        SkDebugf("Assigning register %d to %d\n",
             r->uniqueID(),
             cur->proxy()->uniqueID().asUInt());
#endif
        SkASSERT(!cur->proxy()->peekSurface());
        cur->setRegister(r);

        fActiveIntvls.insertByIncreasingEnd(cur);
    }

    // expire all the remaining intervals to drain the active interval list
    this->expire(std::numeric_limits<unsigned int>::max());

    // TODO: Return here and give the caller a chance to estimate memory cost and bail before
    // instantiating anything.

    // Instantiate surfaces
    while (Interval* cur = fFinishedIntvls.popHead()) {
        if (fFailedInstantiation) {
            break;
        }
        if (cur->proxy()->isInstantiated()) {
            continue;
        }
        if (cur->proxy()->isLazy()) {
            fFailedInstantiation = !cur->proxy()->priv().doLazyInstantiation(fResourceProvider);
            continue;
        }
        Register* r = cur->getRegister();
        SkASSERT(r);
        fFailedInstantiation = !r->instantiateSurface(cur->proxy(), fResourceProvider);
    }
    return !fFailedInstantiation;
}

#if GR_ALLOCATION_SPEW
void GrResourceAllocator::dumpIntervals() {
    // Print all the intervals while computing their range
    SkDebugf("------------------------------------------------------------\n");
    unsigned int min = std::numeric_limits<unsigned int>::max();
    unsigned int max = 0;
    for(const Interval* cur = fIntvlList.peekHead(); cur; cur = cur->next()) {
        SkDebugf("{ %3d,%3d }: [%2d, %2d] - refProxys:%d surfaceRefs:%d\n",
                 cur->proxy()->uniqueID().asUInt(),
                 cur->proxy()->isInstantiated() ? cur->proxy()->underlyingUniqueID().asUInt() : -1,
                 cur->start(),
                 cur->end(),
                 cur->proxy()->priv().getProxyRefCnt(),
                 cur->proxy()->testingOnly_getBackingRefCnt());
        min = std::min(min, cur->start());
        max = std::max(max, cur->end());
    }

    // Draw a graph of the useage intervals
    for(const Interval* cur = fIntvlList.peekHead(); cur; cur = cur->next()) {
        SkDebugf("{ %3d,%3d }: ",
                 cur->proxy()->uniqueID().asUInt(),
                 cur->proxy()->isInstantiated() ? cur->proxy()->underlyingUniqueID().asUInt() : -1);
        for (unsigned int i = min; i <= max; ++i) {
            if (i >= cur->start() && i <= cur->end()) {
                SkDebugf("x");
            } else {
                SkDebugf(" ");
            }
        }
        SkDebugf("\n");
    }
}
#endif
