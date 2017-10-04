/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/layout/svg/SVGResources.h"

#include <memory>
#include "core/layout/svg/LayoutSVGResourceClipper.h"
#include "core/layout/svg/LayoutSVGResourceFilter.h"
#include "core/layout/svg/LayoutSVGResourceMarker.h"
#include "core/layout/svg/LayoutSVGResourceMasker.h"
#include "core/layout/svg/LayoutSVGResourcePaintServer.h"
#include "core/style/ComputedStyle.h"
#include "core/svg/SVGGradientElement.h"
#include "core/svg/SVGPatternElement.h"
#include "core/svg/SVGTreeScopeResources.h"
#include "core/svg/SVGURIReference.h"
#include "core/svg_names.h"
#include "platform/wtf/PtrUtil.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

using namespace SVGNames;

SVGResources::SVGResources() : linked_resource_(nullptr) {}

static HashSet<AtomicString>& ClipperFilterMaskerTags() {
  DEFINE_STATIC_LOCAL(
      HashSet<AtomicString>, tag_list,
      ({
          // "container elements":
          // http://www.w3.org/TR/SVG11/intro.html#TermContainerElement
          // "graphics elements" :
          // http://www.w3.org/TR/SVG11/intro.html#TermGraphicsElement
          aTag.LocalName(), circleTag.LocalName(), ellipseTag.LocalName(),
          gTag.LocalName(), imageTag.LocalName(), lineTag.LocalName(),
          markerTag.LocalName(), maskTag.LocalName(), pathTag.LocalName(),
          polygonTag.LocalName(), polylineTag.LocalName(), rectTag.LocalName(),
          svgTag.LocalName(), textTag.LocalName(), useTag.LocalName(),
          // Not listed in the definitions is the clipPath element, the SVG spec
          // says though:
          // The "clipPath" element or any of its children can specify property
          // "clip-path".
          // So we have to add clipPathTag here, otherwhise clip-path on
          // clipPath will fail. (Already mailed SVG WG, waiting for a solution)
          clipPathTag.LocalName(),
          // Not listed in the definitions are the text content elements, though
          // filter/clipper/masker on tspan/text/.. is allowed.
          // (Already mailed SVG WG, waiting for a solution)
          textPathTag.LocalName(), tspanTag.LocalName(),
          // Not listed in the definitions is the foreignObject element, but
          // clip-path is a supported attribute.
          foreignObjectTag.LocalName(),
          // Elements that we ignore, as it doesn't make any sense.
          // defs, pattern, switch (FIXME: Mail SVG WG about these)
          // symbol (is converted to a svg element, when referenced by use, we
          // can safely ignore it.)
      }));
  return tag_list;
}

bool SVGResources::SupportsMarkers(const SVGElement& element) {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, tag_list,
                      ({
                          lineTag.LocalName(), pathTag.LocalName(),
                          polygonTag.LocalName(), polylineTag.LocalName(),
                      }));
  return tag_list.Contains(element.localName());
}

static HashSet<AtomicString>& FillAndStrokeTags() {
  DEFINE_STATIC_LOCAL(
      HashSet<AtomicString>, tag_list,
      ({
          circleTag.LocalName(), ellipseTag.LocalName(), lineTag.LocalName(),
          pathTag.LocalName(), polygonTag.LocalName(), polylineTag.LocalName(),
          rectTag.LocalName(), textTag.LocalName(), textPathTag.LocalName(),
          tspanTag.LocalName(),
      }));
  return tag_list;
}

static HashSet<AtomicString>& ChainableResourceTags() {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, tag_list,
                      ({
                          linearGradientTag.LocalName(), patternTag.LocalName(),
                          radialGradientTag.LocalName(),
                      }));
  return tag_list;
}

static inline AtomicString TargetReferenceFromResource(SVGElement& element) {
  String target;
  if (auto* pattern = ToSVGPatternElementOrNull(element))
    target = pattern->href()->CurrentValue()->Value();
  else if (auto* gradient = ToSVGGradientElementOrNull(element))
    target = gradient->href()->CurrentValue()->Value();
  else
    NOTREACHED();

  return SVGURIReference::FragmentIdentifierFromIRIString(
      target, element.GetTreeScope());
}

static inline bool SvgPaintTypeHasURL(SVGPaintType paint_type) {
  switch (paint_type) {
    case SVG_PAINTTYPE_URI_NONE:
    case SVG_PAINTTYPE_URI_CURRENTCOLOR:
    case SVG_PAINTTYPE_URI_RGBCOLOR:
    case SVG_PAINTTYPE_URI:
      return true;
    default:
      break;
  }
  return false;
}

namespace {

template <typename ContainerType>
bool IsResourceOfType(LayoutSVGResourceContainer* container) {
  return container->ResourceType() == ContainerType::kResourceType;
}

template <>
bool IsResourceOfType<LayoutSVGResourcePaintServer>(
    LayoutSVGResourceContainer* container) {
  return container->IsSVGPaintServer();
}

template <>
bool IsResourceOfType<LayoutSVGResourceContainer>(
    LayoutSVGResourceContainer* container) {
  return true;
}

template <typename ContainerType>
ContainerType* AttachToResource(SVGTreeScopeResources& tree_scope_resources,
                                const AtomicString& id,
                                SVGElement& element) {
  if (LayoutSVGResourceContainer* container =
          tree_scope_resources.ResourceById(id)) {
    if (IsResourceOfType<ContainerType>(container))
      return static_cast<ContainerType*>(container);
  }
  tree_scope_resources.AddPendingResource(id, element);
  return nullptr;
}
}

bool SVGResources::HasResourceData() const {
  return clipper_filter_masker_data_ || marker_data_ || fill_stroke_data_ ||
         linked_resource_;
}

static inline SVGResources& EnsureResources(
    std::unique_ptr<SVGResources>& resources) {
  if (!resources)
    resources = WTF::WrapUnique(new SVGResources);

  return *resources.get();
}

std::unique_ptr<SVGResources> SVGResources::BuildResources(
    const LayoutObject* object,
    const ComputedStyle& computed_style) {
  DCHECK(object);

  Node* node = object->GetNode();
  DCHECK(node);
  SECURITY_DCHECK(node->IsSVGElement());

  SVGElement& element = ToSVGElement(*node);

  const AtomicString& tag_name = element.localName();
  DCHECK(!tag_name.IsNull());

  TreeScope& tree_scope = element.TreeScopeForIdResolution();
  SVGTreeScopeResources& tree_scope_resources =
      tree_scope.EnsureSVGTreeScopedResources();

  const SVGComputedStyle& style = computed_style.SvgStyle();

  std::unique_ptr<SVGResources> resources;
  if (ClipperFilterMaskerTags().Contains(tag_name)) {
    if (computed_style.ClipPath() && !object->IsSVGRoot()) {
      ClipPathOperation* clip_path_operation = computed_style.ClipPath();
      if (clip_path_operation->GetType() == ClipPathOperation::REFERENCE) {
        const ReferenceClipPathOperation& clip_path_reference =
            ToReferenceClipPathOperation(*clip_path_operation);
        AtomicString id = SVGURIReference::FragmentIdentifierFromIRIString(
            clip_path_reference.Url(), tree_scope);
        EnsureResources(resources).SetClipper(
            AttachToResource<LayoutSVGResourceClipper>(tree_scope_resources, id,
                                                       element));
      }
    }

    if (computed_style.HasFilter() && !object->IsSVGRoot()) {
      const FilterOperations& filter_operations = computed_style.Filter();
      if (filter_operations.size() == 1) {
        const FilterOperation& filter_operation = *filter_operations.at(0);
        if (filter_operation.GetType() == FilterOperation::REFERENCE) {
          const auto& reference_filter_operation =
              ToReferenceFilterOperation(filter_operation);
          AtomicString id = SVGURIReference::FragmentIdentifierFromIRIString(
              reference_filter_operation.Url(), tree_scope);
          EnsureResources(resources).SetFilter(
              AttachToResource<LayoutSVGResourceFilter>(tree_scope_resources,
                                                        id, element));
        }
      }
    }

    if (style.HasMasker()) {
      EnsureResources(resources).SetMasker(
          AttachToResource<LayoutSVGResourceMasker>(
              tree_scope_resources, style.MaskerResource(), element));
    }
  }

  if (style.HasMarkers() && SupportsMarkers(element)) {
    EnsureResources(resources).SetMarkerStart(
        AttachToResource<LayoutSVGResourceMarker>(
            tree_scope_resources, style.MarkerStartResource(), element));
    EnsureResources(resources).SetMarkerMid(
        AttachToResource<LayoutSVGResourceMarker>(
            tree_scope_resources, style.MarkerMidResource(), element));
    EnsureResources(resources).SetMarkerEnd(
        AttachToResource<LayoutSVGResourceMarker>(
            tree_scope_resources, style.MarkerEndResource(), element));
  }

  if (FillAndStrokeTags().Contains(tag_name)) {
    if (style.HasFill() && SvgPaintTypeHasURL(style.FillPaintType())) {
      AtomicString id = SVGURIReference::FragmentIdentifierFromIRIString(
          style.FillPaintUri(), tree_scope);
      EnsureResources(resources).SetFill(
          AttachToResource<LayoutSVGResourcePaintServer>(tree_scope_resources,
                                                         id, element));
    }

    if (style.HasStroke() && SvgPaintTypeHasURL(style.StrokePaintType())) {
      AtomicString id = SVGURIReference::FragmentIdentifierFromIRIString(
          style.StrokePaintUri(), tree_scope);
      EnsureResources(resources).SetStroke(
          AttachToResource<LayoutSVGResourcePaintServer>(tree_scope_resources,
                                                         id, element));
    }
  }

  if (ChainableResourceTags().Contains(tag_name)) {
    AtomicString id = TargetReferenceFromResource(element);
    EnsureResources(resources).SetLinkedResource(
        AttachToResource<LayoutSVGResourceContainer>(tree_scope_resources, id,
                                                     element));
  }

  return (!resources || !resources->HasResourceData()) ? nullptr
                                                       : std::move(resources);
}

void SVGResources::LayoutIfNeeded() {
  if (clipper_filter_masker_data_) {
    if (LayoutSVGResourceClipper* clipper =
            clipper_filter_masker_data_->clipper)
      clipper->LayoutIfNeeded();
    if (LayoutSVGResourceMasker* masker = clipper_filter_masker_data_->masker)
      masker->LayoutIfNeeded();
    if (LayoutSVGResourceFilter* filter = clipper_filter_masker_data_->filter)
      filter->LayoutIfNeeded();
  }

  if (marker_data_) {
    if (LayoutSVGResourceMarker* marker = marker_data_->marker_start)
      marker->LayoutIfNeeded();
    if (LayoutSVGResourceMarker* marker = marker_data_->marker_mid)
      marker->LayoutIfNeeded();
    if (LayoutSVGResourceMarker* marker = marker_data_->marker_end)
      marker->LayoutIfNeeded();
  }

  if (fill_stroke_data_) {
    if (LayoutSVGResourcePaintServer* fill = fill_stroke_data_->fill)
      fill->LayoutIfNeeded();
    if (LayoutSVGResourcePaintServer* stroke = fill_stroke_data_->stroke)
      stroke->LayoutIfNeeded();
  }

  if (linked_resource_)
    linked_resource_->LayoutIfNeeded();
}

void SVGResources::RemoveClientFromCacheAffectingObjectBounds(
    LayoutObject* object,
    bool mark_for_invalidation) const {
  if (!clipper_filter_masker_data_)
    return;
  if (LayoutSVGResourceClipper* clipper = clipper_filter_masker_data_->clipper)
    clipper->RemoveClientFromCache(object, mark_for_invalidation);
  if (LayoutSVGResourceFilter* filter = clipper_filter_masker_data_->filter)
    filter->RemoveClientFromCache(object, mark_for_invalidation);
  if (LayoutSVGResourceMasker* masker = clipper_filter_masker_data_->masker)
    masker->RemoveClientFromCache(object, mark_for_invalidation);
}

void SVGResources::RemoveClientFromCache(LayoutObject* object,
                                         bool mark_for_invalidation) const {
  if (!HasResourceData())
    return;

  if (linked_resource_) {
    DCHECK(!clipper_filter_masker_data_);
    DCHECK(!marker_data_);
    DCHECK(!fill_stroke_data_);
    linked_resource_->RemoveClientFromCache(object, mark_for_invalidation);
    return;
  }

  RemoveClientFromCacheAffectingObjectBounds(object, mark_for_invalidation);

  if (marker_data_) {
    if (marker_data_->marker_start)
      marker_data_->marker_start->RemoveClientFromCache(object,
                                                        mark_for_invalidation);
    if (marker_data_->marker_mid)
      marker_data_->marker_mid->RemoveClientFromCache(object,
                                                      mark_for_invalidation);
    if (marker_data_->marker_end)
      marker_data_->marker_end->RemoveClientFromCache(object,
                                                      mark_for_invalidation);
  }

  if (fill_stroke_data_) {
    if (fill_stroke_data_->fill)
      fill_stroke_data_->fill->RemoveClientFromCache(object,
                                                     mark_for_invalidation);
    if (fill_stroke_data_->stroke)
      fill_stroke_data_->stroke->RemoveClientFromCache(object,
                                                       mark_for_invalidation);
  }
}

void SVGResources::ResourceDestroyed(LayoutSVGResourceContainer* resource) {
  DCHECK(resource);
  if (!HasResourceData())
    return;

  if (linked_resource_ == resource) {
    DCHECK(!clipper_filter_masker_data_);
    DCHECK(!marker_data_);
    DCHECK(!fill_stroke_data_);
    linked_resource_->RemoveAllClientsFromCache();
    linked_resource_ = nullptr;
    return;
  }

  switch (resource->ResourceType()) {
    case kMaskerResourceType:
      if (!clipper_filter_masker_data_)
        break;
      if (clipper_filter_masker_data_->masker == resource)
        clipper_filter_masker_data_->masker = nullptr;
      break;
    case kMarkerResourceType:
      if (!marker_data_)
        break;
      if (marker_data_->marker_start == resource)
        marker_data_->marker_start = nullptr;
      if (marker_data_->marker_mid == resource)
        marker_data_->marker_mid = nullptr;
      if (marker_data_->marker_end == resource)
        marker_data_->marker_end = nullptr;
      break;
    case kPatternResourceType:
    case kLinearGradientResourceType:
    case kRadialGradientResourceType:
      if (!fill_stroke_data_)
        break;
      if (fill_stroke_data_->fill == resource)
        fill_stroke_data_->fill = nullptr;
      if (fill_stroke_data_->stroke == resource)
        fill_stroke_data_->stroke = nullptr;
      break;
    case kFilterResourceType:
      if (!clipper_filter_masker_data_)
        break;
      if (clipper_filter_masker_data_->filter == resource)
        clipper_filter_masker_data_->filter = nullptr;
      break;
    case kClipperResourceType:
      if (!clipper_filter_masker_data_)
        break;
      if (clipper_filter_masker_data_->clipper == resource)
        clipper_filter_masker_data_->clipper = nullptr;
      break;
    default:
      NOTREACHED();
  }
}

void SVGResources::ClearReferencesTo(LayoutSVGResourceContainer* resource) {
  DCHECK(resource);
  if (linked_resource_ == resource) {
    DCHECK(!clipper_filter_masker_data_);
    DCHECK(!marker_data_);
    DCHECK(!fill_stroke_data_);
    linked_resource_ = nullptr;
    return;
  }

  switch (resource->ResourceType()) {
    case kMaskerResourceType:
      DCHECK(clipper_filter_masker_data_);
      DCHECK_EQ(clipper_filter_masker_data_->masker, resource);
      clipper_filter_masker_data_->masker = nullptr;
      break;
    case kMarkerResourceType:
      DCHECK(marker_data_);
      DCHECK(resource == MarkerStart() || resource == MarkerMid() ||
             resource == MarkerEnd());
      if (marker_data_->marker_start == resource)
        marker_data_->marker_start = nullptr;
      if (marker_data_->marker_mid == resource)
        marker_data_->marker_mid = nullptr;
      if (marker_data_->marker_end == resource)
        marker_data_->marker_end = nullptr;
      break;
    case kPatternResourceType:
    case kLinearGradientResourceType:
    case kRadialGradientResourceType:
      DCHECK(fill_stroke_data_);
      DCHECK(resource == Fill() || resource == Stroke());
      if (fill_stroke_data_->fill == resource)
        fill_stroke_data_->fill = nullptr;
      if (fill_stroke_data_->stroke == resource)
        fill_stroke_data_->stroke = nullptr;
      break;
    case kFilterResourceType:
      DCHECK(clipper_filter_masker_data_);
      DCHECK_EQ(clipper_filter_masker_data_->filter, resource);
      clipper_filter_masker_data_->filter = nullptr;
      break;
    case kClipperResourceType:
      DCHECK(clipper_filter_masker_data_);
      DCHECK_EQ(clipper_filter_masker_data_->clipper, resource);
      clipper_filter_masker_data_->clipper = nullptr;
      break;
    default:
      NOTREACHED();
  }
}

void SVGResources::BuildSetOfResources(
    HashSet<LayoutSVGResourceContainer*>& set) {
  if (!HasResourceData())
    return;

  if (linked_resource_) {
    DCHECK(!clipper_filter_masker_data_);
    DCHECK(!marker_data_);
    DCHECK(!fill_stroke_data_);
    set.insert(linked_resource_);
    return;
  }

  if (clipper_filter_masker_data_) {
    if (clipper_filter_masker_data_->clipper)
      set.insert(clipper_filter_masker_data_->clipper);
    if (clipper_filter_masker_data_->filter)
      set.insert(clipper_filter_masker_data_->filter);
    if (clipper_filter_masker_data_->masker)
      set.insert(clipper_filter_masker_data_->masker);
  }

  if (marker_data_) {
    if (marker_data_->marker_start)
      set.insert(marker_data_->marker_start);
    if (marker_data_->marker_mid)
      set.insert(marker_data_->marker_mid);
    if (marker_data_->marker_end)
      set.insert(marker_data_->marker_end);
  }

  if (fill_stroke_data_) {
    if (fill_stroke_data_->fill)
      set.insert(fill_stroke_data_->fill);
    if (fill_stroke_data_->stroke)
      set.insert(fill_stroke_data_->stroke);
  }
}

void SVGResources::SetClipper(LayoutSVGResourceClipper* clipper) {
  if (!clipper)
    return;

  DCHECK_EQ(clipper->ResourceType(), kClipperResourceType);

  if (!clipper_filter_masker_data_)
    clipper_filter_masker_data_ = ClipperFilterMaskerData::Create();

  clipper_filter_masker_data_->clipper = clipper;
}

void SVGResources::SetFilter(LayoutSVGResourceFilter* filter) {
  if (!filter)
    return;

  DCHECK_EQ(filter->ResourceType(), kFilterResourceType);

  if (!clipper_filter_masker_data_)
    clipper_filter_masker_data_ = ClipperFilterMaskerData::Create();

  clipper_filter_masker_data_->filter = filter;
}

void SVGResources::SetMarkerStart(LayoutSVGResourceMarker* marker_start) {
  if (!marker_start)
    return;

  DCHECK_EQ(marker_start->ResourceType(), kMarkerResourceType);

  if (!marker_data_)
    marker_data_ = MarkerData::Create();

  marker_data_->marker_start = marker_start;
}

void SVGResources::SetMarkerMid(LayoutSVGResourceMarker* marker_mid) {
  if (!marker_mid)
    return;

  DCHECK_EQ(marker_mid->ResourceType(), kMarkerResourceType);

  if (!marker_data_)
    marker_data_ = MarkerData::Create();

  marker_data_->marker_mid = marker_mid;
}

void SVGResources::SetMarkerEnd(LayoutSVGResourceMarker* marker_end) {
  if (!marker_end)
    return;

  DCHECK_EQ(marker_end->ResourceType(), kMarkerResourceType);

  if (!marker_data_)
    marker_data_ = MarkerData::Create();

  marker_data_->marker_end = marker_end;
}

void SVGResources::SetMasker(LayoutSVGResourceMasker* masker) {
  if (!masker)
    return;

  DCHECK_EQ(masker->ResourceType(), kMaskerResourceType);

  if (!clipper_filter_masker_data_)
    clipper_filter_masker_data_ = ClipperFilterMaskerData::Create();

  clipper_filter_masker_data_->masker = masker;
}

void SVGResources::SetFill(LayoutSVGResourcePaintServer* fill) {
  if (!fill)
    return;

  if (!fill_stroke_data_)
    fill_stroke_data_ = FillStrokeData::Create();

  fill_stroke_data_->fill = fill;
}

void SVGResources::SetStroke(LayoutSVGResourcePaintServer* stroke) {
  if (!stroke)
    return;

  if (!fill_stroke_data_)
    fill_stroke_data_ = FillStrokeData::Create();

  fill_stroke_data_->stroke = stroke;
}

void SVGResources::SetLinkedResource(
    LayoutSVGResourceContainer* linked_resource) {
  if (!linked_resource)
    return;

  linked_resource_ = linked_resource;
}

#ifndef NDEBUG
void SVGResources::Dump(const LayoutObject* object) {
  DCHECK(object);
  DCHECK(object->GetNode());

  fprintf(stderr, "-> this=%p, SVGResources(layoutObject=%p, node=%p)\n", this,
          object, object->GetNode());
  fprintf(stderr, " | DOM Tree:\n");
  fprintf(stderr, "%s", object->GetNode()->ToTreeStringForThis().Utf8().data());

  fprintf(stderr, "\n | List of resources:\n");
  if (clipper_filter_masker_data_) {
    if (LayoutSVGResourceClipper* clipper =
            clipper_filter_masker_data_->clipper)
      fprintf(stderr, " |-> Clipper    : %p (node=%p)\n", clipper,
              clipper->GetElement());
    if (LayoutSVGResourceFilter* filter = clipper_filter_masker_data_->filter)
      fprintf(stderr, " |-> Filter     : %p (node=%p)\n", filter,
              filter->GetElement());
    if (LayoutSVGResourceMasker* masker = clipper_filter_masker_data_->masker)
      fprintf(stderr, " |-> Masker     : %p (node=%p)\n", masker,
              masker->GetElement());
  }

  if (marker_data_) {
    if (LayoutSVGResourceMarker* marker_start = marker_data_->marker_start)
      fprintf(stderr, " |-> MarkerStart: %p (node=%p)\n", marker_start,
              marker_start->GetElement());
    if (LayoutSVGResourceMarker* marker_mid = marker_data_->marker_mid)
      fprintf(stderr, " |-> MarkerMid  : %p (node=%p)\n", marker_mid,
              marker_mid->GetElement());
    if (LayoutSVGResourceMarker* marker_end = marker_data_->marker_end)
      fprintf(stderr, " |-> MarkerEnd  : %p (node=%p)\n", marker_end,
              marker_end->GetElement());
  }

  if (fill_stroke_data_) {
    if (LayoutSVGResourcePaintServer* fill = fill_stroke_data_->fill)
      fprintf(stderr, " |-> Fill       : %p (node=%p)\n", fill,
              fill->GetElement());
    if (LayoutSVGResourcePaintServer* stroke = fill_stroke_data_->stroke)
      fprintf(stderr, " |-> Stroke     : %p (node=%p)\n", stroke,
              stroke->GetElement());
  }

  if (linked_resource_)
    fprintf(stderr, " |-> xlink:href : %p (node=%p)\n", linked_resource_,
            linked_resource_->GetElement());
}
#endif

}  // namespace blink
