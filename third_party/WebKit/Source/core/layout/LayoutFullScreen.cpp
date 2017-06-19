/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "core/layout/LayoutFullScreen.h"

#include "core/dom/Fullscreen.h"
#include "core/frame/VisualViewport.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/page/Page.h"

#include "public/platform/WebScreenInfo.h"

namespace blink {

namespace {

class LayoutFullScreenPlaceholder final : public LayoutBlockFlow {
 public:
  LayoutFullScreenPlaceholder(LayoutFullScreen* owner)
      : LayoutBlockFlow(nullptr), owner_(owner) {
    SetDocumentForAnonymous(&owner->GetDocument());
  }

  // Must call setStyleWithWritingModeOfParent() instead.
  void SetStyle(PassRefPtr<ComputedStyle>) = delete;

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutFullScreenPlaceholder ||
           LayoutBlockFlow::IsOfType(type);
  }
  bool AnonymousHasStylePropagationOverride() override { return true; }

  void WillBeDestroyed() override;
  LayoutFullScreen* owner_;
};

void LayoutFullScreenPlaceholder::WillBeDestroyed() {
  owner_->ResetPlaceholder();
  LayoutBlockFlow::WillBeDestroyed();
}

}  // namespace

LayoutFullScreen::LayoutFullScreen()
    : LayoutFlexibleBox(nullptr), placeholder_(nullptr) {
  SetIsAtomicInlineLevel(false);
}

LayoutFullScreen* LayoutFullScreen::CreateAnonymous(Document* document) {
  LayoutFullScreen* layout_object = new LayoutFullScreen();
  layout_object->SetDocumentForAnonymous(document);
  return layout_object;
}

void LayoutFullScreen::WillBeDestroyed() {
  if (placeholder_) {
    Remove();
    if (!placeholder_->BeingDestroyed())
      placeholder_->Destroy();
    DCHECK(!placeholder_);
  }

  // LayoutObjects are unretained, so notify the document (which holds a pointer
  // to a LayoutFullScreen) if its LayoutFullScreen is destroyed.
  Fullscreen& fullscreen = Fullscreen::From(GetDocument());
  if (fullscreen.FullScreenLayoutObject() == this)
    fullscreen.FullScreenLayoutObjectDestroyed();

  LayoutFlexibleBox::WillBeDestroyed();
}

void LayoutFullScreen::UpdateStyle(LayoutObject* parent) {
  RefPtr<ComputedStyle> fullscreen_style = ComputedStyle::Create();

  // Create a stacking context:
  fullscreen_style->SetZIndex(INT_MAX);
  fullscreen_style->SetIsStackingContext(true);

  fullscreen_style->SetFontDescription(FontDescription());
  fullscreen_style->GetFont().Update(nullptr);

  fullscreen_style->SetDisplay(EDisplay::kFlex);
  fullscreen_style->SetJustifyContentPosition(kContentPositionCenter);
  // TODO (lajava): Since the FullScrenn layout object is anonymous, its Default
  // Alignment (align-items) value can't be used to resolve its children Self
  // Alignment 'auto' values.
  fullscreen_style->SetAlignItemsPosition(kItemPositionCenter);
  fullscreen_style->SetFlexDirection(EFlexDirection::kColumn);

  fullscreen_style->SetPosition(EPosition::kFixed);
  fullscreen_style->SetLeft(Length(0, blink::kFixed));
  fullscreen_style->SetTop(Length(0, blink::kFixed));
  IntSize viewport_size = GetDocument().GetPage()->GetVisualViewport().Size();
  fullscreen_style->SetWidth(Length(viewport_size.Width(), blink::kFixed));
  fullscreen_style->SetHeight(Length(viewport_size.Height(), blink::kFixed));

  fullscreen_style->SetBackgroundColor(StyleColor(Color::kBlack));

  SetStyleWithWritingModeOf(fullscreen_style, parent);
}

void LayoutFullScreen::UpdateStyle() {
  UpdateStyle(Parent());
}

LayoutObject* LayoutFullScreen::WrapLayoutObject(LayoutObject* object,
                                                 LayoutObject* parent,
                                                 Document* document) {
  // FIXME: We should not modify the structure of the layout tree during
  // layout. crbug.com/370459
  DeprecatedDisableModifyLayoutTreeStructureAsserts disabler;

  LayoutFullScreen* fullscreen_layout_object =
      LayoutFullScreen::CreateAnonymous(document);
  fullscreen_layout_object->UpdateStyle(parent);
  if (parent && !parent->IsChildAllowed(fullscreen_layout_object,
                                        fullscreen_layout_object->StyleRef())) {
    fullscreen_layout_object->Destroy();
    return nullptr;
  }
  if (object) {
    // |object->parent()| can be null if the object is not yet attached
    // to |parent|.
    if (LayoutObject* parent = object->Parent()) {
      LayoutBlock* containing_block = object->ContainingBlock();
      DCHECK(containing_block);
      // Since we are moving the |object| to a new parent
      // |fullscreenLayoutObject|, the line box tree underneath our
      // |containingBlock| is not longer valid.
      if (containing_block->IsLayoutBlockFlow())
        ToLayoutBlockFlow(containing_block)->DeleteLineBoxTree();

      parent->AddChildWithWritingModeOfParent(fullscreen_layout_object, object);
      object->Remove();

      // Always just do a full layout to ensure that line boxes get deleted
      // properly.
      // Because objects moved from |parent| to |fullscreenLayoutObject|, we
      // want to make new line boxes instead of leaving the old ones around.
      parent->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
          LayoutInvalidationReason::kFullscreen);
      containing_block
          ->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
              LayoutInvalidationReason::kFullscreen);
    }
    fullscreen_layout_object->AddChild(object);
    fullscreen_layout_object
        ->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
            LayoutInvalidationReason::kFullscreen);
  }

  DCHECK(document);
  Fullscreen::From(*document).SetFullScreenLayoutObject(
      fullscreen_layout_object);
  return fullscreen_layout_object;
}

void LayoutFullScreen::UnwrapLayoutObject() {
  // FIXME: We should not modify the structure of the layout tree during
  // layout. crbug.com/370459
  DeprecatedDisableModifyLayoutTreeStructureAsserts disabler;

  if (Parent()) {
    for (LayoutObject* child = FirstChild(); child; child = FirstChild()) {
      // We have to clear the override size, because as a flexbox, we
      // may have set one on the child, and we don't want to leave that
      // lying around on the child.
      if (child->IsBox())
        ToLayoutBox(child)->ClearOverrideSize();
      child->Remove();
      Parent()->AddChild(child, this);
      Parent()->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
          LayoutInvalidationReason::kFullscreen);
    }
  }
  if (Placeholder())
    Placeholder()->Remove();
  Remove();
  Destroy();
}

void LayoutFullScreen::CreatePlaceholder(PassRefPtr<ComputedStyle> style,
                                         const LayoutRect& frame_rect) {
  if (style->Width().IsAuto())
    style->SetWidth(Length(frame_rect.Width(), kFixed));
  if (style->Height().IsAuto())
    style->SetHeight(Length(frame_rect.Height(), kFixed));

  if (!placeholder_) {
    placeholder_ = new LayoutFullScreenPlaceholder(this);
    placeholder_->SetStyleWithWritingModeOfParent(std::move(style));
    if (Parent()) {
      Parent()->AddChildWithWritingModeOfParent(placeholder_, this);
      Parent()->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
          LayoutInvalidationReason::kFullscreen);
    }
  } else {
    placeholder_->SetStyle(std::move(style));
    placeholder_->SetStyleWithWritingModeOfParent(std::move(style));
  }
}

}  // namespace blink
