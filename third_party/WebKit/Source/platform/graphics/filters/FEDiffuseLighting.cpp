/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "platform/graphics/filters/FEDiffuseLighting.h"

#include "platform/graphics/filters/LightSource.h"
#include "platform/text/TextStream.h"

namespace blink {

FEDiffuseLighting::FEDiffuseLighting(Filter* filter,
                                     const Color& lighting_color,
                                     float surface_scale,
                                     float diffuse_constant,
                                     RefPtr<LightSource> light_source)
    : FELighting(filter,
                 kDiffuseLighting,
                 lighting_color,
                 surface_scale,
                 diffuse_constant,
                 0,
                 0,
                 std::move(light_source)) {}

FEDiffuseLighting* FEDiffuseLighting::Create(Filter* filter,
                                             const Color& lighting_color,
                                             float surface_scale,
                                             float diffuse_constant,
                                             RefPtr<LightSource> light_source) {
  return new FEDiffuseLighting(filter, lighting_color, surface_scale,
                               diffuse_constant, std::move(light_source));
}

FEDiffuseLighting::~FEDiffuseLighting() {}

Color FEDiffuseLighting::LightingColor() const {
  return lighting_color_;
}

bool FEDiffuseLighting::SetLightingColor(const Color& lighting_color) {
  if (lighting_color_ == lighting_color)
    return false;
  lighting_color_ = lighting_color;
  return true;
}

float FEDiffuseLighting::SurfaceScale() const {
  return surface_scale_;
}

bool FEDiffuseLighting::SetSurfaceScale(float surface_scale) {
  if (surface_scale_ == surface_scale)
    return false;
  surface_scale_ = surface_scale;
  return true;
}

float FEDiffuseLighting::DiffuseConstant() const {
  return diffuse_constant_;
}

bool FEDiffuseLighting::SetDiffuseConstant(float diffuse_constant) {
  diffuse_constant = std::max(diffuse_constant, 0.0f);
  if (diffuse_constant_ == diffuse_constant)
    return false;
  diffuse_constant_ = diffuse_constant;
  return true;
}

const LightSource* FEDiffuseLighting::GetLightSource() const {
  return light_source_.get();
}

void FEDiffuseLighting::SetLightSource(RefPtr<LightSource> light_source) {
  light_source_ = std::move(light_source);
}

TextStream& FEDiffuseLighting::ExternalRepresentation(TextStream& ts,
                                                      int indent) const {
  WriteIndent(ts, indent);
  ts << "[feDiffuseLighting";
  FilterEffect::ExternalRepresentation(ts);
  ts << " surfaceScale=\"" << surface_scale_ << "\" "
     << "diffuseConstant=\"" << diffuse_constant_ << "\"]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink
