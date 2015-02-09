/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/layout/LayoutRuby.h"

#include "core/frame/UseCounter.h"
#include "core/layout/LayoutRubyRun.h"
#include "core/layout/style/LayoutStyle.h"
#include "wtf/RefPtr.h"

namespace blink {

// === generic helper functions to avoid excessive code duplication ===

static inline bool isAnonymousRubyInlineBlock(const LayoutObject* object)
{
    ASSERT(!object
        || !object->parent()->isRuby()
        || object->isRubyRun()
        || (object->isInline() && (object->isBeforeContent() || object->isAfterContent()))
        || (object->isAnonymous() && object->isRenderBlock() && object->style()->display() == INLINE_BLOCK));

    return object
        && object->parent()->isRuby()
        && object->isRenderBlock()
        && !object->isRubyRun();
}

static inline bool isRubyBeforeBlock(const LayoutObject* object)
{
    return isAnonymousRubyInlineBlock(object)
        && !object->previousSibling()
        && toRenderBlock(object)->firstChild()
        && toRenderBlock(object)->firstChild()->style()->styleType() == BEFORE;
}

static inline bool isRubyAfterBlock(const LayoutObject* object)
{
    return isAnonymousRubyInlineBlock(object)
        && !object->nextSibling()
        && toRenderBlock(object)->firstChild()
        && toRenderBlock(object)->firstChild()->style()->styleType() == AFTER;
}

static inline RenderBlock* rubyBeforeBlock(const LayoutObject* ruby)
{
    LayoutObject* child = ruby->slowFirstChild();
    return isRubyBeforeBlock(child) ? toRenderBlock(child) : 0;
}

static inline RenderBlock* rubyAfterBlock(const LayoutObject* ruby)
{
    LayoutObject* child = ruby->slowLastChild();
    return isRubyAfterBlock(child) ? toRenderBlock(child) : 0;
}

static RenderBlockFlow* createAnonymousRubyInlineBlock(LayoutObject* ruby)
{
    RefPtr<LayoutStyle> newStyle = LayoutStyle::createAnonymousStyleWithDisplay(ruby->styleRef(), INLINE_BLOCK);
    RenderBlockFlow* newBlock = RenderBlockFlow::createAnonymous(&ruby->document());
    newBlock->setStyle(newStyle.release());
    return newBlock;
}

static LayoutRubyRun* lastRubyRun(const LayoutObject* ruby)
{
    LayoutObject* child = ruby->slowLastChild();
    if (child && !child->isRubyRun())
        child = child->previousSibling();
    ASSERT(!child || child->isRubyRun() || child->isBeforeContent() || child == rubyBeforeBlock(ruby));
    return child && child->isRubyRun() ? toLayoutRubyRun(child) : 0;
}

static inline LayoutRubyRun* findRubyRunParent(LayoutObject* child)
{
    while (child && !child->isRubyRun())
        child = child->parent();
    return toLayoutRubyRun(child);
}

// === ruby as inline object ===

LayoutRubyAsInline::LayoutRubyAsInline(Element* element)
    : RenderInline(element)
{
    UseCounter::count(document(), UseCounter::RenderRuby);
}

LayoutRubyAsInline::~LayoutRubyAsInline()
{
}

void LayoutRubyAsInline::styleDidChange(StyleDifference diff, const LayoutStyle* oldStyle)
{
    RenderInline::styleDidChange(diff, oldStyle);
    propagateStyleToAnonymousChildren();
}

void LayoutRubyAsInline::addChild(LayoutObject* child, LayoutObject* beforeChild)
{
    // Insert :before and :after content before/after the LayoutRubyRun(s)
    if (child->isBeforeContent()) {
        if (child->isInline()) {
            // Add generated inline content normally
            RenderInline::addChild(child, firstChild());
        } else {
            // Wrap non-inline content with an anonymous inline-block.
            RenderBlock* beforeBlock = rubyBeforeBlock(this);
            if (!beforeBlock) {
                beforeBlock = createAnonymousRubyInlineBlock(this);
                RenderInline::addChild(beforeBlock, firstChild());
            }
            beforeBlock->addChild(child);
        }
        return;
    }
    if (child->isAfterContent()) {
        if (child->isInline()) {
            // Add generated inline content normally
            RenderInline::addChild(child);
        } else {
            // Wrap non-inline content with an anonymous inline-block.
            RenderBlock* afterBlock = rubyAfterBlock(this);
            if (!afterBlock) {
                afterBlock = createAnonymousRubyInlineBlock(this);
                RenderInline::addChild(afterBlock);
            }
            afterBlock->addChild(child);
        }
        return;
    }

    // If the child is a ruby run, just add it normally.
    if (child->isRubyRun()) {
        RenderInline::addChild(child, beforeChild);
        return;
    }

    if (beforeChild && !isAfterContent(beforeChild)) {
        // insert child into run
        LayoutObject* run = beforeChild;
        while (run && !run->isRubyRun())
            run = run->parent();
        if (run) {
            if (beforeChild == run)
                beforeChild = toLayoutRubyRun(beforeChild)->firstChild();
            ASSERT(!beforeChild || beforeChild->isDescendantOf(run));
            run->addChild(child, beforeChild);
            return;
        }
        ASSERT_NOT_REACHED(); // beforeChild should always have a run as parent!
        // Emergency fallback: fall through and just append.
    }

    // If the new child would be appended, try to add the child to the previous run
    // if possible, or create a new run otherwise.
    // (The LayoutRubyRun object will handle the details)
    LayoutRubyRun* lastRun = lastRubyRun(this);
    if (!lastRun || lastRun->hasRubyText()) {
        lastRun = LayoutRubyRun::staticCreateRubyRun(this);
        RenderInline::addChild(lastRun, beforeChild);
    }
    lastRun->addChild(child);
}

void LayoutRubyAsInline::removeChild(LayoutObject* child)
{
    // If the child's parent is *this (must be a ruby run or generated content or anonymous block),
    // just use the normal remove method.
    if (child->parent() == this) {
        ASSERT(child->isRubyRun() || child->isBeforeContent() || child->isAfterContent() || isAnonymousRubyInlineBlock(child));
        RenderInline::removeChild(child);
        return;
    }
    // If the child's parent is an anoymous block (must be generated :before/:after content)
    // just use the block's remove method.
    if (isAnonymousRubyInlineBlock(child->parent())) {
        ASSERT(child->isBeforeContent() || child->isAfterContent());
        child->parent()->removeChild(child);
        removeChild(child->parent());
        return;
    }

    // Otherwise find the containing run and remove it from there.
    LayoutRubyRun* run = findRubyRunParent(child);
    ASSERT(run);
    run->removeChild(child);
}

// === ruby as block object ===

LayoutRubyAsBlock::LayoutRubyAsBlock(Element* element)
    : RenderBlockFlow(element)
{
    UseCounter::count(document(), UseCounter::RenderRuby);
}

LayoutRubyAsBlock::~LayoutRubyAsBlock()
{
}

void LayoutRubyAsBlock::styleDidChange(StyleDifference diff, const LayoutStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);
    propagateStyleToAnonymousChildren();
}

void LayoutRubyAsBlock::addChild(LayoutObject* child, LayoutObject* beforeChild)
{
    // Insert :before and :after content before/after the LayoutRubyRun(s)
    if (child->isBeforeContent()) {
        if (child->isInline()) {
            // Add generated inline content normally
            RenderBlockFlow::addChild(child, firstChild());
        } else {
            // Wrap non-inline content with an anonymous inline-block.
            RenderBlock* beforeBlock = rubyBeforeBlock(this);
            if (!beforeBlock) {
                beforeBlock = createAnonymousRubyInlineBlock(this);
                RenderBlockFlow::addChild(beforeBlock, firstChild());
            }
            beforeBlock->addChild(child);
        }
        return;
    }
    if (child->isAfterContent()) {
        if (child->isInline()) {
            // Add generated inline content normally
            RenderBlockFlow::addChild(child);
        } else {
            // Wrap non-inline content with an anonymous inline-block.
            RenderBlock* afterBlock = rubyAfterBlock(this);
            if (!afterBlock) {
                afterBlock = createAnonymousRubyInlineBlock(this);
                RenderBlockFlow::addChild(afterBlock);
            }
            afterBlock->addChild(child);
        }
        return;
    }

    // If the child is a ruby run, just add it normally.
    if (child->isRubyRun()) {
        RenderBlockFlow::addChild(child, beforeChild);
        return;
    }

    if (beforeChild && !isAfterContent(beforeChild)) {
        // insert child into run
        LayoutObject* run = beforeChild;
        while (run && !run->isRubyRun())
            run = run->parent();
        if (run) {
            if (beforeChild == run)
                beforeChild = toLayoutRubyRun(beforeChild)->firstChild();
            ASSERT(!beforeChild || beforeChild->isDescendantOf(run));
            run->addChild(child, beforeChild);
            return;
        }
        ASSERT_NOT_REACHED(); // beforeChild should always have a run as parent!
        // Emergency fallback: fall through and just append.
    }

    // If the new child would be appended, try to add the child to the previous run
    // if possible, or create a new run otherwise.
    // (The LayoutRubyRun object will handle the details)
    LayoutRubyRun* lastRun = lastRubyRun(this);
    if (!lastRun || lastRun->hasRubyText()) {
        lastRun = LayoutRubyRun::staticCreateRubyRun(this);
        RenderBlockFlow::addChild(lastRun, beforeChild);
    }
    lastRun->addChild(child);
}

void LayoutRubyAsBlock::removeChild(LayoutObject* child)
{
    // If the child's parent is *this (must be a ruby run or generated content or anonymous block),
    // just use the normal remove method.
    if (child->parent() == this) {
        ASSERT(child->isRubyRun() || child->isBeforeContent() || child->isAfterContent() || isAnonymousRubyInlineBlock(child));
        RenderBlockFlow::removeChild(child);
        return;
    }
    // If the child's parent is an anoymous block (must be generated :before/:after content)
    // just use the block's remove method.
    if (isAnonymousRubyInlineBlock(child->parent())) {
        ASSERT(child->isBeforeContent() || child->isAfterContent());
        child->parent()->removeChild(child);
        removeChild(child->parent());
        return;
    }

    // Otherwise find the containing run and remove it from there.
    LayoutRubyRun* run = findRubyRunParent(child);
    ASSERT(run);
    run->removeChild(child);
}

} // namespace blink
