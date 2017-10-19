/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
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

#include "core/svg/SVGFEImageElement.h"

#include "core/dom/Document.h"
#include "core/dom/IdTargetObserver.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/svg/SVGPreserveAspectRatio.h"
#include "core/svg/graphics/filters/SVGFEImage.h"
#include "core/svg_names.h"
#include "platform/graphics/Image.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"

namespace blink {

inline SVGFEImageElement::SVGFEImageElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feImageTag, document),
      SVGURIReference(this),
      preserve_aspect_ratio_(SVGAnimatedPreserveAspectRatio::Create(
          this,
          SVGNames::preserveAspectRatioAttr)) {
  AddToPropertyMap(preserve_aspect_ratio_);
}

DEFINE_NODE_FACTORY(SVGFEImageElement)

SVGFEImageElement::~SVGFEImageElement() {
  ClearImageResource();
}

void SVGFEImageElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(preserve_aspect_ratio_);
  visitor->Trace(cached_image_);
  visitor->Trace(target_id_observer_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
  SVGURIReference::Trace(visitor);
}

bool SVGFEImageElement::CurrentFrameHasSingleSecurityOrigin() const {
  if (cached_image_ && cached_image_->GetImage())
    return cached_image_->GetImage()->CurrentFrameHasSingleSecurityOrigin();

  return true;
}

void SVGFEImageElement::ClearResourceReferences() {
  ClearImageResource();
  UnobserveTarget(target_id_observer_);
  RemoveAllOutgoingReferences();
}

void SVGFEImageElement::FetchImageResource() {
  ResourceLoaderOptions options;
  options.initiator_info.name = localName();
  FetchParameters params(
      ResourceRequest(GetDocument().CompleteURL(HrefString())), options);
  cached_image_ = ImageResourceContent::Fetch(params, GetDocument().Fetcher());

  if (cached_image_)
    cached_image_->AddObserver(this);
}

void SVGFEImageElement::ClearImageResource() {
  if (!cached_image_)
    return;
  cached_image_->RemoveObserver(this);
  cached_image_ = nullptr;
}

void SVGFEImageElement::BuildPendingResource() {
  ClearResourceReferences();
  if (!isConnected())
    return;

  Element* target = ObserveTarget(target_id_observer_, *this);
  if (!target) {
    if (!SVGURLReferenceResolver(HrefString(), GetDocument()).IsLocal())
      FetchImageResource();
  } else if (target->IsSVGElement()) {
    // Register us with the target in the dependencies map. Any change of
    // hrefElement that leads to relayout/repainting now informs us, so we can
    // react to it.
    AddReferenceTo(ToSVGElement(target));
  }

  Invalidate();
}

void SVGFEImageElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == SVGNames::preserveAspectRatioAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    Invalidate();
    return;
  }

  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    BuildPendingResource();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(attr_name);
}

Node::InsertionNotificationRequest SVGFEImageElement::InsertedInto(
    ContainerNode* root_parent) {
  SVGFilterPrimitiveStandardAttributes::InsertedInto(root_parent);
  BuildPendingResource();
  return kInsertionDone;
}

void SVGFEImageElement::RemovedFrom(ContainerNode* root_parent) {
  SVGFilterPrimitiveStandardAttributes::RemovedFrom(root_parent);
  if (root_parent->isConnected())
    ClearResourceReferences();
}

void SVGFEImageElement::ImageNotifyFinished(ImageResourceContent*) {
  if (!isConnected())
    return;

  Element* parent = parentElement();
  if (!parent || !IsSVGFilterElement(parent) || !parent->GetLayoutObject())
    return;

  if (LayoutObject* layout_object = this->GetLayoutObject())
    MarkForLayoutAndParentResourceInvalidation(layout_object);
}

FilterEffect* SVGFEImageElement::Build(SVGFilterBuilder*, Filter* filter) {
  if (cached_image_) {
    // Don't use the broken image icon on image loading errors.
    RefPtr<Image> image =
        cached_image_->ErrorOccurred() ? nullptr : cached_image_->GetImage();
    return FEImage::CreateWithImage(filter, image,
                                    preserve_aspect_ratio_->CurrentValue());
  }

  return FEImage::CreateWithIRIReference(
      filter, GetTreeScope(), HrefString(),
      preserve_aspect_ratio_->CurrentValue());
}

}  // namespace blink
