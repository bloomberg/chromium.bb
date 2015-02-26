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
        || (object->isAnonymous() && object->isLayoutBlock() && object->style()->display() == INLINE_BLOCK));

    return object
        && object->parent()->isRuby()
        && object->isLayoutBlock()
        && !object->isRubyRun();
}

static inline bool isRubyBeforeBlock(const LayoutObject* object)
{
    return isAnonymousRubyInlineBlock(object)
        && !object->previousSibling()
        && toLayoutBlock(object)->firstChild()
        && toLayoutBlock(object)->firstChild()->style()->styleType() == BEFORE;
}

static inline bool isRubyAfterBlock(const LayoutObject* object)
{
    return isAnonymousRubyInlineBlock(object)
        && !object->nextSibling()
        && toLayoutBlock(object)->firstChild()
        && toLayoutBlock(object)->firstChild()->style()->styleType() == AFTER;
}

static inline LayoutBlock* rubyBeforeBlock(const LayoutObject* ruby)
{
    LayoutObject* child = ruby->slowFirstChild();
    return isRubyBeforeBlock(child) ? toLayoutBlock(child) : 0;
}

static inline LayoutBlock* rubyAfterBlock(const LayoutObject* ruby)
{
    LayoutObject* child = ruby->slowLastChild();
    return isRubyAfterBlock(child) ? toLayoutBlock(child) : 0;
}

static LayoutBlockFlow* createAnonymousRubyInlineBlock(LayoutObject* ruby)
{
    RefPtr<LayoutStyle> newStyle = LayoutStyle::createAnonymousStyleWithDisplay(ruby->styleRef(), INLINE_BLOCK);
    LayoutBlockFlow* newBlock = LayoutBlockFlow::createAnonymous(&ruby->document());
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
    : LayoutInline(element)
{
    UseCounter::count(document(), UseCounter::RenderRuby);
}

LayoutRubyAsInline::~LayoutRubyAsInline()
{
}

void LayoutRubyAsInline::styleDidChange(StyleDifference diff, const LayoutStyle* oldStyle)
{
    LayoutInline::styleDidChange(diff, oldStyle);
    propagateStyleToAnonymousChildren();
}

void LayoutRubyAsInline::addChild(LayoutObject* child, LayoutObject* beforeChild)
{
    // Insert :before and :after content before/after the LayoutRubyRun(s)
    if (child->isBeforeContent()) {
        if (child->isInline()) {
            // Add generated inline content normally
            LayoutInline::addChild(child, firstChild());
        } else {
            // Wrap non-inline content with an anonymous inline-block.
            LayoutBlock* beforeBlock = rubyBeforeBlock(this);
            if (!beforeBlock) {
                beforeBlock = createAnonymousRubyInlineBlock(this);
                LayoutInline::addChild(beforeBlock, firstChild());
            }
            beforeBlock->addChild(child);
        }
        return;
    }
    if (child->isAfterContent()) {
        if (child->isInline()) {
            // Add generated inline content normally
            LayoutInline::addChild(child);
        } else {
            // Wrap non-inline content with an anonymous inline-block.
            LayoutBlock* afterBlock = rubyAfterBlock(this);
            if (!afterBlock) {
                afterBlock = createAnonymousRubyInlineBlock(this);
                LayoutInline::addChild(afterBlock);
            }
            afterBlock->addChild(child);
        }
        return;
    }

    // If the child is a ruby run, just add it normally.
    if (child->isRubyRun()) {
        LayoutInline::addChild(child, beforeChild);
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
        LayoutInline::addChild(lastRun, beforeChild);
    }
    lastRun->addChild(child);
}

void LayoutRubyAsInline::removeChild(LayoutObject* child)
{
    // If the child's parent is *this (must be a ruby run or generated content or anonymous block),
    // just use the normal remove method.
    if (child->parent() == this) {
        ASSERT(child->isRubyRun() || child->isBeforeContent() || child->isAfterContent() || isAnonymousRubyInlineBlock(child));
        LayoutInline::removeChild(child);
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
    : LayoutBlockFlow(element)
{
    UseCounter::count(document(), UseCounter::RenderRuby);
}

LayoutRubyAsBlock::~LayoutRubyAsBlock()
{
}

void LayoutRubyAsBlock::styleDidChange(StyleDifference diff, const LayoutStyle* oldStyle)
{
    LayoutBlockFlow::styleDidChange(diff, oldStyle);
    propagateStyleToAnonymousChildren();
}

void LayoutRubyAsBlock::addChild(LayoutObject* child, LayoutObject* beforeChild)
{
    // Insert :before and :after content before/after the LayoutRubyRun(s)
    if (child->isBeforeContent()) {
        if (child->isInline()) {
            // Add generated inline content normally
            LayoutBlockFlow::addChild(child, firstChild());
        } else {
            // Wrap non-inline content with an anonymous inline-block.
            LayoutBlock* beforeBlock = rubyBeforeBlock(this);
            if (!beforeBlock) {
                beforeBlock = createAnonymousRubyInlineBlock(this);
                LayoutBlockFlow::addChild(beforeBlock, firstChild());
            }
            beforeBlock->addChild(child);
        }
        return;
    }
    if (child->isAfterContent()) {
        if (child->isInline()) {
            // Add generated inline content normally
            LayoutBlockFlow::addChild(child);
        } else {
            // Wrap non-inline content with an anonymous inline-block.
            LayoutBlock* afterBlock = rubyAfterBlock(this);
            if (!afterBlock) {
                afterBlock = createAnonymousRubyInlineBlock(this);
                LayoutBlockFlow::addChild(afterBlock);
            }
            afterBlock->addChild(child);
        }
        return;
    }

    // If the child is a ruby run, just add it normally.
    if (child->isRubyRun()) {
        LayoutBlockFlow::addChild(child, beforeChild);
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
        LayoutBlockFlow::addChild(lastRun, beforeChild);
    }
    lastRun->addChild(child);
}

void LayoutRubyAsBlock::removeChild(LayoutObject* child)
{
    // If the child's parent is *this (must be a ruby run or generated content or anonymous block),
    // just use the normal remove method.
    if (child->parent() == this) {
        ASSERT(child->isRubyRun() || child->isBeforeContent() || child->isAfterContent() || isAnonymousRubyInlineBlock(child));
        LayoutBlockFlow::removeChild(child);
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
