/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/css/css_counter_value.h"
#include "third_party/blink/renderer/core/css/css_cursor_image_value.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_layout_function_value.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_value_id_mappings.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/css_variable_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/element_style_resources.h"
#include "third_party/blink/renderer/core/css/resolver/filter_operation_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/font_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/core/style/counter_content.h"
#include "third_party/blink/renderer/core/style/quotes_data.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/style/style_inherited_variables.h"
#include "third_party/blink/renderer/core/style/style_non_inherited_variables.h"
#include "third_party/blink/renderer/core/style/svg_computed_style.h"
#include "third_party/blink/renderer/core/style_builder_functions.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

static inline bool IsValidVisitedLinkProperty(CSSPropertyID id) {
  switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyCaretColor:
    case CSSPropertyColor:
    case CSSPropertyFill:
    case CSSPropertyOutlineColor:
    case CSSPropertyStroke:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
      return true;
    default:
      return false;
  }
}

}  // namespace

void StyleBuilder::ApplyProperty(const CSSProperty& property,
                                 StyleResolverState& state,
                                 const CSSValue& value) {
  CSSPropertyID id = property.PropertyID();
  bool is_inherited = property.IsInherited();
  if (id != CSSPropertyVariable && (value.IsVariableReferenceValue() ||
                                    value.IsPendingSubstitutionValue())) {
    bool omit_animation_tainted =
        CSSAnimations::IsAnimationAffectingProperty(property);
    const CSSValue* resolved_value =
        CSSVariableResolver(state).ResolveVariableReferences(
            id, value, omit_animation_tainted);
    ApplyProperty(property, state, *resolved_value);

    if (!state.Style()->HasVariableReferenceFromNonInheritedProperty() &&
        !is_inherited)
      state.Style()->SetHasVariableReferenceFromNonInheritedProperty();
    return;
  }

  DCHECK(!property.IsShorthand())
      << "Shorthand property id = " << id << " wasn't expanded at parsing time";

  bool is_inherit = state.ParentNode() && value.IsInheritedValue();
  bool is_initial = value.IsInitialValue() ||
                    (!state.ParentNode() && value.IsInheritedValue());

  // isInherit => !isInitial && isInitial => !isInherit
  DCHECK(!is_inherit || !is_initial);
  // isInherit => (state.parentNode() && state.parentStyle())
  DCHECK(!is_inherit || (state.ParentNode() && state.ParentStyle()));

  if (!state.ApplyPropertyToRegularStyle() &&
      (!state.ApplyPropertyToVisitedLinkStyle() ||
       !IsValidVisitedLinkProperty(id))) {
    // Limit the properties that can be applied to only the ones honored by
    // :visited.
    return;
  }

  if (is_inherit && !state.ParentStyle()->HasExplicitlyInheritedProperties() &&
      !is_inherited) {
    state.ParentStyle()->SetHasExplicitlyInheritedProperties();
  } else if (value.IsUnsetValue()) {
    DCHECK(!is_inherit && !is_initial);
    if (is_inherited)
      is_inherit = true;
    else
      is_initial = true;
  }

  StyleBuilder::ApplyProperty(property, state, value, is_initial, is_inherit);
}

}  // namespace blink
