/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"

#include "base/macros.h"
#include "third_party/blink/renderer/core/animation/css/css_animation_data.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_import_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/css_media_rule.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/font_size_functions.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_rule_usage_tracker.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/css/style_sheet_list.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_history.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_container.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_content_loader.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/shaping/caching_word_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_concatenate.h"

namespace blink {

using protocol::Response;

namespace {

int g_frontend_operation_counter = 0;

class FrontendOperationScope {
  STACK_ALLOCATED();

 public:
  FrontendOperationScope() { ++g_frontend_operation_counter; }
  ~FrontendOperationScope() { --g_frontend_operation_counter; }
};

String CreateShorthandValue(Document* document,
                            const String& shorthand,
                            const String& old_text,
                            const String& longhand,
                            const String& new_value) {
  auto* style_sheet_contents = MakeGarbageCollected<StyleSheetContents>(
      StrictCSSParserContext(document->GetSecureContextMode()));
  String text = " div { " + shorthand + ": " + old_text + "; }";
  CSSParser::ParseSheet(MakeGarbageCollected<CSSParserContext>(*document),
                        style_sheet_contents, text);

  auto* style_sheet = MakeGarbageCollected<CSSStyleSheet>(style_sheet_contents);
  auto* rule = To<CSSStyleRule>(style_sheet->item(0));
  CSSStyleDeclaration* style = rule->style();
  DummyExceptionStateForTesting exception_state;
  style->setProperty(document->GetExecutionContext(), longhand, new_value,
                     style->getPropertyPriority(longhand), exception_state);
  return style->getPropertyValue(shorthand);
}

HeapVector<Member<CSSStyleRule>> FilterDuplicateRules(
    RuleIndexList* rule_list) {
  HeapVector<Member<CSSStyleRule>> uniq_rules;
  HeapHashSet<Member<CSSRule>> uniq_rules_set;
  for (unsigned i = rule_list ? rule_list->size() : 0; i > 0; --i) {
    CSSRule* rule = rule_list->at(i - 1).first;
    auto* style_rule = DynamicTo<CSSStyleRule>(rule);
    if (!style_rule || uniq_rules_set.Contains(rule))
      continue;
    uniq_rules_set.insert(rule);
    uniq_rules.push_back(style_rule);
  }
  uniq_rules.Reverse();
  return uniq_rules;
}

// Get the elements which overlap the given rectangle.
HeapVector<Member<Element>> ElementsFromRect(const PhysicalRect& rect,
                                             Document& document) {
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                         HitTestRequest::kListBased |
                         HitTestRequest::kPenetratingList |
                         HitTestRequest::kIgnoreClipping);

  HitTestLocation location(rect);
  HitTestResult result(request, location);
  document.GetFrame()->ContentLayoutObject()->HitTest(location, result);
  HeapVector<Member<Element>> elements;
  Node* previous_node = nullptr;
  for (const auto& hit_test_result_node : result.ListBasedTestResult()) {
    Node* node = hit_test_result_node.Get();
    if (!node || node->IsDocumentNode())
      continue;
    if (node->IsPseudoElement() || node->IsTextNode())
      node = node->ParentOrShadowHostNode();
    auto* element = DynamicTo<Element>(node);
    if (!node || node == previous_node || !element)
      continue;
    elements.push_back(element);
    previous_node = node;
  }
  return elements;
}

// Blends the colors from the given gradient with the existing colors.
void BlendWithColorsFromGradient(cssvalue::CSSGradientValue* gradient,
                                 Vector<Color>& colors,
                                 bool& found_non_transparent_color,
                                 bool& found_opaque_color,
                                 const LayoutObject& layout_object) {
  const Document& document = layout_object.GetDocument();
  const ComputedStyle& style = layout_object.StyleRef();

  Vector<Color> stop_colors = gradient->GetStopColors(document, style);
  if (colors.IsEmpty()) {
    colors.AppendRange(stop_colors.begin(), stop_colors.end());
  } else {
    if (colors.size() > 1) {
      // Gradient on gradient is too complicated, bail out.
      colors.clear();
      return;
    }

    Color existing_color = colors.front();
    colors.clear();
    for (auto stop_color : stop_colors) {
      found_non_transparent_color =
          found_non_transparent_color || (stop_color.Alpha() != 0);
      colors.push_back(existing_color.Blend(stop_color));
    }
  }
  found_opaque_color =
      found_opaque_color || gradient->KnownToBeOpaque(document, style);
}

// Gets the colors from an image style, if one exists and it is a gradient.
void AddColorsFromImageStyle(const ComputedStyle& style,
                             Vector<Color>& colors,
                             bool& found_opaque_color,
                             bool& found_non_transparent_color,
                             const LayoutObject& layout_object) {
  const FillLayer& background_layers = style.BackgroundLayers();
  if (!background_layers.AnyLayerHasImage())
    return;

  StyleImage* style_image = background_layers.GetImage();
  // hasImage() does not always indicate that this is non-null
  if (!style_image)
    return;

  if (!style_image->IsGeneratedImage()) {
    // Make no assertions about the colors in non-generated images
    colors.clear();
    found_opaque_color = false;
    return;
  }

  StyleGeneratedImage* gen_image = To<StyleGeneratedImage>(style_image);
  CSSValue* image_css = gen_image->CssValue();
  if (auto* gradient = DynamicTo<cssvalue::CSSGradientValue>(image_css)) {
    BlendWithColorsFromGradient(gradient, colors, found_non_transparent_color,
                                found_opaque_color, layout_object);
  }
}

// Get the background colors behind the given rect in the given document, by
// walking up all the elements returned by a hit test (but not going beyond
// |topElement|) covering the area of the rect, and blending their background
// colors.
bool GetColorsFromRect(PhysicalRect rect,
                       Document& document,
                       Element* top_element,
                       Vector<Color>& colors) {
  HeapVector<Member<Element>> elements_under_rect =
      ElementsFromRect(rect, document);

  bool found_opaque_color = false;
  bool found_top_element = false;

  for (auto e = elements_under_rect.rbegin();
       !found_top_element && e != elements_under_rect.rend(); ++e) {
    const Element* element = *e;
    if (element == top_element)
      found_top_element = true;

    const LayoutObject* layout_object = element->GetLayoutObject();
    if (!layout_object)
      continue;

    if (IsA<HTMLCanvasElement>(element) || IsA<HTMLEmbedElement>(element) ||
        IsA<HTMLImageElement>(element) || IsA<HTMLObjectElement>(element) ||
        IsA<HTMLPictureElement>(element) || element->IsSVGElement() ||
        IsA<HTMLVideoElement>(element)) {
      colors.clear();
      found_opaque_color = false;
      continue;
    }

    const ComputedStyle* style = layout_object->Style();
    if (!style)
      continue;

    Color background_color =
        style->VisitedDependentColor(GetCSSPropertyBackgroundColor());
    bool found_non_transparent_color = false;
    if (background_color.Alpha() != 0) {
      found_non_transparent_color = true;
      if (colors.IsEmpty()) {
        if (!background_color.HasAlpha())
          found_opaque_color = true;
        colors.push_back(background_color);
      } else {
        if (!background_color.HasAlpha()) {
          colors.clear();
          colors.push_back(background_color);
          found_opaque_color = true;
        } else {
          for (wtf_size_t i = 0; i < colors.size(); i++)
            colors[i] = colors[i].Blend(background_color);
          found_opaque_color =
              found_opaque_color || background_color.HasAlpha();
        }
      }
    }

    AddColorsFromImageStyle(*style, colors, found_opaque_color,
                            found_non_transparent_color, *layout_object);

    bool contains = found_top_element || element->BoundingBox().Contains(rect);
    if (!contains && found_non_transparent_color) {
      // Only return colors if some opaque element covers up this one.
      colors.clear();
      found_opaque_color = false;
    }
  }
  return found_opaque_color;
}

void CollectPlatformFontsFromRunFontDataList(
    const Vector<ShapeResult::RunFontData>& run_font_data_list,
    HashCountedSet<std::pair<int, String>>* font_stats) {
  for (const auto& run_font_data : run_font_data_list) {
    const auto* simple_font_data = run_font_data.font_data_;
    String family_name = simple_font_data->PlatformData().FontFamilyName();
    if (family_name.IsNull())
      family_name = "";
    font_stats->insert(
        std::make_pair(simple_font_data->IsCustomFont() ? 1 : 0, family_name),
        run_font_data.glyph_count_);
  }
}

}  // namespace

typedef blink::protocol::CSS::Backend::EnableCallback EnableCallback;

enum ForcePseudoClassFlags {
  kPseudoNone = 0,
  kPseudoHover = 1 << 0,
  kPseudoFocus = 1 << 1,
  kPseudoActive = 1 << 2,
  kPseudoVisited = 1 << 3,
  kPseudoFocusWithin = 1 << 4,
  kPseudoFocusVisible = 1 << 5
};

static unsigned ComputePseudoClassMask(
    std::unique_ptr<protocol::Array<String>> pseudo_class_array) {
  DEFINE_STATIC_LOCAL(String, active, ("active"));
  DEFINE_STATIC_LOCAL(String, hover, ("hover"));
  DEFINE_STATIC_LOCAL(String, focus, ("focus"));
  DEFINE_STATIC_LOCAL(String, focusVisible, ("focus-visible"));
  DEFINE_STATIC_LOCAL(String, focusWithin, ("focus-within"));
  DEFINE_STATIC_LOCAL(String, visited, ("visited"));
  if (!pseudo_class_array || pseudo_class_array->empty())
    return kPseudoNone;

  unsigned result = kPseudoNone;
  for (const String& pseudo_class : *pseudo_class_array) {
    if (pseudo_class == active)
      result |= kPseudoActive;
    else if (pseudo_class == hover)
      result |= kPseudoHover;
    else if (pseudo_class == focus)
      result |= kPseudoFocus;
    else if (pseudo_class == focusVisible)
      result |= kPseudoFocusVisible;
    else if (pseudo_class == focusWithin)
      result |= kPseudoFocusWithin;
    else if (pseudo_class == visited)
      result |= kPseudoVisited;
  }

  return result;
}

class InspectorCSSAgent::StyleSheetAction : public InspectorHistory::Action {
 public:
  StyleSheetAction(const String& name) : InspectorHistory::Action(name) {}

  virtual std::unique_ptr<protocol::CSS::CSSStyle> TakeSerializedStyle() {
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StyleSheetAction);
};

class InspectorCSSAgent::SetStyleSheetTextAction final
    : public InspectorCSSAgent::StyleSheetAction {
 public:
  SetStyleSheetTextAction(InspectorStyleSheetBase* style_sheet,
                          const String& text)
      : InspectorCSSAgent::StyleSheetAction("SetStyleSheetText"),
        style_sheet_(style_sheet),
        text_(text) {}

  bool Perform(ExceptionState& exception_state) override {
    if (!style_sheet_->GetText(&old_text_))
      return false;
    return Redo(exception_state);
  }

  bool Undo(ExceptionState& exception_state) override {
    return style_sheet_->SetText(old_text_, exception_state);
  }

  bool Redo(ExceptionState& exception_state) override {
    return style_sheet_->SetText(text_, exception_state);
  }

  String MergeId() override {
    return String::Format("SetStyleSheetText %s",
                          style_sheet_->Id().Utf8().c_str());
  }

  void Merge(Action* action) override {
    DCHECK_EQ(action->MergeId(), MergeId());

    SetStyleSheetTextAction* other =
        static_cast<SetStyleSheetTextAction*>(action);
    text_ = other->text_;
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(style_sheet_);
    InspectorCSSAgent::StyleSheetAction::Trace(visitor);
  }

 private:
  Member<InspectorStyleSheetBase> style_sheet_;
  String text_;
  String old_text_;
  DISALLOW_COPY_AND_ASSIGN(SetStyleSheetTextAction);
};

class InspectorCSSAgent::ModifyRuleAction final
    : public InspectorCSSAgent::StyleSheetAction {
 public:
  enum Type {
    kSetRuleSelector,
    kSetStyleText,
    kSetMediaRuleText,
    kSetKeyframeKey
  };

  ModifyRuleAction(Type type,
                   InspectorStyleSheet* style_sheet,
                   const SourceRange& range,
                   const String& text)
      : InspectorCSSAgent::StyleSheetAction("ModifyRuleAction"),
        style_sheet_(style_sheet),
        type_(type),
        new_text_(text),
        old_range_(range),
        css_rule_(nullptr) {}

  bool Perform(ExceptionState& exception_state) override {
    return Redo(exception_state);
  }

  bool Undo(ExceptionState& exception_state) override {
    switch (type_) {
      case kSetRuleSelector:
        return style_sheet_->SetRuleSelector(new_range_, old_text_, nullptr,
                                             nullptr, exception_state);
      case kSetStyleText:
        return style_sheet_->SetStyleText(new_range_, old_text_, nullptr,
                                          nullptr, exception_state);
      case kSetMediaRuleText:
        return style_sheet_->SetMediaRuleText(new_range_, old_text_, nullptr,
                                              nullptr, exception_state);
      case kSetKeyframeKey:
        return style_sheet_->SetKeyframeKey(new_range_, old_text_, nullptr,
                                            nullptr, exception_state);
      default:
        NOTREACHED();
    }
    return false;
  }

  bool Redo(ExceptionState& exception_state) override {
    switch (type_) {
      case kSetRuleSelector:
        css_rule_ = style_sheet_->SetRuleSelector(
            old_range_, new_text_, &new_range_, &old_text_, exception_state);
        break;
      case kSetStyleText:
        css_rule_ = style_sheet_->SetStyleText(
            old_range_, new_text_, &new_range_, &old_text_, exception_state);
        break;
      case kSetMediaRuleText:
        css_rule_ = style_sheet_->SetMediaRuleText(
            old_range_, new_text_, &new_range_, &old_text_, exception_state);
        break;
      case kSetKeyframeKey:
        css_rule_ = style_sheet_->SetKeyframeKey(
            old_range_, new_text_, &new_range_, &old_text_, exception_state);
        break;
      default:
        NOTREACHED();
    }
    return css_rule_;
  }

  CSSRule* TakeRule() {
    CSSRule* result = css_rule_;
    css_rule_ = nullptr;
    return result;
  }

  std::unique_ptr<protocol::CSS::CSSStyle> TakeSerializedStyle() override {
    if (type_ != kSetStyleText)
      return nullptr;
    CSSRule* rule = TakeRule();
    if (auto* style_rule = DynamicTo<CSSStyleRule>(rule))
      return style_sheet_->BuildObjectForStyle(style_rule->style());
    if (auto* keyframe_rule = DynamicTo<CSSKeyframeRule>(rule))
      return style_sheet_->BuildObjectForStyle(keyframe_rule->style());
    return nullptr;
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(style_sheet_);
    visitor->Trace(css_rule_);
    InspectorCSSAgent::StyleSheetAction::Trace(visitor);
  }

  String MergeId() override {
    return String::Format("ModifyRuleAction:%d %s:%d", type_,
                          style_sheet_->Id().Utf8().c_str(), old_range_.start);
  }

  bool IsNoop() override { return old_text_ == new_text_; }

  void Merge(Action* action) override {
    DCHECK_EQ(action->MergeId(), MergeId());

    ModifyRuleAction* other = static_cast<ModifyRuleAction*>(action);
    new_text_ = other->new_text_;
    new_range_ = other->new_range_;
  }

 private:
  Member<InspectorStyleSheet> style_sheet_;
  Type type_;
  String old_text_;
  String new_text_;
  SourceRange old_range_;
  SourceRange new_range_;
  Member<CSSRule> css_rule_;
  DISALLOW_COPY_AND_ASSIGN(ModifyRuleAction);
};

class InspectorCSSAgent::SetElementStyleAction final
    : public InspectorCSSAgent::StyleSheetAction {
 public:
  SetElementStyleAction(InspectorStyleSheetForInlineStyle* style_sheet,
                        const String& text)
      : InspectorCSSAgent::StyleSheetAction("SetElementStyleAction"),
        style_sheet_(style_sheet),
        text_(text) {}

  bool Perform(ExceptionState& exception_state) override {
    return Redo(exception_state);
  }

  bool Undo(ExceptionState& exception_state) override {
    return style_sheet_->SetText(old_text_, exception_state);
  }

  bool Redo(ExceptionState& exception_state) override {
    if (!style_sheet_->GetText(&old_text_))
      return false;
    return style_sheet_->SetText(text_, exception_state);
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(style_sheet_);
    InspectorCSSAgent::StyleSheetAction::Trace(visitor);
  }

  String MergeId() override {
    return String::Format("SetElementStyleAction:%s",
                          style_sheet_->Id().Utf8().c_str());
  }

  std::unique_ptr<protocol::CSS::CSSStyle> TakeSerializedStyle() override {
    return style_sheet_->BuildObjectForStyle(style_sheet_->InlineStyle());
  }

  void Merge(Action* action) override {
    DCHECK_EQ(action->MergeId(), MergeId());

    SetElementStyleAction* other = static_cast<SetElementStyleAction*>(action);
    text_ = other->text_;
  }

 private:
  Member<InspectorStyleSheetForInlineStyle> style_sheet_;
  String text_;
  String old_text_;
  DISALLOW_COPY_AND_ASSIGN(SetElementStyleAction);
};

class InspectorCSSAgent::AddRuleAction final
    : public InspectorCSSAgent::StyleSheetAction {
 public:
  AddRuleAction(InspectorStyleSheet* style_sheet,
                const String& rule_text,
                const SourceRange& location)
      : InspectorCSSAgent::StyleSheetAction("AddRule"),
        style_sheet_(style_sheet),
        rule_text_(rule_text),
        location_(location) {}

  bool Perform(ExceptionState& exception_state) override {
    return Redo(exception_state);
  }

  bool Undo(ExceptionState& exception_state) override {
    CSSStyleSheet::InspectorMutationScope scope(style_sheet_->PageStyleSheet());
    return style_sheet_->DeleteRule(added_range_, exception_state);
  }

  bool Redo(ExceptionState& exception_state) override {
    CSSStyleSheet::InspectorMutationScope scope(style_sheet_->PageStyleSheet());
    css_rule_ = style_sheet_->AddRule(rule_text_, location_, &added_range_,
                                      exception_state);
    if (exception_state.HadException())
      return false;
    return true;
  }

  CSSStyleRule* TakeRule() {
    CSSStyleRule* result = css_rule_;
    css_rule_ = nullptr;
    return result;
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(style_sheet_);
    visitor->Trace(css_rule_);
    InspectorCSSAgent::StyleSheetAction::Trace(visitor);
  }

 private:
  Member<InspectorStyleSheet> style_sheet_;
  Member<CSSStyleRule> css_rule_;
  String rule_text_;
  String old_text_;
  SourceRange location_;
  SourceRange added_range_;
  DISALLOW_COPY_AND_ASSIGN(AddRuleAction);
};

// static
CSSStyleRule* InspectorCSSAgent::AsCSSStyleRule(CSSRule* rule) {
  return DynamicTo<CSSStyleRule>(rule);
}

// static
CSSMediaRule* InspectorCSSAgent::AsCSSMediaRule(CSSRule* rule) {
  return DynamicTo<CSSMediaRule>(rule);
}

InspectorCSSAgent::InspectorCSSAgent(
    InspectorDOMAgent* dom_agent,
    InspectedFrames* inspected_frames,
    InspectorNetworkAgent* network_agent,
    InspectorResourceContentLoader* resource_content_loader,
    InspectorResourceContainer* resource_container)
    : dom_agent_(dom_agent),
      inspected_frames_(inspected_frames),
      network_agent_(network_agent),
      resource_content_loader_(resource_content_loader),
      resource_container_(resource_container),
      resource_content_loader_client_id_(
          resource_content_loader->CreateClientId()),
      enable_requested_(&agent_state_, /*default_value=*/false),
      enable_completed_(false),
      coverage_enabled_(&agent_state_, /*default_value=*/false) {}

InspectorCSSAgent::~InspectorCSSAgent() = default;

void InspectorCSSAgent::Restore() {
  if (enable_requested_.Get())
    CompleteEnabled();
  if (coverage_enabled_.Get())
    SetCoverageEnabled(true);
}

void InspectorCSSAgent::FlushPendingProtocolNotifications() {
  if (!invalidated_documents_.size())
    return;
  HeapHashSet<Member<Document>> invalidated_documents;
  invalidated_documents_.swap(invalidated_documents);
  for (Document* document : invalidated_documents)
    UpdateActiveStyleSheets(document);
}

void InspectorCSSAgent::Reset() {
  id_to_inspector_style_sheet_.clear();
  id_to_inspector_style_sheet_for_inline_style_.clear();
  css_style_sheet_to_inspector_style_sheet_.clear();
  document_to_css_style_sheets_.clear();
  invalidated_documents_.clear();
  node_to_inspector_style_sheet_.clear();
  ResetNonPersistentData();
}

void InspectorCSSAgent::ResetNonPersistentData() {
  ResetPseudoStates();
}

void InspectorCSSAgent::enable(std::unique_ptr<EnableCallback> prp_callback) {
  if (!dom_agent_->Enabled()) {
    prp_callback->sendFailure(
        Response::ServerError("DOM agent needs to be enabled first."));
    return;
  }
  enable_requested_.Set(true);
  resource_content_loader_->EnsureResourcesContentLoaded(
      resource_content_loader_client_id_,
      WTF::Bind(&InspectorCSSAgent::ResourceContentLoaded, WrapPersistent(this),
                WTF::Passed(std::move(prp_callback))));
}

void InspectorCSSAgent::ResourceContentLoaded(
    std::unique_ptr<EnableCallback> callback) {
  if (enable_requested_.Get())  // Could be disabled while fetching resources.
    CompleteEnabled();
  callback->sendSuccess();
}

void InspectorCSSAgent::CompleteEnabled() {
  instrumenting_agents_->AddInspectorCSSAgent(this);
  dom_agent_->SetDOMListener(this);
  HeapVector<Member<Document>> documents = dom_agent_->Documents();
  for (Document* document : documents)
    UpdateActiveStyleSheets(document);
  enable_completed_ = true;
}

Response InspectorCSSAgent::disable() {
  Reset();
  dom_agent_->SetDOMListener(nullptr);
  instrumenting_agents_->RemoveInspectorCSSAgent(this);
  enable_completed_ = false;
  enable_requested_.Set(false);
  resource_content_loader_->Cancel(resource_content_loader_client_id_);
  coverage_enabled_.Set(false);
  SetCoverageEnabled(false);
  return Response::Success();
}

void InspectorCSSAgent::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  if (frame == inspected_frames_->Root())
    Reset();
}

void InspectorCSSAgent::MediaQueryResultChanged() {
  FlushPendingProtocolNotifications();
  GetFrontend()->mediaQueryResultChanged();
}

void InspectorCSSAgent::FontsUpdated(
    const FontFace* font,
    const String& src,
    const FontCustomPlatformData* fontCustomPlatformData) {
  FlushPendingProtocolNotifications();

  if (!(font && src && fontCustomPlatformData)) {
    GetFrontend()->fontsUpdated();
    return;
  }

  // blink::FontFace returns sane property defaults per the web fonts spec,
  // so we don't perform null checks here.
  std::unique_ptr<protocol::CSS::FontFace> font_face =
      protocol::CSS::FontFace::create()
          .setFontFamily(font->family())
          .setFontStyle(font->style())
          .setFontVariant(font->variant())
          .setFontWeight(font->weight())
          .setFontStretch(font->stretch())
          .setUnicodeRange(font->unicodeRange())
          .setSrc(src)
          .setPlatformFontFamily(
              fontCustomPlatformData->FamilyNameForInspector())
          .build();
  GetFrontend()->fontsUpdated(std::move(font_face));
}

void InspectorCSSAgent::ActiveStyleSheetsUpdated(Document* document) {
  invalidated_documents_.insert(document);
}

void InspectorCSSAgent::UpdateActiveStyleSheets(Document* document) {
  HeapVector<Member<CSSStyleSheet>> new_sheets_vector;
  InspectorCSSAgent::CollectAllDocumentStyleSheets(document, new_sheets_vector);
  SetActiveStyleSheets(document, new_sheets_vector);
}

void InspectorCSSAgent::SetActiveStyleSheets(
    Document* document,
    const HeapVector<Member<CSSStyleSheet>>& all_sheets_vector) {
  HeapHashSet<Member<CSSStyleSheet>>* document_css_style_sheets =
      document_to_css_style_sheets_.at(document);
  if (!document_css_style_sheets) {
    document_css_style_sheets =
        MakeGarbageCollected<HeapHashSet<Member<CSSStyleSheet>>>();
    document_to_css_style_sheets_.Set(document, document_css_style_sheets);
  }

  HeapHashSet<Member<CSSStyleSheet>> removed_sheets(*document_css_style_sheets);
  HeapVector<Member<CSSStyleSheet>> added_sheets;
  for (CSSStyleSheet* css_style_sheet : all_sheets_vector) {
    if (removed_sheets.Contains(css_style_sheet)) {
      removed_sheets.erase(css_style_sheet);
    } else {
      added_sheets.push_back(css_style_sheet);
    }
  }

  for (CSSStyleSheet* css_style_sheet : removed_sheets) {
    InspectorStyleSheet* inspector_style_sheet =
        css_style_sheet_to_inspector_style_sheet_.at(css_style_sheet);
    DCHECK(inspector_style_sheet);

    document_css_style_sheets->erase(css_style_sheet);
    if (id_to_inspector_style_sheet_.Contains(inspector_style_sheet->Id())) {
      String id = UnbindStyleSheet(inspector_style_sheet);
      if (GetFrontend())
        GetFrontend()->styleSheetRemoved(id);
    }
  }

  for (CSSStyleSheet* css_style_sheet : added_sheets) {
    InspectorStyleSheet* new_style_sheet = BindStyleSheet(css_style_sheet);
    document_css_style_sheets->insert(css_style_sheet);
    if (GetFrontend()) {
      GetFrontend()->styleSheetAdded(
          new_style_sheet->BuildObjectForStyleSheetInfo());
    }
  }

  if (document_css_style_sheets->IsEmpty())
    document_to_css_style_sheets_.erase(document);
}

void InspectorCSSAgent::DocumentDetached(Document* document) {
  invalidated_documents_.erase(document);
  SetActiveStyleSheets(document, HeapVector<Member<CSSStyleSheet>>());
}

void InspectorCSSAgent::ForcePseudoState(Element* element,
                                         CSSSelector::PseudoType pseudo_type,
                                         bool* result) {
  if (node_id_to_forced_pseudo_state_.IsEmpty())
    return;

  int node_id = dom_agent_->BoundNodeId(element);
  if (!node_id)
    return;

  // First check whether focus-within was set because focus or focus-within was
  // forced for a child node.
  NodeIdToNumberFocusedChildren::iterator focused_it =
      node_id_to_number_focused_children_.find(node_id);
  unsigned focused_count =
      focused_it == node_id_to_number_focused_children_.end()
          ? 0
          : focused_it->value;
  if (pseudo_type == CSSSelector::kPseudoFocusWithin && focused_count > 0) {
    *result = true;
    return;
  }

  NodeIdToForcedPseudoState::iterator it =
      node_id_to_forced_pseudo_state_.find(node_id);
  if (it == node_id_to_forced_pseudo_state_.end())
    return;

  bool force = false;
  unsigned forced_pseudo_state = it->value;

  switch (pseudo_type) {
    case CSSSelector::kPseudoActive:
      force = forced_pseudo_state & kPseudoActive;
      break;
    case CSSSelector::kPseudoFocus:
      force = forced_pseudo_state & kPseudoFocus;
      break;
    case CSSSelector::kPseudoFocusWithin:
      force = forced_pseudo_state & kPseudoFocusWithin;
      break;
    case CSSSelector::kPseudoFocusVisible:
      force = forced_pseudo_state & kPseudoFocusVisible;
      break;
    case CSSSelector::kPseudoHover:
      force = forced_pseudo_state & kPseudoHover;
      break;
    case CSSSelector::kPseudoVisited:
      force = forced_pseudo_state & kPseudoVisited;
      break;
    default:
      break;
  }
  if (force)
    *result = true;
}

Response InspectorCSSAgent::getMediaQueries(
    std::unique_ptr<protocol::Array<protocol::CSS::CSSMedia>>* medias) {
  *medias = std::make_unique<protocol::Array<protocol::CSS::CSSMedia>>();
  for (auto& style : id_to_inspector_style_sheet_) {
    InspectorStyleSheet* style_sheet = style.value;
    CollectMediaQueriesFromStyleSheet(style_sheet->PageStyleSheet(),
                                      medias->get());
    const CSSRuleVector& flat_rules = style_sheet->FlatRules();
    for (unsigned i = 0; i < flat_rules.size(); ++i) {
      CSSRule* rule = flat_rules.at(i).Get();
      if (rule->type() == CSSRule::kMediaRule ||
          rule->type() == CSSRule::kImportRule)
        CollectMediaQueriesFromRule(rule, medias->get());
    }
  }
  return Response::Success();
}

Response InspectorCSSAgent::getMatchedStylesForNode(
    int node_id,
    Maybe<protocol::CSS::CSSStyle>* inline_style,
    Maybe<protocol::CSS::CSSStyle>* attributes_style,
    Maybe<protocol::Array<protocol::CSS::RuleMatch>>* matched_css_rules,
    Maybe<protocol::Array<protocol::CSS::PseudoElementMatches>>*
        pseudo_id_matches,
    Maybe<protocol::Array<protocol::CSS::InheritedStyleEntry>>*
        inherited_entries,
    Maybe<protocol::Array<protocol::CSS::CSSKeyframesRule>>*
        css_keyframes_rules) {
  Response response = AssertEnabled();
  if (!response.IsSuccess())
    return response;

  Element* element = nullptr;
  response = dom_agent_->AssertElement(node_id, element);
  if (!response.IsSuccess())
    return response;

  Element* original_element = element;
  PseudoId element_pseudo_id = element->GetPseudoId();
  if (element_pseudo_id) {
    element = element->ParentOrShadowHostElement();
    if (!element)
      return Response::ServerError("Pseudo element has no parent");
  }

  Document* owner_document = element->ownerDocument();
  // A non-active document has no styles.
  if (!owner_document->IsActive())
    return Response::ServerError("Document is not active");

  // FIXME: It's really gross for the inspector to reach in and access
  // StyleResolver directly here. We need to provide the Inspector better APIs
  // to get this information without grabbing at internal style classes!

  // Matched rules.
  StyleResolver& style_resolver = owner_document->EnsureStyleResolver();

  element->UpdateDistributionForUnknownReasons();
  RuleIndexList* matched_rules = style_resolver.PseudoCSSRulesForElement(
      element, element_pseudo_id, StyleResolver::kAllCSSRules);
  *matched_css_rules = BuildArrayForMatchedRuleList(
      matched_rules, original_element, kPseudoIdNone);

  // Pseudo elements.
  if (element_pseudo_id)
    return Response::Success();

  InspectorStyleSheetForInlineStyle* inline_style_sheet =
      AsInspectorStyleSheet(element);
  if (inline_style_sheet) {
    *inline_style = inline_style_sheet->BuildObjectForStyle(element->style());
    *attributes_style = BuildObjectForAttributesStyle(element);
  }

  *pseudo_id_matches =
      std::make_unique<protocol::Array<protocol::CSS::PseudoElementMatches>>();
  for (PseudoId pseudo_id = kFirstPublicPseudoId;
       pseudo_id < kAfterLastInternalPseudoId;
       pseudo_id = static_cast<PseudoId>(pseudo_id + 1)) {
    RuleIndexList* matched_rules = style_resolver.PseudoCSSRulesForElement(
        element, pseudo_id, StyleResolver::kAllCSSRules);
    if (matched_rules && matched_rules->size()) {
      pseudo_id_matches->fromJust()->emplace_back(
          protocol::CSS::PseudoElementMatches::create()
              .setPseudoType(
                  InspectorDOMAgent::ProtocolPseudoElementType(pseudo_id))
              .setMatches(BuildArrayForMatchedRuleList(matched_rules, element,
                                                       pseudo_id))
              .build());
    }
  }

  // Inherited styles.
  *inherited_entries =
      std::make_unique<protocol::Array<protocol::CSS::InheritedStyleEntry>>();
  Element* parent_element = element->ParentOrShadowHostElement();
  while (parent_element) {
    StyleResolver& parent_style_resolver =
        parent_element->ownerDocument()->EnsureStyleResolver();
    RuleIndexList* parent_matched_rules =
        parent_style_resolver.CssRulesForElement(parent_element,
                                                 StyleResolver::kAllCSSRules);
    std::unique_ptr<protocol::CSS::InheritedStyleEntry> entry =
        protocol::CSS::InheritedStyleEntry::create()
            .setMatchedCSSRules(BuildArrayForMatchedRuleList(
                parent_matched_rules, parent_element, kPseudoIdNone))
            .build();
    if (parent_element->style() && parent_element->style()->length()) {
      InspectorStyleSheetForInlineStyle* style_sheet =
          AsInspectorStyleSheet(parent_element);
      if (style_sheet)
        entry->setInlineStyle(
            style_sheet->BuildObjectForStyle(style_sheet->InlineStyle()));
    }

    inherited_entries->fromJust()->emplace_back(std::move(entry));
    parent_element = parent_element->ParentOrShadowHostElement();
  }

  *css_keyframes_rules = AnimationsForNode(element);
  return Response::Success();
}

template <class CSSRuleCollection>
static CSSKeyframesRule* FindKeyframesRule(CSSRuleCollection* css_rules,
                                           StyleRuleKeyframes* keyframes_rule) {
  CSSKeyframesRule* result = nullptr;
  for (unsigned j = 0; css_rules && j < css_rules->length() && !result; ++j) {
    CSSRule* css_rule = css_rules->item(j);
    if (auto* css_style_rule = DynamicTo<CSSKeyframesRule>(css_rule)) {
      if (css_style_rule->Keyframes() == keyframes_rule)
        result = css_style_rule;
    } else if (auto* css_import_rule = DynamicTo<CSSImportRule>(css_rule)) {
      result = FindKeyframesRule(css_import_rule->styleSheet(), keyframes_rule);
    } else {
      result = FindKeyframesRule(css_rule->cssRules(), keyframes_rule);
    }
  }
  return result;
}

std::unique_ptr<protocol::Array<protocol::CSS::CSSKeyframesRule>>
InspectorCSSAgent::AnimationsForNode(Element* element) {
  auto css_keyframes_rules =
      std::make_unique<protocol::Array<protocol::CSS::CSSKeyframesRule>>();
  Document* owner_document = element->ownerDocument();

  owner_document->UpdateStyleAndLayoutTreeForNode(element);
  const ComputedStyle* style = element->EnsureComputedStyle();
  if (!style)
    return css_keyframes_rules;
  const CSSAnimationData* animation_data = style->Animations();
  StyleResolver& style_resolver = owner_document->EnsureStyleResolver();
  for (wtf_size_t i = 0;
       animation_data && i < animation_data->NameList().size(); ++i) {
    AtomicString animation_name(animation_data->NameList()[i]);
    if (animation_name == CSSAnimationData::InitialName())
      continue;
    StyleRuleKeyframes* keyframes_rule =
        style_resolver.FindKeyframesRule(element, animation_name);
    if (!keyframes_rule)
      continue;

    // Find CSSOM wrapper.
    CSSKeyframesRule* css_keyframes_rule = nullptr;
    for (CSSStyleSheet* style_sheet :
         *document_to_css_style_sheets_.at(owner_document)) {
      css_keyframes_rule = FindKeyframesRule(style_sheet, keyframes_rule);
      if (css_keyframes_rule)
        break;
    }
    if (!css_keyframes_rule)
      continue;

    auto keyframes =
        std::make_unique<protocol::Array<protocol::CSS::CSSKeyframeRule>>();
    for (unsigned j = 0; j < css_keyframes_rule->length(); ++j) {
      InspectorStyleSheet* inspector_style_sheet =
          BindStyleSheet(css_keyframes_rule->parentStyleSheet());
      keyframes->emplace_back(inspector_style_sheet->BuildObjectForKeyframeRule(
          css_keyframes_rule->Item(j)));
    }

    InspectorStyleSheet* inspector_style_sheet =
        BindStyleSheet(css_keyframes_rule->parentStyleSheet());
    CSSRuleSourceData* source_data =
        inspector_style_sheet->SourceDataForRule(css_keyframes_rule);
    std::unique_ptr<protocol::CSS::Value> name =
        protocol::CSS::Value::create()
            .setText(css_keyframes_rule->name())
            .build();
    if (source_data)
      name->setRange(inspector_style_sheet->BuildSourceRangeObject(
          source_data->rule_header_range));
    css_keyframes_rules->emplace_back(protocol::CSS::CSSKeyframesRule::create()
                                          .setAnimationName(std::move(name))
                                          .setKeyframes(std::move(keyframes))
                                          .build());
  }
  return css_keyframes_rules;
}

Response InspectorCSSAgent::getInlineStylesForNode(
    int node_id,
    Maybe<protocol::CSS::CSSStyle>* inline_style,
    Maybe<protocol::CSS::CSSStyle>* attributes_style) {
  Response response = AssertEnabled();
  if (!response.IsSuccess())
    return response;
  Element* element = nullptr;
  response = dom_agent_->AssertElement(node_id, element);
  if (!response.IsSuccess())
    return response;

  InspectorStyleSheetForInlineStyle* style_sheet =
      AsInspectorStyleSheet(element);
  if (!style_sheet)
    return Response::ServerError("Element is not a style sheet");

  *inline_style = style_sheet->BuildObjectForStyle(element->style());
  *attributes_style = BuildObjectForAttributesStyle(element);
  return Response::Success();
}

Response InspectorCSSAgent::getComputedStyleForNode(
    int node_id,
    std::unique_ptr<protocol::Array<protocol::CSS::CSSComputedStyleProperty>>*
        style) {
  Response response = AssertEnabled();
  if (!response.IsSuccess())
    return response;
  Node* node = nullptr;
  response = dom_agent_->AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  auto* computed_style_info =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(node, true);
  *style = std::make_unique<
      protocol::Array<protocol::CSS::CSSComputedStyleProperty>>();
  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& property_class =
        CSSProperty::Get(resolveCSSPropertyID(property_id));
    if (!property_class.IsWebExposed(node->GetExecutionContext()) ||
        property_class.IsShorthand() || !property_class.IsProperty())
      continue;
    (*style)->emplace_back(
        protocol::CSS::CSSComputedStyleProperty::create()
            .setName(property_class.GetPropertyNameString())
            .setValue(computed_style_info->GetPropertyValue(property_id))
            .build());
  }

  for (const auto& it : computed_style_info->GetVariables()) {
    (*style)->emplace_back(protocol::CSS::CSSComputedStyleProperty::create()
                               .setName(it.key)
                               .setValue(it.value->CssText())
                               .build());
  }
  return Response::Success();
}

void InspectorCSSAgent::CollectPlatformFontsForLayoutObject(
    LayoutObject* layout_object,
    HashCountedSet<std::pair<int, String>>* font_stats,
    unsigned descendants_depth) {
  if (!layout_object->IsText()) {
    if (!descendants_depth)
      return;

    // Skip recursing inside a display-locked tree.
    if (DisplayLockUtilities::NearestLockedInclusiveAncestor(*layout_object))
      return;

    if (!layout_object->IsAnonymous())
      --descendants_depth;
    for (LayoutObject* child = layout_object->SlowFirstChild(); child;
         child = child->NextSibling()) {
      CollectPlatformFontsForLayoutObject(child, font_stats, descendants_depth);
    }
    return;
  }

  // Don't gather text on a display-locked tree.
  if (DisplayLockUtilities::NearestLockedExclusiveAncestor(*layout_object))
    return;

  FontCachePurgePreventer preventer;
  LayoutText* layout_text = ToLayoutText(layout_object);

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    if (layout_object->IsInLayoutNGInlineFormattingContext()) {
      NGInlineCursor cursor;
      cursor.MoveTo(*layout_object);
      for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
        const ShapeResultView* shape_result =
            cursor.Current().TextShapeResult();
        if (!shape_result)
          continue;
        Vector<ShapeResult::RunFontData> run_font_data_list;
        shape_result->GetRunFontData(&run_font_data_list);
        CollectPlatformFontsFromRunFontDataList(run_font_data_list, font_stats);
      }
      return;
    }
    // If !IsInLayoutNGInlineFormattingContext, the LayoutText is in legacy
    // inline formatting context. Fallback to InlineTextBox code below.
  }

  for (InlineTextBox* box : layout_text->TextBoxes()) {
    const ComputedStyle& style = layout_text->StyleRef(box->IsFirstLineStyle());
    const Font& font = style.GetFont();
    TextRun run = box->ConstructTextRunForInspector(style);
    CachingWordShaper shaper(font);
    CollectPlatformFontsFromRunFontDataList(shaper.GetRunFontData(run),
                                            font_stats);
  }
}

Response InspectorCSSAgent::getPlatformFontsForNode(
    int node_id,
    std::unique_ptr<protocol::Array<protocol::CSS::PlatformFontUsage>>*
        platform_fonts) {
  Response response = AssertEnabled();
  if (!response.IsSuccess())
    return response;
  Node* node = nullptr;
  response = dom_agent_->AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  HashCountedSet<std::pair<int, String>> font_stats;
  LayoutObject* root = node->GetLayoutObject();
  if (root) {
    // Iterate upto two layers deep.
    const unsigned descendants_depth = 2;
    CollectPlatformFontsForLayoutObject(root, &font_stats, descendants_depth);
  }
  *platform_fonts =
      std::make_unique<protocol::Array<protocol::CSS::PlatformFontUsage>>();
  for (auto& font : font_stats) {
    std::pair<int, String>& font_description = font.key;
    bool is_custom_font = font_description.first == 1;
    String font_name = font_description.second;
    (*platform_fonts)
        ->emplace_back(protocol::CSS::PlatformFontUsage::create()
                           .setFamilyName(font_name)
                           .setIsCustomFont(is_custom_font)
                           .setGlyphCount(font.value)
                           .build());
  }
  return Response::Success();
}

Response InspectorCSSAgent::getStyleSheetText(const String& style_sheet_id,
                                              String* result) {
  InspectorStyleSheetBase* inspector_style_sheet = nullptr;
  Response response =
      AssertStyleSheetForId(style_sheet_id, inspector_style_sheet);
  if (!response.IsSuccess())
    return response;

  inspector_style_sheet->GetText(result);
  return Response::Success();
}

Response InspectorCSSAgent::collectClassNames(
    const String& style_sheet_id,
    std::unique_ptr<protocol::Array<String>>* class_names) {
  InspectorStyleSheet* inspector_style_sheet = nullptr;
  Response response =
      AssertInspectorStyleSheetForId(style_sheet_id, inspector_style_sheet);
  if (!response.IsSuccess())
    return response;
  *class_names = inspector_style_sheet->CollectClassNames();
  return Response::Success();
}

Response InspectorCSSAgent::setStyleSheetText(
    const String& style_sheet_id,
    const String& text,
    protocol::Maybe<String>* source_map_url) {
  FrontendOperationScope scope;
  InspectorStyleSheetBase* inspector_style_sheet = nullptr;
  Response response =
      AssertStyleSheetForId(style_sheet_id, inspector_style_sheet);
  if (!response.IsSuccess())
    return response;

  DummyExceptionStateForTesting exception_state;
  dom_agent_->History()->Perform(MakeGarbageCollected<SetStyleSheetTextAction>(
                                     inspector_style_sheet, text),
                                 exception_state);
  response = InspectorDOMAgent::ToResponse(exception_state);
  if (!response.IsSuccess())
    return response;
  if (!inspector_style_sheet->SourceMapURL().IsEmpty())
    *source_map_url = inspector_style_sheet->SourceMapURL();
  return Response::Success();
}

static Response JsonRangeToSourceRange(
    InspectorStyleSheetBase* inspector_style_sheet,
    protocol::CSS::SourceRange* range,
    SourceRange* source_range) {
  if (range->getStartLine() < 0) {
    return Response::ServerError(
        "range.startLine must be a non-negative integer");
  }
  if (range->getStartColumn() < 0) {
    return Response::ServerError(
        "range.startColumn must be a non-negative integer");
  }
  if (range->getEndLine() < 0) {
    return Response::ServerError(
        "range.endLine must be a non-negative integer");
  }
  if (range->getEndColumn() < 0) {
    return Response::ServerError(
        "range.endColumn must be a non-negative integer");
  }

  unsigned start_offset = 0;
  unsigned end_offset = 0;
  bool success =
      inspector_style_sheet->LineNumberAndColumnToOffset(
          range->getStartLine(), range->getStartColumn(), &start_offset) &&
      inspector_style_sheet->LineNumberAndColumnToOffset(
          range->getEndLine(), range->getEndColumn(), &end_offset);
  if (!success)
    return Response::ServerError("Specified range is out of bounds");

  if (start_offset > end_offset)
    return Response::ServerError("Range start must not succeed its end");
  source_range->start = start_offset;
  source_range->end = end_offset;
  return Response::Success();
}

Response InspectorCSSAgent::setRuleSelector(
    const String& style_sheet_id,
    std::unique_ptr<protocol::CSS::SourceRange> range,
    const String& selector,
    std::unique_ptr<protocol::CSS::SelectorList>* result) {
  FrontendOperationScope scope;
  InspectorStyleSheet* inspector_style_sheet = nullptr;
  Response response =
      AssertInspectorStyleSheetForId(style_sheet_id, inspector_style_sheet);
  if (!response.IsSuccess())
    return response;
  SourceRange selector_range;
  response = JsonRangeToSourceRange(inspector_style_sheet, range.get(),
                                    &selector_range);
  if (!response.IsSuccess())
    return response;

  DummyExceptionStateForTesting exception_state;
  ModifyRuleAction* action = MakeGarbageCollected<ModifyRuleAction>(
      ModifyRuleAction::kSetRuleSelector, inspector_style_sheet, selector_range,
      selector);
  bool success = dom_agent_->History()->Perform(action, exception_state);
  if (success) {
    CSSStyleRule* rule = InspectorCSSAgent::AsCSSStyleRule(action->TakeRule());
    InspectorStyleSheet* inspector_style_sheet =
        InspectorStyleSheetForRule(rule);
    if (!inspector_style_sheet) {
      return Response::ServerError(
          "Failed to get inspector style sheet for rule.");
    }
    *result = inspector_style_sheet->BuildObjectForSelectorList(rule);
  }
  return InspectorDOMAgent::ToResponse(exception_state);
}

Response InspectorCSSAgent::setKeyframeKey(
    const String& style_sheet_id,
    std::unique_ptr<protocol::CSS::SourceRange> range,
    const String& key_text,
    std::unique_ptr<protocol::CSS::Value>* result) {
  FrontendOperationScope scope;
  InspectorStyleSheet* inspector_style_sheet = nullptr;
  Response response =
      AssertInspectorStyleSheetForId(style_sheet_id, inspector_style_sheet);
  if (!response.IsSuccess())
    return response;
  SourceRange key_range;
  response =
      JsonRangeToSourceRange(inspector_style_sheet, range.get(), &key_range);
  if (!response.IsSuccess())
    return response;

  DummyExceptionStateForTesting exception_state;
  ModifyRuleAction* action = MakeGarbageCollected<ModifyRuleAction>(
      ModifyRuleAction::kSetKeyframeKey, inspector_style_sheet, key_range,
      key_text);
  bool success = dom_agent_->History()->Perform(action, exception_state);
  if (success) {
    auto* rule = To<CSSKeyframeRule>(action->TakeRule());
    InspectorStyleSheet* inspector_style_sheet =
        BindStyleSheet(rule->parentStyleSheet());
    if (!inspector_style_sheet) {
      return Response::ServerError(
          "Failed to get inspector style sheet for rule.");
    }
    CSSRuleSourceData* source_data =
        inspector_style_sheet->SourceDataForRule(rule);
    *result = protocol::CSS::Value::create()
                  .setText(rule->keyText())
                  .setRange(inspector_style_sheet->BuildSourceRangeObject(
                      source_data->rule_header_range))
                  .build();
  }
  return InspectorDOMAgent::ToResponse(exception_state);
}

Response InspectorCSSAgent::MultipleStyleTextsActions(
    std::unique_ptr<protocol::Array<protocol::CSS::StyleDeclarationEdit>> edits,
    HeapVector<Member<StyleSheetAction>>* actions) {
  size_t n = edits->size();
  if (n == 0)
    return Response::ServerError("Edits should not be empty");

  for (size_t i = 0; i < n; ++i) {
    protocol::CSS::StyleDeclarationEdit* edit = (*edits)[i].get();
    InspectorStyleSheetBase* inspector_style_sheet = nullptr;
    Response response =
        AssertStyleSheetForId(edit->getStyleSheetId(), inspector_style_sheet);
    if (!response.IsSuccess()) {
      return Response::ServerError(
          String::Format("StyleSheet not found for edit #%zu of %zu", i + 1, n)
              .Utf8());
    }

    SourceRange range;
    response =
        JsonRangeToSourceRange(inspector_style_sheet, edit->getRange(), &range);
    if (!response.IsSuccess())
      return response;

    if (inspector_style_sheet->IsInlineStyle()) {
      InspectorStyleSheetForInlineStyle* inline_style_sheet =
          static_cast<InspectorStyleSheetForInlineStyle*>(
              inspector_style_sheet);
      SetElementStyleAction* action =
          MakeGarbageCollected<SetElementStyleAction>(inline_style_sheet,
                                                      edit->getText());
      actions->push_back(action);
    } else {
      ModifyRuleAction* action = MakeGarbageCollected<ModifyRuleAction>(
          ModifyRuleAction::kSetStyleText,
          static_cast<InspectorStyleSheet*>(inspector_style_sheet), range,
          edit->getText());
      actions->push_back(action);
    }
  }
  return Response::Success();
}

Response InspectorCSSAgent::setStyleTexts(
    std::unique_ptr<protocol::Array<protocol::CSS::StyleDeclarationEdit>> edits,
    std::unique_ptr<protocol::Array<protocol::CSS::CSSStyle>>* result) {
  FrontendOperationScope scope;
  HeapVector<Member<StyleSheetAction>> actions;
  Response response = MultipleStyleTextsActions(std::move(edits), &actions);
  if (!response.IsSuccess())
    return response;

  DummyExceptionStateForTesting exception_state;

  int n = actions.size();
  auto serialized_styles =
      std::make_unique<protocol::Array<protocol::CSS::CSSStyle>>();
  for (int i = 0; i < n; ++i) {
    Member<StyleSheetAction> action = actions.at(i);
    bool success = action->Perform(exception_state);
    if (!success) {
      for (int j = i - 1; j >= 0; --j) {
        Member<StyleSheetAction> revert = actions.at(j);
        DummyExceptionStateForTesting undo_exception_state;
        revert->Undo(undo_exception_state);
        DCHECK(!undo_exception_state.HadException());
      }
      return Response::ServerError(
          String::Format("Failed applying edit #%d: ", i).Utf8() +
          InspectorDOMAgent::ToResponse(exception_state).Message());
    }
    serialized_styles->emplace_back(action->TakeSerializedStyle());
  }

  for (int i = 0; i < n; ++i) {
    Member<StyleSheetAction> action = actions.at(i);
    dom_agent_->History()->AppendPerformedAction(action);
  }
  *result = std::move(serialized_styles);
  return Response::Success();
}

Response InspectorCSSAgent::SetStyleText(
    InspectorStyleSheetBase* inspector_style_sheet,
    const SourceRange& range,
    const String& text,
    CSSStyleDeclaration*& result) {
  DummyExceptionStateForTesting exception_state;
  if (inspector_style_sheet->IsInlineStyle()) {
    InspectorStyleSheetForInlineStyle* inline_style_sheet =
        static_cast<InspectorStyleSheetForInlineStyle*>(inspector_style_sheet);
    SetElementStyleAction* action =
        MakeGarbageCollected<SetElementStyleAction>(inline_style_sheet, text);
    bool success = dom_agent_->History()->Perform(action, exception_state);
    if (success) {
      result = inline_style_sheet->InlineStyle();
      return Response::Success();
    }
  } else {
    ModifyRuleAction* action = MakeGarbageCollected<ModifyRuleAction>(
        ModifyRuleAction::kSetStyleText,
        static_cast<InspectorStyleSheet*>(inspector_style_sheet), range, text);
    bool success = dom_agent_->History()->Perform(action, exception_state);
    if (success) {
      CSSRule* rule = action->TakeRule();
      if (auto* style_rule = DynamicTo<CSSStyleRule>(rule)) {
        result = style_rule->style();
        return Response::Success();
      }
      if (auto* keyframe_rule = DynamicTo<CSSKeyframeRule>(rule)) {
        result = keyframe_rule->style();
        return Response::Success();
      }
    }
  }
  return InspectorDOMAgent::ToResponse(exception_state);
}

Response InspectorCSSAgent::setMediaText(
    const String& style_sheet_id,
    std::unique_ptr<protocol::CSS::SourceRange> range,
    const String& text,
    std::unique_ptr<protocol::CSS::CSSMedia>* result) {
  FrontendOperationScope scope;
  InspectorStyleSheet* inspector_style_sheet = nullptr;
  Response response =
      AssertInspectorStyleSheetForId(style_sheet_id, inspector_style_sheet);
  if (!response.IsSuccess())
    return response;
  SourceRange text_range;
  response =
      JsonRangeToSourceRange(inspector_style_sheet, range.get(), &text_range);
  if (!response.IsSuccess())
    return response;

  DummyExceptionStateForTesting exception_state;
  ModifyRuleAction* action = MakeGarbageCollected<ModifyRuleAction>(
      ModifyRuleAction::kSetMediaRuleText, inspector_style_sheet, text_range,
      text);
  bool success = dom_agent_->History()->Perform(action, exception_state);
  if (success) {
    CSSMediaRule* rule = InspectorCSSAgent::AsCSSMediaRule(action->TakeRule());
    String source_url = rule->parentStyleSheet()->Contents()->BaseURL();
    if (source_url.IsEmpty())
      source_url = InspectorDOMAgent::DocumentURLString(
          rule->parentStyleSheet()->OwnerDocument());
    *result = BuildMediaObject(rule->media(), kMediaListSourceMediaRule,
                               source_url, rule->parentStyleSheet());
  }
  return InspectorDOMAgent::ToResponse(exception_state);
}

Response InspectorCSSAgent::createStyleSheet(
    const String& frame_id,
    protocol::CSS::StyleSheetId* out_style_sheet_id) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  if (!frame)
    return Response::ServerError("Frame not found");

  Document* document = frame->GetDocument();
  if (!document)
    return Response::ServerError("Frame does not have a document");

  InspectorStyleSheet* inspector_style_sheet = ViaInspectorStyleSheet(document);
  if (!inspector_style_sheet)
    return Response::ServerError("No target stylesheet found");

  UpdateActiveStyleSheets(document);

  *out_style_sheet_id = inspector_style_sheet->Id();
  return Response::Success();
}

Response InspectorCSSAgent::addRule(
    const String& style_sheet_id,
    const String& rule_text,
    std::unique_ptr<protocol::CSS::SourceRange> location,
    std::unique_ptr<protocol::CSS::CSSRule>* result) {
  FrontendOperationScope scope;
  InspectorStyleSheet* inspector_style_sheet = nullptr;
  Response response =
      AssertInspectorStyleSheetForId(style_sheet_id, inspector_style_sheet);
  if (!response.IsSuccess())
    return response;
  SourceRange rule_location;
  response = JsonRangeToSourceRange(inspector_style_sheet, location.get(),
                                    &rule_location);
  if (!response.IsSuccess())
    return response;

  DummyExceptionStateForTesting exception_state;
  AddRuleAction* action = MakeGarbageCollected<AddRuleAction>(
      inspector_style_sheet, rule_text, rule_location);
  bool success = dom_agent_->History()->Perform(action, exception_state);
  if (!success)
    return InspectorDOMAgent::ToResponse(exception_state);

  CSSStyleRule* rule = action->TakeRule();
  *result = BuildObjectForRule(rule);
  return Response::Success();
}

Response InspectorCSSAgent::forcePseudoState(
    int node_id,
    std::unique_ptr<protocol::Array<String>> forced_pseudo_classes) {
  Response response = AssertEnabled();
  if (!response.IsSuccess())
    return response;
  Element* element = nullptr;
  response = dom_agent_->AssertElement(node_id, element);
  if (!response.IsSuccess())
    return response;

  unsigned forced_pseudo_state =
      ComputePseudoClassMask(std::move(forced_pseudo_classes));
  NodeIdToForcedPseudoState::iterator it =
      node_id_to_forced_pseudo_state_.find(node_id);
  unsigned current_forced_pseudo_state =
      it == node_id_to_forced_pseudo_state_.end() ? 0 : it->value;
  bool need_style_recalc = forced_pseudo_state != current_forced_pseudo_state;
  if (!need_style_recalc)
    return Response::Success();

  if (forced_pseudo_state)
    node_id_to_forced_pseudo_state_.Set(node_id, forced_pseudo_state);
  else
    node_id_to_forced_pseudo_state_.erase(node_id);

  // When adding focus or focus-within, we force focus-within for ancestor
  // nodes to emulate real focus for user convenience.

  // Flips from no forced focus to the forced focus (:focus or :focus-within).
  if (((forced_pseudo_state & kPseudoFocus) == kPseudoFocus &&
       (current_forced_pseudo_state & kPseudoFocus) == 0) ||
      ((forced_pseudo_state & kPseudoFocusWithin) == kPseudoFocusWithin &&
       (current_forced_pseudo_state & kPseudoFocusWithin) == 0)) {
    IncrementFocusedCountForAncestors(element);
  }

  // Flips from the forced focus (:focus or :focus-within) to no focus.
  if (((forced_pseudo_state & kPseudoFocus) == 0 &&
       (current_forced_pseudo_state & kPseudoFocus) == kPseudoFocus) ||
      ((forced_pseudo_state & kPseudoFocusWithin) == 0 &&
       (current_forced_pseudo_state & kPseudoFocusWithin) ==
           kPseudoFocusWithin)) {
    DecrementFocusedCountForAncestors(element);
  }

  element->ownerDocument()->GetStyleEngine().MarkAllElementsForStyleRecalc(
      StyleChangeReasonForTracing::Create(style_change_reason::kInspector));
  return Response::Success();
}

void InspectorCSSAgent::IncrementFocusedCountForAncestors(Element* element) {
  for (Node& ancestor : FlatTreeTraversal::AncestorsOf(*element)) {
    int node_id = dom_agent_->BoundNodeId(&ancestor);
    if (!node_id)
      continue;
    NodeIdToNumberFocusedChildren::iterator it =
        node_id_to_number_focused_children_.find(node_id);
    unsigned count =
        it == node_id_to_number_focused_children_.end() ? 0 : it->value;
    node_id_to_number_focused_children_.Set(node_id, count + 1);
  }
}

void InspectorCSSAgent::DecrementFocusedCountForAncestors(Element* element) {
  for (Node& ancestor : FlatTreeTraversal::AncestorsOf(*element)) {
    int node_id = dom_agent_->BoundNodeId(&ancestor);
    if (!node_id)
      continue;
    NodeIdToNumberFocusedChildren::iterator it =
        node_id_to_number_focused_children_.find(node_id);
    unsigned count =
        it == node_id_to_number_focused_children_.end() ? 1 : it->value;
    if (count <= 1) {
      // If `count - 1` is zero or overflows, erase the node_id
      // from the map to save memory. If there is zero focused child
      // elements, :focus-within should not be forced.
      node_id_to_number_focused_children_.erase(node_id);
    } else {
      node_id_to_number_focused_children_.Set(node_id, count - 1);
    }
  }
}

std::unique_ptr<protocol::CSS::CSSMedia> InspectorCSSAgent::BuildMediaObject(
    const MediaList* media,
    MediaListSource media_list_source,
    const String& source_url,
    CSSStyleSheet* parent_style_sheet) {
  // Make certain compilers happy by initializing |source| up-front.
  String source = protocol::CSS::CSSMedia::SourceEnum::InlineSheet;
  switch (media_list_source) {
    case kMediaListSourceMediaRule:
      source = protocol::CSS::CSSMedia::SourceEnum::MediaRule;
      break;
    case kMediaListSourceImportRule:
      source = protocol::CSS::CSSMedia::SourceEnum::ImportRule;
      break;
    case kMediaListSourceLinkedSheet:
      source = protocol::CSS::CSSMedia::SourceEnum::LinkedSheet;
      break;
    case kMediaListSourceInlineSheet:
      source = protocol::CSS::CSSMedia::SourceEnum::InlineSheet;
      break;
  }

  const MediaQuerySet* queries = media->Queries();
  const Vector<std::unique_ptr<MediaQuery>>& query_vector =
      queries->QueryVector();
  LocalFrame* frame = nullptr;
  if (parent_style_sheet) {
    if (Document* document = parent_style_sheet->OwnerDocument())
      frame = document->GetFrame();
  }
  MediaQueryEvaluator* media_evaluator =
      MakeGarbageCollected<MediaQueryEvaluator>(frame);

  InspectorStyleSheet* inspector_style_sheet =
      parent_style_sheet
          ? css_style_sheet_to_inspector_style_sheet_.at(parent_style_sheet)
          : nullptr;
  auto media_list_array =
      std::make_unique<protocol::Array<protocol::CSS::MediaQuery>>();
  MediaValues* media_values = MediaValues::CreateDynamicIfFrameExists(frame);
  bool has_media_query_items = false;
  for (wtf_size_t i = 0; i < query_vector.size(); ++i) {
    MediaQuery& query = *query_vector.at(i);
    const ExpressionHeapVector& expressions = query.Expressions();
    auto expression_array = std::make_unique<
        protocol::Array<protocol::CSS::MediaQueryExpression>>();
    bool has_expression_items = false;
    for (wtf_size_t j = 0; j < expressions.size(); ++j) {
      const MediaQueryExp& media_query_exp = expressions.at(j);
      MediaQueryExpValue exp_value = media_query_exp.ExpValue();
      if (!exp_value.is_value)
        continue;
      const char* value_name =
          CSSPrimitiveValue::UnitTypeToString(exp_value.unit);
      std::unique_ptr<protocol::CSS::MediaQueryExpression>
          media_query_expression =
              protocol::CSS::MediaQueryExpression::create()
                  .setValue(exp_value.value)
                  .setUnit(String(value_name))
                  .setFeature(media_query_exp.MediaFeature())
                  .build();

      if (inspector_style_sheet && media->ParentRule())
        media_query_expression->setValueRange(
            inspector_style_sheet->MediaQueryExpValueSourceRange(
                media->ParentRule(), i, j));

      int computed_length;
      if (media_values->ComputeLength(exp_value.value, exp_value.unit,
                                      computed_length))
        media_query_expression->setComputedLength(computed_length);

      expression_array->emplace_back(std::move(media_query_expression));
      has_expression_items = true;
    }
    if (!has_expression_items)
      continue;
    std::unique_ptr<protocol::CSS::MediaQuery> media_query =
        protocol::CSS::MediaQuery::create()
            .setActive(media_evaluator->Eval(query, nullptr))
            .setExpressions(std::move(expression_array))
            .build();
    media_list_array->emplace_back(std::move(media_query));
    has_media_query_items = true;
  }

  // The |mediaText()| getter does not require an ExecutionContext as it is
  // only used for setting/parsing new media queries and features.
  std::unique_ptr<protocol::CSS::CSSMedia> media_object =
      protocol::CSS::CSSMedia::create()
          .setText(media->mediaText(/*execution_context=*/nullptr))
          .setSource(source)
          .build();
  if (has_media_query_items)
    media_object->setMediaList(std::move(media_list_array));

  if (inspector_style_sheet && media_list_source != kMediaListSourceLinkedSheet)
    media_object->setStyleSheetId(inspector_style_sheet->Id());

  if (!source_url.IsEmpty()) {
    media_object->setSourceURL(source_url);

    CSSRule* parent_rule = media->ParentRule();
    if (!parent_rule)
      return media_object;
    InspectorStyleSheet* inspector_style_sheet =
        BindStyleSheet(parent_rule->parentStyleSheet());
    media_object->setRange(
        inspector_style_sheet->RuleHeaderSourceRange(parent_rule));
  }
  return media_object;
}

void InspectorCSSAgent::CollectMediaQueriesFromStyleSheet(
    CSSStyleSheet* style_sheet,
    protocol::Array<protocol::CSS::CSSMedia>* media_array) {
  MediaList* media_list = style_sheet->media();
  String source_url;
  if (media_list && media_list->length()) {
    Document* doc = style_sheet->OwnerDocument();
    if (doc)
      source_url = doc->Url();
    else if (!style_sheet->Contents()->BaseURL().IsEmpty())
      source_url = style_sheet->Contents()->BaseURL();
    else
      source_url = "";
    media_array->emplace_back(
        BuildMediaObject(media_list,
                         style_sheet->ownerNode() ? kMediaListSourceLinkedSheet
                                                  : kMediaListSourceInlineSheet,
                         source_url, style_sheet));
  }
}

void InspectorCSSAgent::CollectMediaQueriesFromRule(
    CSSRule* rule,
    protocol::Array<protocol::CSS::CSSMedia>* media_array) {
  MediaList* media_list;
  String source_url;
  CSSStyleSheet* parent_style_sheet = nullptr;
  bool is_media_rule = true;
  if (auto* media_rule = DynamicTo<CSSMediaRule>(rule)) {
    media_list = media_rule->media();
    parent_style_sheet = media_rule->parentStyleSheet();
  } else if (auto* import_rule = DynamicTo<CSSImportRule>(rule)) {
    media_list = import_rule->media();
    parent_style_sheet = import_rule->parentStyleSheet();
    is_media_rule = false;
  } else {
    media_list = nullptr;
  }

  if (parent_style_sheet) {
    source_url = parent_style_sheet->Contents()->BaseURL();
    if (source_url.IsEmpty())
      source_url = InspectorDOMAgent::DocumentURLString(
          parent_style_sheet->OwnerDocument());
  } else {
    source_url = "";
  }

  if (media_list && media_list->length())
    media_array->emplace_back(BuildMediaObject(
        media_list,
        is_media_rule ? kMediaListSourceMediaRule : kMediaListSourceImportRule,
        source_url, parent_style_sheet));
}

std::unique_ptr<protocol::Array<protocol::CSS::CSSMedia>>
InspectorCSSAgent::BuildMediaListChain(CSSRule* rule) {
  if (!rule)
    return nullptr;
  auto media_array =
      std::make_unique<protocol::Array<protocol::CSS::CSSMedia>>();
  CSSRule* parent_rule = rule;
  while (parent_rule) {
    CollectMediaQueriesFromRule(parent_rule, media_array.get());
    if (parent_rule->parentRule()) {
      parent_rule = parent_rule->parentRule();
    } else {
      CSSStyleSheet* style_sheet = parent_rule->parentStyleSheet();
      while (style_sheet) {
        CollectMediaQueriesFromStyleSheet(style_sheet, media_array.get());
        parent_rule = style_sheet->ownerRule();
        if (parent_rule)
          break;
        style_sheet = style_sheet->parentStyleSheet();
      }
    }
  }
  return media_array;
}

InspectorStyleSheetForInlineStyle* InspectorCSSAgent::AsInspectorStyleSheet(
    Element* element) {
  NodeToInspectorStyleSheet::iterator it =
      node_to_inspector_style_sheet_.find(element);
  if (it != node_to_inspector_style_sheet_.end())
    return it->value.Get();

  CSSStyleDeclaration* style = element->style();
  if (!style)
    return nullptr;

  InspectorStyleSheetForInlineStyle* inspector_style_sheet =
      MakeGarbageCollected<InspectorStyleSheetForInlineStyle>(element, this);
  id_to_inspector_style_sheet_for_inline_style_.Set(inspector_style_sheet->Id(),
                                                    inspector_style_sheet);
  node_to_inspector_style_sheet_.Set(element, inspector_style_sheet);
  return inspector_style_sheet;
}

// static
void InspectorCSSAgent::CollectAllDocumentStyleSheets(
    Document* document,
    HeapVector<Member<CSSStyleSheet>>& result) {
  for (const auto& style :
       document->GetStyleEngine().ActiveStyleSheetsForInspector())
    InspectorCSSAgent::CollectStyleSheets(style.first, result);
}

// static
void InspectorCSSAgent::CollectStyleSheets(
    CSSStyleSheet* style_sheet,
    HeapVector<Member<CSSStyleSheet>>& result) {
  result.push_back(style_sheet);
  for (unsigned i = 0, size = style_sheet->length(); i < size; ++i) {
    CSSRule* rule = style_sheet->item(i);
    if (auto* import_rule = DynamicTo<CSSImportRule>(rule)) {
      CSSStyleSheet* imported_style_sheet = import_rule->styleSheet();
      if (imported_style_sheet)
        InspectorCSSAgent::CollectStyleSheets(imported_style_sheet, result);
    }
  }
}

InspectorStyleSheet* InspectorCSSAgent::BindStyleSheet(
    CSSStyleSheet* style_sheet) {
  InspectorStyleSheet* inspector_style_sheet =
      css_style_sheet_to_inspector_style_sheet_.at(style_sheet);
  if (!inspector_style_sheet) {
    Document* document = style_sheet->OwnerDocument();
    inspector_style_sheet = MakeGarbageCollected<InspectorStyleSheet>(
        network_agent_, style_sheet, DetectOrigin(style_sheet, document),
        InspectorDOMAgent::DocumentURLString(document), this,
        resource_container_);
    id_to_inspector_style_sheet_.Set(inspector_style_sheet->Id(),
                                     inspector_style_sheet);
    css_style_sheet_to_inspector_style_sheet_.Set(style_sheet,
                                                  inspector_style_sheet);
  }
  return inspector_style_sheet;
}

String InspectorCSSAgent::StyleSheetId(CSSStyleSheet* style_sheet) {
  return BindStyleSheet(style_sheet)->Id();
}

String InspectorCSSAgent::UnbindStyleSheet(
    InspectorStyleSheet* inspector_style_sheet) {
  String id = inspector_style_sheet->Id();
  id_to_inspector_style_sheet_.erase(id);
  if (inspector_style_sheet->PageStyleSheet())
    css_style_sheet_to_inspector_style_sheet_.erase(
        inspector_style_sheet->PageStyleSheet());
  return id;
}

InspectorStyleSheet* InspectorCSSAgent::InspectorStyleSheetForRule(
    CSSStyleRule* rule) {
  if (!rule)
    return nullptr;

  // CSSRules returned by StyleResolver::pseudoCSSRulesForElement lack parent
  // pointers if they are coming from user agent stylesheets. To work around
  // this issue, we use CSSOM wrapper created by inspector.
  if (!rule->parentStyleSheet()) {
    if (!inspector_user_agent_style_sheet_)
      inspector_user_agent_style_sheet_ = MakeGarbageCollected<CSSStyleSheet>(
          CSSDefaultStyleSheets::Instance().DefaultStyleSheet());
    rule->SetParentStyleSheet(inspector_user_agent_style_sheet_.Get());
  }
  return BindStyleSheet(rule->parentStyleSheet());
}

InspectorStyleSheet* InspectorCSSAgent::ViaInspectorStyleSheet(
    Document* document) {
  if (!document)
    return nullptr;

  if (!IsA<HTMLDocument>(document) && !document->IsSVGDocument())
    return nullptr;

  CSSStyleSheet& inspector_sheet =
      document->GetStyleEngine().EnsureInspectorStyleSheet();

  FlushPendingProtocolNotifications();

  return css_style_sheet_to_inspector_style_sheet_.at(&inspector_sheet);
}

Response InspectorCSSAgent::AssertEnabled() {
  return enable_completed_ ? Response::Success()
                           : Response::ServerError("CSS agent was not enabled");
}

Response InspectorCSSAgent::AssertInspectorStyleSheetForId(
    const String& style_sheet_id,
    InspectorStyleSheet*& result) {
  Response response = AssertEnabled();
  if (!response.IsSuccess())
    return response;
  IdToInspectorStyleSheet::iterator it =
      id_to_inspector_style_sheet_.find(style_sheet_id);
  if (it == id_to_inspector_style_sheet_.end())
    return Response::ServerError("No style sheet with given id found");
  result = it->value.Get();
  return Response::Success();
}

Response InspectorCSSAgent::AssertStyleSheetForId(
    const String& style_sheet_id,
    InspectorStyleSheetBase*& result) {
  InspectorStyleSheet* style_sheet = nullptr;
  Response response =
      AssertInspectorStyleSheetForId(style_sheet_id, style_sheet);
  if (response.IsSuccess()) {
    result = style_sheet;
    return response;
  }
  IdToInspectorStyleSheetForInlineStyle::iterator it =
      id_to_inspector_style_sheet_for_inline_style_.find(style_sheet_id);
  if (it == id_to_inspector_style_sheet_for_inline_style_.end())
    return Response::ServerError("No style sheet with given id found");
  result = it->value.Get();
  return Response::Success();
}

protocol::CSS::StyleSheetOrigin InspectorCSSAgent::DetectOrigin(
    CSSStyleSheet* page_style_sheet,
    Document* owner_document) {
  DCHECK(page_style_sheet);

  if (!page_style_sheet->ownerNode() && page_style_sheet->href().IsEmpty() &&
      !page_style_sheet->IsConstructed())
    return protocol::CSS::StyleSheetOriginEnum::UserAgent;

  if (page_style_sheet->ownerNode() &&
      page_style_sheet->ownerNode()->IsDocumentNode()) {
    if (page_style_sheet ==
        owner_document->GetStyleEngine().InspectorStyleSheet())
      return protocol::CSS::StyleSheetOriginEnum::Inspector;
    return protocol::CSS::StyleSheetOriginEnum::Injected;
  }
  return protocol::CSS::StyleSheetOriginEnum::Regular;
}

std::unique_ptr<protocol::CSS::CSSRule> InspectorCSSAgent::BuildObjectForRule(
    CSSStyleRule* rule) {
  InspectorStyleSheet* inspector_style_sheet = InspectorStyleSheetForRule(rule);
  if (!inspector_style_sheet)
    return nullptr;

  std::unique_ptr<protocol::CSS::CSSRule> result =
      inspector_style_sheet->BuildObjectForRuleWithoutMedia(rule);
  result->setMedia(BuildMediaListChain(rule));
  return result;
}

std::unique_ptr<protocol::Array<protocol::CSS::RuleMatch>>
InspectorCSSAgent::BuildArrayForMatchedRuleList(
    RuleIndexList* rule_list,
    Element* element,
    PseudoId matches_for_pseudo_id) {
  auto result = std::make_unique<protocol::Array<protocol::CSS::RuleMatch>>();
  if (!rule_list)
    return result;

  // Dedupe matches coming from the same rule source.
  HeapVector<Member<CSSStyleRule>> uniq_rules;
  HeapHashSet<Member<CSSRule>> uniq_rules_set;
  HeapHashMap<Member<CSSStyleRule>, std::unique_ptr<Vector<unsigned>>>
      rule_indices;
  for (auto it = rule_list->rbegin(); it != rule_list->rend(); ++it) {
    CSSRule* rule = it->first;
    auto* style_rule = DynamicTo<CSSStyleRule>(rule);
    if (!style_rule)
      continue;
    if (!uniq_rules_set.Contains(rule)) {
      uniq_rules_set.insert(rule);
      uniq_rules.push_back(style_rule);
      rule_indices.Set(style_rule, std::make_unique<Vector<unsigned>>());
    }
    rule_indices.at(style_rule)->push_back(it->second);
  }

  for (auto it = uniq_rules.rbegin(); it != uniq_rules.rend(); ++it) {
    CSSStyleRule* rule = it->Get();
    std::unique_ptr<protocol::CSS::CSSRule> rule_object =
        BuildObjectForRule(rule);
    if (!rule_object)
      continue;

    // Transform complex rule_indices into client-friendly, compound-basis for
    // matching_selectors.
    // e.g. ".foo + .bar, h1, body h1" for <h1>
    //  (complex): {.foo: 0, .bar: 1, h1: 2, body: 3, h1: 4}, matches: [2, 4]
    // (compound): {.foo: 0, .bar: 0, h1: 1, body: 2, h1: 2}, matches: [1, 2]
    auto matching_selectors = std::make_unique<protocol::Array<int>>();
    if (rule->GetStyleRule() && rule->GetStyleRule()->SelectorList().First()) {
      const CSSSelectorList& list = rule->GetStyleRule()->SelectorList();
      // Compound index (0 -> 1 -> 2).
      int compound = 0;
      // Complex index of the next compound (0 -> 2 -> 3 -> kNotFound).
      wtf_size_t next_compound_start = list.IndexOfNextSelectorAfter(0);

      std::sort(rule_indices.at(rule)->begin(), rule_indices.at(rule)->end());
      for (unsigned complex_match : (*rule_indices.at(rule))) {
        while (complex_match >= next_compound_start &&
               next_compound_start != kNotFound) {
          next_compound_start =
              list.IndexOfNextSelectorAfter(next_compound_start);
          compound++;
        }
        matching_selectors->push_back(compound);
      }
    }

    result->emplace_back(
        protocol::CSS::RuleMatch::create()
            .setRule(std::move(rule_object))
            .setMatchingSelectors(std::move(matching_selectors))
            .build());
  }

  return result;
}

std::unique_ptr<protocol::CSS::CSSStyle>
InspectorCSSAgent::BuildObjectForAttributesStyle(Element* element) {
  if (!element->IsStyledElement())
    return nullptr;

  // FIXME: Ugliness below.
  auto* mutable_attribute_style = DynamicTo<MutableCSSPropertyValueSet>(
      const_cast<CSSPropertyValueSet*>(element->PresentationAttributeStyle()));
  if (!mutable_attribute_style)
    return nullptr;

  InspectorStyle* inspector_style = MakeGarbageCollected<InspectorStyle>(
      mutable_attribute_style->EnsureCSSStyleDeclaration(
          element->GetExecutionContext()),
      nullptr, nullptr);
  return inspector_style->BuildObjectForStyle();
}

void InspectorCSSAgent::DidAddDocument(Document* document) {
  if (!tracker_)
    return;

  document->GetStyleEngine().SetRuleUsageTracker(tracker_);
  document->GetStyleEngine().MarkAllElementsForStyleRecalc(
      StyleChangeReasonForTracing::Create(style_change_reason::kInspector));
}

void InspectorCSSAgent::DidRemoveDocument(Document* document) {}

void InspectorCSSAgent::DidRemoveDOMNode(Node* node) {
  if (!node)
    return;

  int node_id = dom_agent_->BoundNodeId(node);
  if (node_id)
    node_id_to_forced_pseudo_state_.erase(node_id);

  NodeToInspectorStyleSheet::iterator it =
      node_to_inspector_style_sheet_.find(node);
  if (it == node_to_inspector_style_sheet_.end())
    return;

  id_to_inspector_style_sheet_for_inline_style_.erase(it->value->Id());
  node_to_inspector_style_sheet_.erase(node);
}

void InspectorCSSAgent::DidModifyDOMAttr(Element* element) {
  if (!element)
    return;

  NodeToInspectorStyleSheet::iterator it =
      node_to_inspector_style_sheet_.find(element);
  if (it == node_to_inspector_style_sheet_.end())
    return;

  it->value->DidModifyElementAttribute();
}

void InspectorCSSAgent::StyleSheetChanged(
    InspectorStyleSheetBase* style_sheet) {
  if (g_frontend_operation_counter)
    return;
  FlushPendingProtocolNotifications();
  GetFrontend()->styleSheetChanged(style_sheet->Id());
}

void InspectorCSSAgent::ResetPseudoStates() {
  HeapHashSet<Member<Document>> documents_to_change;
  for (auto& state : node_id_to_forced_pseudo_state_) {
    auto* element = To<Element>(dom_agent_->NodeForId(state.key));
    if (element && element->ownerDocument())
      documents_to_change.insert(element->ownerDocument());
  }

  for (auto& count : node_id_to_number_focused_children_) {
    auto* element = To<Element>(dom_agent_->NodeForId(count.key));
    if (element && element->ownerDocument())
      documents_to_change.insert(element->ownerDocument());
  }

  node_id_to_forced_pseudo_state_.clear();
  node_id_to_number_focused_children_.clear();
  for (auto& document : documents_to_change) {
    document->GetStyleEngine().MarkAllElementsForStyleRecalc(
        StyleChangeReasonForTracing::Create(style_change_reason::kInspector));
  }
}

HeapVector<Member<CSSStyleDeclaration>> InspectorCSSAgent::MatchingStyles(
    Element* element) {
  PseudoId pseudo_id = element->GetPseudoId();
  if (pseudo_id)
    element = element->parentElement();
  StyleResolver& style_resolver =
      element->ownerDocument()->EnsureStyleResolver();
  element->UpdateDistributionForUnknownReasons();

  HeapVector<Member<CSSStyleRule>> rules =
      FilterDuplicateRules(style_resolver.PseudoCSSRulesForElement(
          element, pseudo_id, StyleResolver::kAllCSSRules));
  HeapVector<Member<CSSStyleDeclaration>> styles;
  if (!pseudo_id && element->style())
    styles.push_back(element->style());
  for (unsigned i = rules.size(); i > 0; --i) {
    CSSStyleSheet* parent_style_sheet = rules.at(i - 1)->parentStyleSheet();
    if (!parent_style_sheet || !parent_style_sheet->ownerNode())
      continue;  // User agent.
    styles.push_back(rules.at(i - 1)->style());
  }
  return styles;
}

CSSStyleDeclaration* InspectorCSSAgent::FindEffectiveDeclaration(
    const CSSProperty& property_class,
    const HeapVector<Member<CSSStyleDeclaration>>& styles) {
  if (!styles.size())
    return nullptr;

  String longhand = property_class.GetPropertyNameString();
  CSSStyleDeclaration* found_style = nullptr;

  for (unsigned i = 0; i < styles.size(); ++i) {
    CSSStyleDeclaration* style = styles.at(i).Get();
    if (style->getPropertyValue(longhand).IsEmpty())
      continue;
    if (style->getPropertyPriority(longhand) == "important")
      return style;
    if (!found_style)
      found_style = style;
  }

  return found_style ? found_style : styles.at(0).Get();
}

Response InspectorCSSAgent::setEffectivePropertyValueForNode(
    int node_id,
    const String& property_name,
    const String& value) {
  Element* element = nullptr;
  Response response = dom_agent_->AssertElement(node_id, element);
  if (!response.IsSuccess())
    return response;
  if (element->GetPseudoId())
    return Response::ServerError("Elements is pseudo");

  if (!element->ownerDocument()->IsActive()) {
    return Response::ServerError(
        "Can't edit a node from a non-active document");
  }

  CSSPropertyID property =
      cssPropertyID(element->GetExecutionContext(), property_name);
  if (!isValidCSSPropertyID(property))
    return Response::ServerError("Invalid property name");

  CSSPropertyID property_id =
      cssPropertyID(element->GetExecutionContext(), property_name);
  const CSSProperty& property_class = CSSProperty::Get(property_id);
  CSSStyleDeclaration* style =
      FindEffectiveDeclaration(property_class, MatchingStyles(element));
  if (!style)
    return Response::ServerError("Can't find a style to edit");

  bool force_important = false;
  InspectorStyleSheetBase* inspector_style_sheet = nullptr;
  CSSRuleSourceData* source_data;
  // An absence of the parent rule means that given style is an inline style.
  if (style->parentRule()) {
    InspectorStyleSheet* style_sheet =
        BindStyleSheet(style->ParentStyleSheet());
    inspector_style_sheet = style_sheet;
    source_data = style_sheet->SourceDataForRule(style->parentRule());
  } else {
    InspectorStyleSheetForInlineStyle* inline_style_sheet =
        AsInspectorStyleSheet(element);
    inspector_style_sheet = inline_style_sheet;
    source_data = inline_style_sheet->RuleSourceData();
  }

  if (!source_data)
    return Response::ServerError("Can't find a source to edit");

  Vector<StylePropertyShorthand, 4> shorthands;
  getMatchingShorthandsForLonghand(property_id, &shorthands);

  String shorthand =
      shorthands.size() > 0
          ? CSSProperty::Get(shorthands[0].id()).GetPropertyNameString()
          : String();
  String longhand = property_class.GetPropertyNameString();

  int found_index = -1;
  Vector<CSSPropertySourceData>& properties = source_data->property_data;
  for (unsigned i = 0; i < properties.size(); ++i) {
    CSSPropertySourceData property = properties[properties.size() - i - 1];
    String name = property.name;
    if (property.disabled)
      continue;

    if (name != shorthand && name != longhand)
      continue;

    if (property.important || found_index == -1)
      found_index = properties.size() - i - 1;

    if (property.important)
      break;
  }

  SourceRange body_range = source_data->rule_body_range;
  String style_sheet_text;
  inspector_style_sheet->GetText(&style_sheet_text);
  String style_text =
      style_sheet_text.Substring(body_range.start, body_range.length());
  SourceRange change_range;
  if (found_index == -1) {
    String new_property_text = "\n" + longhand + ": " + value +
                               (force_important ? " !important" : "") + ";";
    if (!style_text.IsEmpty() && !style_text.StripWhiteSpace().EndsWith(';'))
      new_property_text = ";" + new_property_text;
    style_text = style_text + new_property_text;
    change_range.start = body_range.end;
    change_range.end = body_range.end + new_property_text.length();
  } else {
    CSSPropertySourceData declaration = properties[found_index];
    String new_value_text;
    if (declaration.name == shorthand)
      new_value_text = CreateShorthandValue(element->ownerDocument(), shorthand,
                                            declaration.value, longhand, value);
    else
      new_value_text = value;

    String new_property_text =
        declaration.name + ": " + new_value_text +
        (declaration.important || force_important ? " !important" : "") + ";";
    style_text.replace(declaration.range.start - body_range.start,
                       declaration.range.length(), new_property_text);
    change_range.start = declaration.range.start;
    change_range.end = change_range.start + new_property_text.length();
  }
  CSSStyleDeclaration* result_style;
  return SetStyleText(inspector_style_sheet, body_range, style_text,
                      result_style);
}

Response InspectorCSSAgent::getBackgroundColors(
    int node_id,
    Maybe<protocol::Array<String>>* background_colors,
    Maybe<String>* computed_font_size,
    Maybe<String>* computed_font_weight) {
  Element* element = nullptr;
  Response response = dom_agent_->AssertElement(node_id, element);
  if (!response.IsSuccess())
    return response;

  Vector<Color> bgcolors;
  String fs;
  String fw;
  InspectorCSSAgent::GetBackgroundColors(element, &bgcolors, &fs, &fw);

  if (bgcolors.size()) {
    *background_colors = std::make_unique<protocol::Array<String>>();
    for (const auto& color : bgcolors) {
      background_colors->fromJust()->emplace_back(
          cssvalue::CSSColorValue::SerializeAsCSSComponentValue(color));
    }
  }
  if (!fs.IsEmpty())
    *computed_font_size = fs;
  if (!fw.IsEmpty())
    *computed_font_weight = fw;
  return Response::Success();
}

// static
void InspectorCSSAgent::GetBackgroundColors(Element* element,
                                            Vector<Color>* colors,
                                            String* computed_font_size,
                                            String* computed_font_weight) {
  // TODO: only support the single text child node here.
  // Follow up with a larger fix post-merge.
  auto* text_node = DynamicTo<Text>(element->firstChild());
  if (!text_node || element->firstChild()->nextSibling()) {
    return;
  }

  PhysicalRect content_bounds = text_node->BoundingBox();
  LocalFrameView* view = text_node->GetDocument().View();
  if (!view)
    return;

  Document& document = text_node->GetDocument();
  bool is_main_frame = document.IsInMainFrame();
  bool found_opaque_color = false;
  if (is_main_frame) {
    // Start with the "default" page color (typically white).
    Color base_background_color = view->BaseBackgroundColor();
    colors->push_back(view->BaseBackgroundColor());
    found_opaque_color = !base_background_color.HasAlpha();
  }

  found_opaque_color = GetColorsFromRect(
      content_bounds, text_node->GetDocument(), element, *colors);

  if (!found_opaque_color && !is_main_frame) {
    for (HTMLFrameOwnerElement* owner_element = document.LocalOwner();
         !found_opaque_color && owner_element;
         owner_element = owner_element->GetDocument().LocalOwner()) {
      found_opaque_color = GetColorsFromRect(
          content_bounds, owner_element->GetDocument(), nullptr, *colors);
    }
  }

  auto* computed_style_info =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(element, true);
  const CSSValue* font_size =
      computed_style_info->GetPropertyCSSValue(CSSPropertyID::kFontSize);
  if (font_size)
    *computed_font_size = font_size->CssText();
  const CSSValue* font_weight =
      computed_style_info->GetPropertyCSSValue(CSSPropertyID::kFontWeight);
  if (font_weight)
    *computed_font_weight = font_weight->CssText();
}

void InspectorCSSAgent::SetCoverageEnabled(bool enabled) {
  if (enabled == !!tracker_)
    return;
  tracker_ = enabled ? MakeGarbageCollected<StyleRuleUsageTracker>() : nullptr;

  for (Document* document : dom_agent_->Documents())
    document->GetStyleEngine().SetRuleUsageTracker(tracker_);
}

void InspectorCSSAgent::WillChangeStyleElement(Element* element) {
  resource_container_->EraseStyleElementContent(DOMNodeIds::IdForNode(element));
}

Response InspectorCSSAgent::startRuleUsageTracking() {
  coverage_enabled_.Set(true);
  SetCoverageEnabled(true);

  for (Document* document : dom_agent_->Documents()) {
    document->GetStyleEngine().MarkAllElementsForStyleRecalc(
        StyleChangeReasonForTracing::Create(style_change_reason::kInspector));
    document->UpdateStyleAndLayoutTree();
  }

  return Response::Success();
}

Response InspectorCSSAgent::stopRuleUsageTracking(
    std::unique_ptr<protocol::Array<protocol::CSS::RuleUsage>>* result) {
  for (Document* document : dom_agent_->Documents())
    document->UpdateStyleAndLayoutTree();
  double timestamp;
  Response response = takeCoverageDelta(result, &timestamp);
  SetCoverageEnabled(false);
  return response;
}

Response InspectorCSSAgent::takeCoverageDelta(
    std::unique_ptr<protocol::Array<protocol::CSS::RuleUsage>>* result,
    double* out_timestamp) {
  if (!tracker_)
    return Response::ServerError("CSS rule usage tracking is not enabled");

  StyleRuleUsageTracker::RuleListByStyleSheet coverage_delta =
      tracker_->TakeDelta();

  *out_timestamp = base::TimeTicks::Now().since_origin().InSecondsF();

  *result = std::make_unique<protocol::Array<protocol::CSS::RuleUsage>>();

  for (const auto& entry : coverage_delta) {
    const CSSStyleSheet* css_style_sheet = entry.key.Get();
    InspectorStyleSheet* style_sheet =
        css_style_sheet_to_inspector_style_sheet_.at(
            const_cast<CSSStyleSheet*>(css_style_sheet));
    if (!style_sheet)
      continue;

    HeapHashMap<Member<const StyleRule>, Member<CSSStyleRule>> rule_to_css_rule;
    const CSSRuleVector& css_rules = style_sheet->FlatRules();
    for (auto css_rule : css_rules) {
      if (css_rule->type() != CSSRule::kStyleRule)
        continue;
      CSSStyleRule* css_style_rule = AsCSSStyleRule(css_rule);
      rule_to_css_rule.Set(css_style_rule->GetStyleRule(), css_style_rule);
    }
    for (auto used_rule : entry.value) {
      CSSStyleRule* css_style_rule = rule_to_css_rule.at(used_rule);
      if (std::unique_ptr<protocol::CSS::RuleUsage> rule_usage_object =
              style_sheet->BuildObjectForRuleUsage(css_style_rule, true)) {
        (*result)->emplace_back(std::move(rule_usage_object));
      }
    }
  }

  return Response::Success();
}

void InspectorCSSAgent::Trace(Visitor* visitor) {
  visitor->Trace(dom_agent_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(network_agent_);
  visitor->Trace(resource_content_loader_);
  visitor->Trace(resource_container_);
  visitor->Trace(id_to_inspector_style_sheet_);
  visitor->Trace(id_to_inspector_style_sheet_for_inline_style_);
  visitor->Trace(css_style_sheet_to_inspector_style_sheet_);
  visitor->Trace(document_to_css_style_sheets_);
  visitor->Trace(invalidated_documents_);
  visitor->Trace(node_to_inspector_style_sheet_);
  visitor->Trace(inspector_user_agent_style_sheet_);
  visitor->Trace(tracker_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
