// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_fragment.h"

#include "core/layout/ng/geometry/ng_border_edges.h"
#include "core/layout/ng/geometry/ng_box_strut.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/style/ComputedStyle.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {
namespace {

bool AppendFragmentOffsetAndSize(const NGPhysicalFragment* fragment,
                                 StringBuilder* builder,
                                 NGPhysicalFragment::DumpFlags flags,
                                 bool has_content) {
  if (flags & NGPhysicalFragment::DumpOffset) {
    if (has_content)
      builder->Append(" ");
    builder->Append("offset:");
    if (fragment->IsPlaced())
      builder->Append(fragment->Offset().ToString());
    else
      builder->Append("unplaced");
    has_content = true;
  }
  if (flags & NGPhysicalFragment::DumpSize) {
    if (has_content)
      builder->Append(" ");
    builder->Append("size:");
    builder->Append(fragment->Size().ToString());
    has_content = true;
  }
  return has_content;
}

void AppendFragmentToString(const NGPhysicalFragment* fragment,
                            StringBuilder* builder,
                            NGPhysicalFragment::DumpFlags flags,
                            unsigned indent = 2) {
  if (flags & NGPhysicalFragment::DumpIndentation) {
    for (unsigned i = 0; i < indent; i++)
      builder->Append(" ");
  }

  bool has_content = false;
  if (fragment->IsBox()) {
    if (flags & NGPhysicalFragment::DumpType) {
      builder->Append("Box");
      has_content = true;
    }
    has_content =
        AppendFragmentOffsetAndSize(fragment, builder, flags, has_content);

    const auto* box = ToNGPhysicalBoxFragment(fragment);
    if (flags & NGPhysicalFragment::DumpOverflow) {
      if (has_content)
        builder->Append(" ");
      builder->Append("overflow:");
      builder->Append(box->OverflowSize().ToString());
      has_content = true;
    }
    builder->Append("\n");

    if (flags & NGPhysicalFragment::DumpSubtree) {
      const auto& children = box->Children();
      for (unsigned i = 0; i < children.size(); i++)
        AppendFragmentToString(children[i].Get(), builder, flags, indent + 2);
    }
    return;
  }

  if (fragment->IsLineBox()) {
    if (flags & NGPhysicalFragment::DumpType) {
      builder->Append("LineBox");
      has_content = true;
    }
    has_content =
        AppendFragmentOffsetAndSize(fragment, builder, flags, has_content);
    builder->Append("\n");

    if (flags & NGPhysicalFragment::DumpSubtree) {
      const auto* line_box = ToNGPhysicalLineBoxFragment(fragment);
      const auto& children = line_box->Children();
      for (unsigned i = 0; i < children.size(); i++)
        AppendFragmentToString(children[i].Get(), builder, flags, indent + 2);
      return;
    }
  }

  if (fragment->IsText()) {
    if (flags & NGPhysicalFragment::DumpType) {
      builder->Append("Text");
      has_content = true;
    }
    has_content =
        AppendFragmentOffsetAndSize(fragment, builder, flags, has_content);

    if (flags & NGPhysicalFragment::DumpTextOffsets) {
      const auto* text = ToNGPhysicalTextFragment(fragment);
      if (has_content)
        builder->Append(" ");
      builder->Append("start: ");
      builder->Append(String::Format("%u", text->StartOffset()));
      builder->Append(" end: ");
      builder->Append(String::Format("%u", text->EndOffset()));
      has_content = true;
    }
    builder->Append("\n");
    return;
  }

  if (flags & NGPhysicalFragment::DumpType) {
    builder->Append("Unknown fragment type");
    has_content = true;
  }
  has_content =
      AppendFragmentOffsetAndSize(fragment, builder, flags, has_content);
  builder->Append("\n");
}

}  // namespace

NGPhysicalFragment::NGPhysicalFragment(LayoutObject* layout_object,
                                       const ComputedStyle& style,
                                       NGPhysicalSize size,
                                       NGFragmentType type,
                                       RefPtr<NGBreakToken> break_token)
    : layout_object_(layout_object),
      style_(&style),
      size_(size),
      break_token_(std::move(break_token)),
      type_(type),
      is_placed_(false) {}

// Keep the implementation of the destructor here, to avoid dependencies on
// ComputedStyle in the header file.
NGPhysicalFragment::~NGPhysicalFragment() {}

void NGPhysicalFragment::Destroy() const {
  switch (Type()) {
    case kFragmentBox:
      delete static_cast<const NGPhysicalBoxFragment*>(this);
      break;
    case kFragmentText:
      delete static_cast<const NGPhysicalTextFragment*>(this);
      break;
    case kFragmentLineBox:
      delete static_cast<const NGPhysicalLineBoxFragment*>(this);
      break;
    default:
      NOTREACHED();
      break;
  }
}

const ComputedStyle& NGPhysicalFragment::Style() const {
  DCHECK(style_);
  return *style_;
}

NGPixelSnappedPhysicalBoxStrut NGPhysicalFragment::BorderWidths() const {
  unsigned edges = BorderEdges();
  NGPhysicalBoxStrut box_strut(
      LayoutUnit((edges & NGBorderEdges::kTop) ? Style().BorderTopWidth()
                                               : .0f),
      LayoutUnit((edges & NGBorderEdges::kRight) ? Style().BorderRightWidth()
                                                 : .0f),
      LayoutUnit((edges & NGBorderEdges::kBottom) ? Style().BorderBottomWidth()
                                                  : .0f),
      LayoutUnit((edges & NGBorderEdges::kLeft) ? Style().BorderLeftWidth()
                                                : .0f));
  return box_strut.SnapToDevicePixels();
}

RefPtr<NGPhysicalFragment> NGPhysicalFragment::CloneWithoutOffset() const {
  switch (Type()) {
    case kFragmentBox:
      return static_cast<const NGPhysicalBoxFragment*>(this)
          ->CloneWithoutOffset();
      break;
    case kFragmentText:
      return static_cast<const NGPhysicalTextFragment*>(this)
          ->CloneWithoutOffset();
      break;
    case kFragmentLineBox:
      return static_cast<const NGPhysicalLineBoxFragment*>(this)
          ->CloneWithoutOffset();
      break;
    default:
      NOTREACHED();
      break;
  }
  return nullptr;
}

void NGPhysicalFragment::UpdateVisualRect() const {
  SetVisualRect({LayoutPoint(), LayoutSize(Size().width, Size().height)});
}

String NGPhysicalFragment::ToString() const {
  return String::Format("Type: '%d' Size: '%s' Offset: '%s' Placed: '%d'",
                        Type(), Size().ToString().Ascii().data(),
                        Offset().ToString().Ascii().data(), IsPlaced());
}

String NGPhysicalFragment::DumpFragmentTree(DumpFlags flags) const {
  StringBuilder string_builder;
  if (flags & DumpHeaderText)
    string_builder.Append(".:: LayoutNG Physical Fragment Tree ::.\n");
  AppendFragmentToString(this, &string_builder, flags);
  return string_builder.ToString();
}

#ifndef NDEBUG
void NGPhysicalFragment::ShowFragmentTree() const {
  fprintf(stderr, "%s\n", DumpFragmentTree(DumpAll).Utf8().data());
}
#endif  // !NDEBUG

}  // namespace blink
