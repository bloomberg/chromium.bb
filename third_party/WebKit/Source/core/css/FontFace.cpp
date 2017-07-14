/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/css/FontFace.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/StringOrArrayBufferOrArrayBufferView.h"
#include "core/CSSValueKeywords.h"
#include "core/css/BinaryDataFontFaceSource.h"
#include "core/css/CSSFontFace.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/FontFaceDescriptors.h"
#include "core/css/LocalFontFaceSource.h"
#include "core/css/RemoteFontFaceSource.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "core/typed_arrays/DOMArrayBufferView.h"
#include "platform/FontFamilyNames.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/WebTaskRunner.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

static const CSSValue* ParseCSSValue(const Document* document,
                                     const String& value,
                                     CSSPropertyID property_id) {
  CSSParserContext* context = CSSParserContext::Create(*document);
  return CSSParser::ParseFontFaceDescriptor(property_id, value, context);
}

FontFace* FontFace::Create(ExecutionContext* context,
                           const AtomicString& family,
                           StringOrArrayBufferOrArrayBufferView& source,
                           const FontFaceDescriptors& descriptors) {
  if (source.isString())
    return Create(context, family, source.getAsString(), descriptors);
  if (source.isArrayBuffer())
    return Create(context, family, source.getAsArrayBuffer(), descriptors);
  if (source.isArrayBufferView()) {
    return Create(context, family, source.getAsArrayBufferView().View(),
                  descriptors);
  }
  NOTREACHED();
  return nullptr;
}

FontFace* FontFace::Create(ExecutionContext* context,
                           const AtomicString& family,
                           const String& source,
                           const FontFaceDescriptors& descriptors) {
  FontFace* font_face = new FontFace(context, family, descriptors);

  const CSSValue* src =
      ParseCSSValue(ToDocument(context), source, CSSPropertySrc);
  if (!src || !src->IsValueList())
    font_face->SetError(DOMException::Create(
        kSyntaxError, "The source provided ('" + source +
                          "') could not be parsed as a value list."));

  font_face->InitCSSFontFace(ToDocument(context), src);
  return font_face;
}

FontFace* FontFace::Create(ExecutionContext* context,
                           const AtomicString& family,
                           DOMArrayBuffer* source,
                           const FontFaceDescriptors& descriptors) {
  FontFace* font_face = new FontFace(context, family, descriptors);
  font_face->InitCSSFontFace(static_cast<const unsigned char*>(source->Data()),
                             source->ByteLength());
  return font_face;
}

FontFace* FontFace::Create(ExecutionContext* context,
                           const AtomicString& family,
                           DOMArrayBufferView* source,
                           const FontFaceDescriptors& descriptors) {
  FontFace* font_face = new FontFace(context, family, descriptors);
  font_face->InitCSSFontFace(
      static_cast<const unsigned char*>(source->BaseAddress()),
      source->byteLength());
  return font_face;
}

FontFace* FontFace::Create(Document* document,
                           const StyleRuleFontFace* font_face_rule) {
  const StylePropertySet& properties = font_face_rule->Properties();

  // Obtain the font-family property and the src property. Both must be defined.
  const CSSValue* family =
      properties.GetPropertyCSSValue(CSSPropertyFontFamily);
  if (!family || (!family->IsFontFamilyValue() && !family->IsIdentifierValue()))
    return nullptr;
  const CSSValue* src = properties.GetPropertyCSSValue(CSSPropertySrc);
  if (!src || !src->IsValueList())
    return nullptr;

  FontFace* font_face = new FontFace(document);

  if (font_face->SetFamilyValue(*family) &&
      font_face->SetPropertyFromStyle(properties, CSSPropertyFontStyle) &&
      font_face->SetPropertyFromStyle(properties, CSSPropertyFontWeight) &&
      font_face->SetPropertyFromStyle(properties, CSSPropertyFontStretch) &&
      font_face->SetPropertyFromStyle(properties, CSSPropertyUnicodeRange) &&
      font_face->SetPropertyFromStyle(properties, CSSPropertyFontVariant) &&
      font_face->SetPropertyFromStyle(properties,
                                      CSSPropertyFontFeatureSettings) &&
      font_face->SetPropertyFromStyle(properties, CSSPropertyFontDisplay) &&
      !font_face->family().IsEmpty() && font_face->Traits().Bitfield()) {
    font_face->InitCSSFontFace(document, src);
    return font_face;
  }
  return nullptr;
}

FontFace::FontFace(ExecutionContext* context)
    : ContextClient(context), status_(kUnloaded) {}

FontFace::FontFace(ExecutionContext* context,
                   const AtomicString& family,
                   const FontFaceDescriptors& descriptors)
    : ContextClient(context), family_(family), status_(kUnloaded) {
  Document* document = ToDocument(context);
  SetPropertyFromString(document, descriptors.style(), CSSPropertyFontStyle);
  SetPropertyFromString(document, descriptors.weight(), CSSPropertyFontWeight);
  SetPropertyFromString(document, descriptors.stretch(),
                        CSSPropertyFontStretch);
  SetPropertyFromString(document, descriptors.unicodeRange(),
                        CSSPropertyUnicodeRange);
  SetPropertyFromString(document, descriptors.variant(),
                        CSSPropertyFontVariant);
  SetPropertyFromString(document, descriptors.featureSettings(),
                        CSSPropertyFontFeatureSettings);
  if (RuntimeEnabledFeatures::CSSFontDisplayEnabled()) {
    SetPropertyFromString(document, descriptors.display(),
                          CSSPropertyFontDisplay);
  }
}

FontFace::~FontFace() {}

String FontFace::style() const {
  return style_ ? style_->CssText() : "normal";
}

String FontFace::weight() const {
  return weight_ ? weight_->CssText() : "normal";
}

String FontFace::stretch() const {
  return stretch_ ? stretch_->CssText() : "normal";
}

String FontFace::unicodeRange() const {
  return unicode_range_ ? unicode_range_->CssText() : "U+0-10FFFF";
}

String FontFace::variant() const {
  return variant_ ? variant_->CssText() : "normal";
}

String FontFace::featureSettings() const {
  return feature_settings_ ? feature_settings_->CssText() : "normal";
}

String FontFace::display() const {
  return display_ ? display_->CssText() : "auto";
}

void FontFace::setStyle(ExecutionContext* context,
                        const String& s,
                        ExceptionState& exception_state) {
  SetPropertyFromString(ToDocument(context), s, CSSPropertyFontStyle,
                        &exception_state);
}

void FontFace::setWeight(ExecutionContext* context,
                         const String& s,
                         ExceptionState& exception_state) {
  SetPropertyFromString(ToDocument(context), s, CSSPropertyFontWeight,
                        &exception_state);
}

void FontFace::setStretch(ExecutionContext* context,
                          const String& s,
                          ExceptionState& exception_state) {
  SetPropertyFromString(ToDocument(context), s, CSSPropertyFontStretch,
                        &exception_state);
}

void FontFace::setUnicodeRange(ExecutionContext* context,
                               const String& s,
                               ExceptionState& exception_state) {
  SetPropertyFromString(ToDocument(context), s, CSSPropertyUnicodeRange,
                        &exception_state);
}

void FontFace::setVariant(ExecutionContext* context,
                          const String& s,
                          ExceptionState& exception_state) {
  SetPropertyFromString(ToDocument(context), s, CSSPropertyFontVariant,
                        &exception_state);
}

void FontFace::setFeatureSettings(ExecutionContext* context,
                                  const String& s,
                                  ExceptionState& exception_state) {
  SetPropertyFromString(ToDocument(context), s, CSSPropertyFontFeatureSettings,
                        &exception_state);
}

void FontFace::setDisplay(ExecutionContext* context,
                          const String& s,
                          ExceptionState& exception_state) {
  SetPropertyFromString(ToDocument(context), s, CSSPropertyFontDisplay,
                        &exception_state);
}

void FontFace::SetPropertyFromString(const Document* document,
                                     const String& s,
                                     CSSPropertyID property_id,
                                     ExceptionState* exception_state) {
  const CSSValue* value = ParseCSSValue(document, s, property_id);
  if (value && SetPropertyValue(value, property_id))
    return;

  String message = "Failed to set '" + s + "' as a property value.";
  if (exception_state)
    exception_state->ThrowDOMException(kSyntaxError, message);
  else
    SetError(DOMException::Create(kSyntaxError, message));
}

bool FontFace::SetPropertyFromStyle(const StylePropertySet& properties,
                                    CSSPropertyID property_id) {
  return SetPropertyValue(properties.GetPropertyCSSValue(property_id),
                          property_id);
}

bool FontFace::SetPropertyValue(const CSSValue* value,
                                CSSPropertyID property_id) {
  switch (property_id) {
    case CSSPropertyFontStyle:
      style_ = value;
      break;
    case CSSPropertyFontWeight:
      weight_ = value;
      break;
    case CSSPropertyFontStretch:
      stretch_ = value;
      break;
    case CSSPropertyUnicodeRange:
      if (value && !value->IsValueList())
        return false;
      unicode_range_ = value;
      break;
    case CSSPropertyFontVariant:
      variant_ = value;
      break;
    case CSSPropertyFontFeatureSettings:
      feature_settings_ = value;
      break;
    case CSSPropertyFontDisplay:
      display_ = value;
      break;
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

bool FontFace::SetFamilyValue(const CSSValue& family_value) {
  AtomicString family;
  if (family_value.IsFontFamilyValue()) {
    family = AtomicString(ToCSSFontFamilyValue(family_value).Value());
  } else if (family_value.IsIdentifierValue()) {
    // We need to use the raw text for all the generic family types, since
    // @font-face is a way of actually defining what font to use for those
    // types.
    switch (ToCSSIdentifierValue(family_value).GetValueID()) {
      case CSSValueSerif:
        family = FontFamilyNames::webkit_serif;
        break;
      case CSSValueSansSerif:
        family = FontFamilyNames::webkit_sans_serif;
        break;
      case CSSValueCursive:
        family = FontFamilyNames::webkit_cursive;
        break;
      case CSSValueFantasy:
        family = FontFamilyNames::webkit_fantasy;
        break;
      case CSSValueMonospace:
        family = FontFamilyNames::webkit_monospace;
        break;
      case CSSValueWebkitPictograph:
        family = FontFamilyNames::webkit_pictograph;
        break;
      default:
        return false;
    }
  }
  family_ = family;
  return true;
}

String FontFace::status() const {
  switch (status_) {
    case kUnloaded:
      return "unloaded";
    case kLoading:
      return "loading";
    case kLoaded:
      return "loaded";
    case kError:
      return "error";
    default:
      NOTREACHED();
  }
  return g_empty_string;
}

void FontFace::SetLoadStatus(LoadStatusType status) {
  status_ = status;
  DCHECK(status_ != kError || error_);

  // When promises are resolved with 'thenables', instead of the object being
  // returned directly, the 'then' method is executed (the resolver tries to
  // resolve the thenable). This can lead to synchronous script execution, so we
  // post a task. This does not apply to promise rejection (i.e. a thenable
  // would be returned as is).
  if (status_ == kLoaded || status_ == kError) {
    if (loaded_property_) {
      if (status_ == kLoaded) {
        GetTaskRunner()->PostTask(
            BLINK_FROM_HERE, WTF::Bind(&LoadedProperty::Resolve<FontFace*>,
                                       WrapPersistent(loaded_property_.Get()),
                                       WrapPersistent(this)));
      } else
        loaded_property_->Reject(error_.Get());
    }

    GetTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        WTF::Bind(&FontFace::RunCallbacks, WrapPersistent(this)));
  }
}

WebTaskRunner* FontFace::GetTaskRunner() {
  return TaskRunnerHelper::Get(TaskType::kDOMManipulation,
                               GetExecutionContext())
      .Get();
}

void FontFace::RunCallbacks() {
  HeapVector<Member<LoadFontCallback>> callbacks;
  callbacks_.swap(callbacks);
  for (size_t i = 0; i < callbacks.size(); ++i) {
    if (status_ == kLoaded)
      callbacks[i]->NotifyLoaded(this);
    else
      callbacks[i]->NotifyError(this);
  }
}

void FontFace::SetError(DOMException* error) {
  if (!error_)
    error_ = error ? error : DOMException::Create(kNetworkError);
  SetLoadStatus(kError);
}

ScriptPromise FontFace::FontStatusPromise(ScriptState* script_state) {
  if (!loaded_property_) {
    loaded_property_ = new LoadedProperty(ExecutionContext::From(script_state),
                                          this, LoadedProperty::kLoaded);
    if (status_ == kLoaded)
      loaded_property_->Resolve(this);
    else if (status_ == kError)
      loaded_property_->Reject(error_.Get());
  }
  return loaded_property_->Promise(script_state->World());
}

ScriptPromise FontFace::load(ScriptState* script_state) {
  if (status_ == kUnloaded)
    css_font_face_->Load();
  return FontStatusPromise(script_state);
}

void FontFace::LoadWithCallback(LoadFontCallback* callback) {
  if (status_ == kUnloaded)
    css_font_face_->Load();
  AddCallback(callback);
}

void FontFace::AddCallback(LoadFontCallback* callback) {
  if (status_ == kLoaded)
    callback->NotifyLoaded(this);
  else if (status_ == kError)
    callback->NotifyError(this);
  else
    callbacks_.push_back(callback);
}

FontTraits FontFace::Traits() const {
  FontStretch stretch = kFontStretchNormal;
  if (stretch_) {
    if (!stretch_->IsIdentifierValue())
      return 0;

    switch (ToCSSIdentifierValue(stretch_.Get())->GetValueID()) {
      case CSSValueUltraCondensed:
        stretch = kFontStretchUltraCondensed;
        break;
      case CSSValueExtraCondensed:
        stretch = kFontStretchExtraCondensed;
        break;
      case CSSValueCondensed:
        stretch = kFontStretchCondensed;
        break;
      case CSSValueSemiCondensed:
        stretch = kFontStretchSemiCondensed;
        break;
      case CSSValueSemiExpanded:
        stretch = kFontStretchSemiExpanded;
        break;
      case CSSValueExpanded:
        stretch = kFontStretchExpanded;
        break;
      case CSSValueExtraExpanded:
        stretch = kFontStretchExtraExpanded;
        break;
      case CSSValueUltraExpanded:
        stretch = kFontStretchUltraExpanded;
        break;
      default:
        break;
    }
  }

  FontStyle style = kFontStyleNormal;
  if (style_) {
    if (!style_->IsIdentifierValue())
      return 0;

    switch (ToCSSIdentifierValue(style_.Get())->GetValueID()) {
      case CSSValueNormal:
        style = kFontStyleNormal;
        break;
      case CSSValueOblique:
        style = kFontStyleOblique;
        break;
      case CSSValueItalic:
        style = kFontStyleItalic;
        break;
      default:
        break;
    }
  }

  FontWeight weight = kFontWeight400;
  if (weight_) {
    if (!weight_->IsIdentifierValue())
      return 0;

    switch (ToCSSIdentifierValue(weight_.Get())->GetValueID()) {
      case CSSValueBold:
      case CSSValue700:
        weight = kFontWeight700;
        break;
      case CSSValueNormal:
      case CSSValue400:
        weight = kFontWeight400;
        break;
      case CSSValue900:
        weight = kFontWeight900;
        break;
      case CSSValue800:
        weight = kFontWeight800;
        break;
      case CSSValue600:
        weight = kFontWeight600;
        break;
      case CSSValue500:
        weight = kFontWeight500;
        break;
      case CSSValue300:
        weight = kFontWeight300;
        break;
      case CSSValue200:
        weight = kFontWeight200;
        break;
      case CSSValue100:
        weight = kFontWeight100;
        break;
      // Although 'lighter' and 'bolder' are valid keywords for font-weights,
      // they are invalid inside font-face rules so they are ignored. Reference:
      // http://www.w3.org/TR/css3-fonts/#descdef-font-weight.
      case CSSValueLighter:
      case CSSValueBolder:
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  return FontTraits(style, weight, stretch);
}

size_t FontFace::ApproximateBlankCharacterCount() const {
  if (status_ == kLoading)
    return css_font_face_->ApproximateBlankCharacterCount();
  return 0;
}

static FontDisplay CSSValueToFontDisplay(const CSSValue* value) {
  if (value && value->IsIdentifierValue()) {
    switch (ToCSSIdentifierValue(value)->GetValueID()) {
      case CSSValueAuto:
        return kFontDisplayAuto;
      case CSSValueBlock:
        return kFontDisplayBlock;
      case CSSValueSwap:
        return kFontDisplaySwap;
      case CSSValueFallback:
        return kFontDisplayFallback;
      case CSSValueOptional:
        return kFontDisplayOptional;
      default:
        break;
    }
  }
  return kFontDisplayAuto;
}

static CSSFontFace* CreateCSSFontFace(FontFace* font_face,
                                      const CSSValue* unicode_range) {
  Vector<UnicodeRange> ranges;
  if (const CSSValueList* range_list = ToCSSValueList(unicode_range)) {
    unsigned num_ranges = range_list->length();
    for (unsigned i = 0; i < num_ranges; i++) {
      const CSSUnicodeRangeValue& range =
          ToCSSUnicodeRangeValue(range_list->Item(i));
      ranges.push_back(UnicodeRange(range.From(), range.To()));
    }
  }

  return new CSSFontFace(font_face, ranges);
}

void FontFace::InitCSSFontFace(Document* document, const CSSValue* src) {
  css_font_face_ = CreateCSSFontFace(this, unicode_range_.Get());
  if (error_)
    return;

  // Each item in the src property's list is a single CSSFontFaceSource. Put
  // them all into a CSSFontFace.
  DCHECK(src);
  DCHECK(src->IsValueList());
  const CSSValueList* src_list = ToCSSValueList(src);
  int src_length = src_list->length();

  for (int i = 0; i < src_length; i++) {
    // An item in the list either specifies a string (local font name) or a URL
    // (remote font to download).
    const CSSFontFaceSrcValue& item = ToCSSFontFaceSrcValue(src_list->Item(i));
    CSSFontFaceSource* source = nullptr;

    if (!item.IsLocal()) {
      const Settings* settings = document ? document->GetSettings() : nullptr;
      bool allow_downloading =
          settings && settings->GetDownloadableBinaryFontsEnabled();
      if (allow_downloading && item.IsSupportedFormat() && document) {
        FontResource* fetched = item.Fetch(document);
        if (fetched) {
          CSSFontSelector* font_selector =
              document->GetStyleEngine().FontSelector();
          source = new RemoteFontFaceSource(
              fetched, font_selector, CSSValueToFontDisplay(display_.Get()));
        }
      }
    } else {
      source = new LocalFontFaceSource(item.GetResource());
    }

    if (source)
      css_font_face_->AddSource(source);
  }

  if (display_) {
    DEFINE_STATIC_LOCAL(EnumerationHistogram, font_display_histogram,
                        ("WebFont.FontDisplayValue", kFontDisplayEnumMax));
    font_display_histogram.Count(CSSValueToFontDisplay(display_.Get()));
  }
}

void FontFace::InitCSSFontFace(const unsigned char* data, size_t size) {
  css_font_face_ = CreateCSSFontFace(this, unicode_range_.Get());
  if (error_)
    return;

  RefPtr<SharedBuffer> buffer = SharedBuffer::Create(data, size);
  BinaryDataFontFaceSource* source =
      new BinaryDataFontFaceSource(buffer.Get(), ots_parse_message_);
  if (source->IsValid())
    SetLoadStatus(kLoaded);
  else
    SetError(DOMException::Create(kSyntaxError,
                                  "Invalid font data in ArrayBuffer."));
  css_font_face_->AddSource(source);
}

DEFINE_TRACE(FontFace) {
  visitor->Trace(style_);
  visitor->Trace(weight_);
  visitor->Trace(stretch_);
  visitor->Trace(unicode_range_);
  visitor->Trace(variant_);
  visitor->Trace(feature_settings_);
  visitor->Trace(display_);
  visitor->Trace(error_);
  visitor->Trace(loaded_property_);
  visitor->Trace(css_font_face_);
  visitor->Trace(callbacks_);
  ContextClient::Trace(visitor);
}

bool FontFace::HadBlankText() const {
  return css_font_face_->HadBlankText();
}

bool FontFace::HasPendingActivity() const {
  return status_ == kLoading && GetExecutionContext();
}

}  // namespace blink
