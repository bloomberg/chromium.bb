/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkSGGroup.h"

#include <algorithm>

namespace sksg {

Group::Group(std::vector<sk_sp<RenderNode>> children)
    : fChildren(std::move(children)) {
    for (const auto& child : fChildren) {
        this->observeInval(child);
    }
}

Group::~Group() {
    for (const auto& child : fChildren) {
        this->unobserveInval(child);
    }
}

void Group::addChild(sk_sp<RenderNode> node) {
    // should we allow duplicates?
    for (const auto& child : fChildren) {
        if (child == node) {
            return;
        }
    }

    this->observeInval(node);
    fChildren.push_back(std::move(node));

    this->invalidate();
}

void Group::removeChild(const sk_sp<RenderNode>& node) {
    SkDEBUGCODE(const auto origSize = fChildren.size());
    fChildren.erase(std::remove(fChildren.begin(), fChildren.end(), node), fChildren.end());
    SkASSERT(fChildren.size() == origSize - 1);

    this->unobserveInval(node);
    this->invalidate();
}

void Group::onRender(SkCanvas* canvas, const RenderContext* ctx) const {
    // TODO: this heuristic works at the moment, but:
    //   a) it is fragile because it relies on all leaf render nodes being atomic draws
    //   b) could be improved by e.g. detecting all leaf render draws are non-overlapping
    const auto isolate = fChildren.size() > 1;
    const auto local_ctx = ScopedRenderContext(canvas, ctx).setIsolation(this->bounds(), isolate);

    for (const auto& child : fChildren) {
        child->render(canvas, local_ctx);
    }
}

SkRect Group::onRevalidate(InvalidationController* ic, const SkMatrix& ctm) {
    SkASSERT(this->hasInval());

    SkRect bounds = SkRect::MakeEmpty();

    for (const auto& child : fChildren) {
        bounds.join(child->revalidate(ic, ctm));
    }

    return bounds;
}

} // namespace sksg
