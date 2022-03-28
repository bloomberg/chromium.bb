// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_highlight_overlay.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/platform/fonts/ng_text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

using HighlightLayerType = NGHighlightOverlay::HighlightLayerType;
using HighlightLayer = NGHighlightOverlay::HighlightLayer;
using HighlightEdge = NGHighlightOverlay::HighlightEdge;
using HighlightPart = NGHighlightOverlay::HighlightPart;

unsigned GetTextContentOffset(const Text& text, unsigned offset) {
  // TODO(yoichio): Sanitize DocumentMarker around text length.
  const Position position(text, std::min(offset, text.length()));
  const NGOffsetMapping* const offset_mapping =
      NGOffsetMapping::GetFor(position);
  DCHECK(offset_mapping);
  const absl::optional<unsigned>& ng_offset =
      offset_mapping->GetTextContentOffset(position);
  DCHECK(ng_offset.has_value());
  return ng_offset.value();
}

}  // namespace

String HighlightLayer::ToString() const {
  StringBuilder result{};
  switch (type) {
    case HighlightLayerType::kOriginating:
      result.Append("ORIG");
      break;
    case HighlightLayerType::kCustom:
      result.Append(name.GetString());
      break;
    case HighlightLayerType::kGrammar:
      result.Append("GRAM");
      break;
    case HighlightLayerType::kSpelling:
      result.Append("SPEL");
      break;
    case HighlightLayerType::kTargetText:
      result.Append("TARG");
      break;
    case HighlightLayerType::kSelection:
      result.Append("SELE");
      break;
    default:
      NOTREACHED();
  }
  return result.ToString();
}

enum PseudoId HighlightLayer::PseudoId() const {
  switch (type) {
    case HighlightLayerType::kOriginating:
      return kPseudoIdNone;
    case HighlightLayerType::kCustom:
      return kPseudoIdHighlight;
    case HighlightLayerType::kGrammar:
      return kPseudoIdGrammarError;
    case HighlightLayerType::kSpelling:
      return kPseudoIdSpellingError;
    case HighlightLayerType::kTargetText:
      return kPseudoIdTargetText;
    case HighlightLayerType::kSelection:
      return kPseudoIdSelection;
    default:
      NOTREACHED();
  }
}

const AtomicString& HighlightLayer::PseudoArgument() const {
  return name;
}

bool HighlightLayer::operator==(const HighlightLayer& other) const {
  return type == other.type && name == other.name;
}

bool HighlightLayer::operator!=(const HighlightLayer& other) const {
  return !operator==(other);
}

int8_t HighlightLayer::ComparePaintOrder(
    const HighlightLayer& other,
    const HighlightRegistry& registry) const {
  if (type < other.type) {
    return HighlightRegistry::OverlayStackingPosition::
        kOverlayStackingPositionBelow;
  }
  if (type > other.type) {
    return HighlightRegistry::OverlayStackingPosition::
        kOverlayStackingPositionAbove;
  }
  if (type != HighlightLayerType::kCustom) {
    return HighlightRegistry::OverlayStackingPosition::
        kOverlayStackingPositionEquivalent;
  }
  const HighlightRegistryMap& map = registry.GetHighlights();
  auto* this_entry = map.find(MakeGarbageCollected<HighlightRegistryMapEntry>(
                                  PseudoArgument()))
                         ->Get();
  auto* other_entry = map.find(MakeGarbageCollected<HighlightRegistryMapEntry>(
                                   other.PseudoArgument()))
                          ->Get();
  return registry.CompareOverlayStackingPosition(
      PseudoArgument(), this_entry->highlight, other.PseudoArgument(),
      other_entry->highlight);
}

String HighlightEdge::ToString() const {
  StringBuilder result{};
  result.AppendNumber(offset);
  result.Append(type == HighlightEdgeType::kStart ? "<" : ">");
  result.Append(layer.ToString());
  return result.ToString();
}

bool HighlightEdge::LessThan(const HighlightEdge& other,
                             const HighlightRegistry& registry) const {
  if (offset < other.offset)
    return true;
  if (offset > other.offset)
    return false;
  if (type > other.type)
    return true;
  if (type < other.type)
    return false;
  return layer.ComparePaintOrder(other.layer, registry) < 0;
}

bool HighlightEdge::operator==(const HighlightEdge& other) const {
  return offset == other.offset && type == other.type && layer == other.layer;
}

bool HighlightEdge::operator!=(const HighlightEdge& other) const {
  return !operator==(other);
}

HighlightPart::HighlightPart(HighlightLayer layer,
                             unsigned from,
                             unsigned to,
                             Vector<HighlightLayer> decorations)
    : layer(layer), from(from), to(to), decorations(decorations) {
  DCHECK_LT(from, to);
}

HighlightPart::HighlightPart(HighlightLayer layer, unsigned from, unsigned to)
    : HighlightPart(layer, from, to, Vector<HighlightLayer>{}) {}

String HighlightPart::ToString() const {
  StringBuilder result{};
  result.Append(layer.ToString());
  result.Append("[");
  result.AppendNumber(from);
  result.Append(",");
  result.AppendNumber(to);
  result.Append(")");
  for (const HighlightLayer& layer : decorations) {
    result.Append("+");
    result.Append(layer.ToString());
  }
  return result.ToString();
}

bool HighlightPart::operator==(const HighlightPart& other) const {
  return layer == other.layer && from == other.from && to == other.to &&
         decorations == other.decorations;
}

bool HighlightPart::operator!=(const HighlightPart& other) const {
  return !operator==(other);
}

Vector<HighlightLayer> NGHighlightOverlay::ComputeLayers(
    const HighlightRegistry* registry,
    const NGTextFragmentPaintInfo& originating,
    const LayoutSelectionStatus* selection,
    const DocumentMarkerVector& custom,
    const DocumentMarkerVector& grammar,
    const DocumentMarkerVector& spelling,
    const DocumentMarkerVector& target) {
  DCHECK(RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled());

  Vector<HighlightLayer> result{};
  result.emplace_back(HighlightLayerType::kOriginating);

  for (const auto& marker : custom) {
    auto* custom = To<CustomHighlightMarker>(marker.Get());
    HighlightLayer layer{HighlightLayerType::kCustom,
                         custom->GetHighlightName()};
    if (!result.Contains(layer))
      result.push_back(layer);
  }
  if (!grammar.IsEmpty())
    result.emplace_back(HighlightLayerType::kGrammar);
  if (!spelling.IsEmpty())
    result.emplace_back(HighlightLayerType::kSpelling);
  if (!target.IsEmpty())
    result.emplace_back(HighlightLayerType::kTargetText);
  if (selection)
    result.emplace_back(HighlightLayerType::kSelection);

  std::sort(result.begin(), result.end(),
            [registry](const HighlightLayer& p, const HighlightLayer& q) {
              return p.ComparePaintOrder(q, *registry) < 0;
            });

  return result;
}

Vector<HighlightEdge> NGHighlightOverlay::ComputeEdges(
    const Node* node,
    const HighlightRegistry* registry,
    const NGTextFragmentPaintInfo& originating,
    const LayoutSelectionStatus* selection,
    const DocumentMarkerVector& custom,
    const DocumentMarkerVector& grammar,
    const DocumentMarkerVector& spelling,
    const DocumentMarkerVector& target) {
  DCHECK(RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled());

  Vector<HighlightEdge> result{};
  DCHECK_LT(originating.from, originating.to);
  result.emplace_back(originating.from,
                      HighlightLayer{HighlightLayerType::kOriginating},
                      HighlightEdgeType::kStart);
  result.emplace_back(originating.to,
                      HighlightLayer{HighlightLayerType::kOriginating},
                      HighlightEdgeType::kEnd);

  // |node| might not be a Text node (e.g. <br>), or it might be nullptr (e.g.
  // ::first-letter). In both cases, we should still try to paint kOriginating
  // and kSelection if necessary, but we can’t paint marker-based highlights,
  // because GetTextContentOffset requires a Text node. Markers are defined and
  // stored in terms of Text nodes anyway, so this check should never fail.
  const auto* text_node = DynamicTo<Text>(node);
  if (!text_node) {
    DCHECK(custom.IsEmpty() && grammar.IsEmpty() && spelling.IsEmpty() &&
           target.IsEmpty())
        << "markers can not be painted without a valid Text node";
  }

  for (const auto& marker : custom) {
    auto* custom = To<CustomHighlightMarker>(marker.Get());
    unsigned content_start =
        GetTextContentOffset(*text_node, marker->StartOffset());
    unsigned content_end =
        GetTextContentOffset(*text_node, marker->EndOffset());
    if (content_start >= content_end)
      continue;
    result.emplace_back(
        content_start,
        HighlightLayer{HighlightLayerType::kCustom, custom->GetHighlightName()},
        HighlightEdgeType::kStart);
    result.emplace_back(
        content_end,
        HighlightLayer{HighlightLayerType::kCustom, custom->GetHighlightName()},
        HighlightEdgeType::kEnd);
  }
  for (const auto& marker : grammar) {
    unsigned content_start =
        GetTextContentOffset(*text_node, marker->StartOffset());
    unsigned content_end =
        GetTextContentOffset(*text_node, marker->EndOffset());
    if (content_start >= content_end)
      continue;
    result.emplace_back(content_start,
                        HighlightLayer{HighlightLayerType::kGrammar},
                        HighlightEdgeType::kStart);
    result.emplace_back(content_end,
                        HighlightLayer{HighlightLayerType::kGrammar},
                        HighlightEdgeType::kEnd);
  }
  for (const auto& marker : spelling) {
    unsigned content_start =
        GetTextContentOffset(*text_node, marker->StartOffset());
    unsigned content_end =
        GetTextContentOffset(*text_node, marker->EndOffset());
    if (content_start >= content_end)
      continue;
    result.emplace_back(content_start,
                        HighlightLayer{HighlightLayerType::kSpelling},
                        HighlightEdgeType::kStart);
    result.emplace_back(content_end,
                        HighlightLayer{HighlightLayerType::kSpelling},
                        HighlightEdgeType::kEnd);
  }
  for (const auto& marker : target) {
    unsigned content_start =
        GetTextContentOffset(*text_node, marker->StartOffset());
    unsigned content_end =
        GetTextContentOffset(*text_node, marker->EndOffset());
    if (content_start >= content_end)
      continue;
    result.emplace_back(content_start,
                        HighlightLayer{HighlightLayerType::kTargetText},
                        HighlightEdgeType::kStart);
    result.emplace_back(content_end,
                        HighlightLayer{HighlightLayerType::kTargetText},
                        HighlightEdgeType::kEnd);
  }
  if (selection) {
    DCHECK_LT(selection->start, selection->end);
    result.emplace_back(selection->start,
                        HighlightLayer{HighlightLayerType::kSelection},
                        HighlightEdgeType::kStart);
    result.emplace_back(selection->end,
                        HighlightLayer{HighlightLayerType::kSelection},
                        HighlightEdgeType::kEnd);
  }

  std::sort(result.begin(), result.end(),
            [registry](const HighlightEdge& p, const HighlightEdge& q) {
              return p.LessThan(q, *registry);
            });

  return result;
}

Vector<HighlightPart> NGHighlightOverlay::ComputeParts(
    const Vector<HighlightLayer>& layers,
    const Vector<HighlightEdge>& edges) {
  DCHECK(RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled());
  Vector<HighlightPart> result{};
  Vector<bool> active(layers.size());
  absl::optional<unsigned> prev_offset{};
  for (const HighlightEdge& edge : edges) {
    // If there is actually some text between the previous and current edges...
    if (prev_offset.has_value() && *prev_offset < edge.offset) {
      // ...and there is at least one active layer over that range...
      wtf_size_t topmost_active_index = active.ReverseFind(true);
      if (topmost_active_index != kNotFound) {
        // ...then enqueue this part of the text to be painted in that layer.
        HighlightPart part{layers[topmost_active_index], *prev_offset,
                           edge.offset};
        for (wtf_size_t i = 0; i < layers.size(); i++) {
          if (active[i])
            part.decorations.push_back(layers[i]);
        }
        result.push_back(part);
      }
    }
    wtf_size_t edge_layer_index = layers.Find(edge.layer);
    DCHECK_NE(edge_layer_index, kNotFound)
        << "edge layer should be one of the given layers";
    // TODO(crbug.com/1147859) this will crash if there are overlapping ranges
    // for a given highlight layer (see logic in old ComputeMarkersToPaint)
    DCHECK(active[edge_layer_index] ? edge.type == HighlightEdgeType::kEnd
                                    : edge.type == HighlightEdgeType::kStart)
        << "edge should be kStart iff the layer is active or else kEnd";
    active[edge_layer_index] = edge.type == HighlightEdgeType::kStart;
    prev_offset.emplace(edge.offset);
  }
  return result;
}

std::ostream& operator<<(std::ostream& result,
                         const NGHighlightOverlay::HighlightLayer& layer) {
  return result << layer.ToString().Utf8();
}

std::ostream& operator<<(std::ostream& result,
                         const NGHighlightOverlay::HighlightEdge& edge) {
  return result << edge.ToString().Utf8();
}

std::ostream& operator<<(std::ostream& result,
                         const NGHighlightOverlay::HighlightPart& part) {
  return result << part.ToString().Utf8();
}

}  // namespace blink
