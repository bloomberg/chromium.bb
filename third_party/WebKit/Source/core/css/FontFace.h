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

#ifndef FontFace_h
#define FontFace_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSValue.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMException.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/fonts/FontSelectionTypes.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSFontFace;
class CSSValue;
class DOMArrayBuffer;
class DOMArrayBufferView;
class Document;
class ExceptionState;
class FontFaceDescriptors;
class StringOrArrayBufferOrArrayBufferView;
class StylePropertySet;
class StyleRuleFontFace;

class CORE_EXPORT FontFace : public GarbageCollectedFinalized<FontFace>,
                             public ScriptWrappable,
                             public ActiveScriptWrappable<FontFace>,
                             public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(FontFace);
  WTF_MAKE_NONCOPYABLE(FontFace);

 public:
  enum LoadStatusType { kUnloaded, kLoading, kLoaded, kError };

  static FontFace* Create(ExecutionContext*,
                          const AtomicString& family,
                          StringOrArrayBufferOrArrayBufferView&,
                          const FontFaceDescriptors&);
  static FontFace* Create(Document*, const StyleRuleFontFace*);

  virtual ~FontFace();

  const AtomicString& family() const { return family_; }
  String style() const;
  String weight() const;
  String stretch() const;
  String unicodeRange() const;
  String variant() const;
  String featureSettings() const;
  String display() const;

  // FIXME: Changing these attributes should affect font matching.
  void setFamily(ExecutionContext*, const AtomicString& s, ExceptionState&) {
    family_ = s;
  }
  void setStyle(ExecutionContext*, const String&, ExceptionState&);
  void setWeight(ExecutionContext*, const String&, ExceptionState&);
  void setStretch(ExecutionContext*, const String&, ExceptionState&);
  void setUnicodeRange(ExecutionContext*, const String&, ExceptionState&);
  void setVariant(ExecutionContext*, const String&, ExceptionState&);
  void setFeatureSettings(ExecutionContext*, const String&, ExceptionState&);
  void setDisplay(ExecutionContext*, const String&, ExceptionState&);

  String status() const;
  ScriptPromise loaded(ScriptState* script_state) {
    return FontStatusPromise(script_state);
  }

  ScriptPromise load(ScriptState*);

  LoadStatusType LoadStatus() const { return status_; }
  void SetLoadStatus(LoadStatusType);
  void SetError(DOMException* = nullptr);
  DOMException* GetError() const { return error_; }
  FontSelectionCapabilities GetFontSelectionCapabilities() const;
  CSSFontFace* CssFontFace() { return css_font_face_.Get(); }
  size_t ApproximateBlankCharacterCount() const;

  DECLARE_VIRTUAL_TRACE();

  bool HadBlankText() const;

  class CORE_EXPORT LoadFontCallback : public GarbageCollectedMixin {
   public:
    virtual ~LoadFontCallback() {}
    virtual void NotifyLoaded(FontFace*) = 0;
    virtual void NotifyError(FontFace*) = 0;
    DEFINE_INLINE_VIRTUAL_TRACE() {}
  };
  void LoadWithCallback(LoadFontCallback*);
  void AddCallback(LoadFontCallback*);

  // ScriptWrappable:
  bool HasPendingActivity() const final;

 private:
  static FontFace* Create(ExecutionContext*,
                          const AtomicString& family,
                          DOMArrayBuffer* source,
                          const FontFaceDescriptors&);
  static FontFace* Create(ExecutionContext*,
                          const AtomicString& family,
                          DOMArrayBufferView*,
                          const FontFaceDescriptors&);
  static FontFace* Create(ExecutionContext*,
                          const AtomicString& family,
                          const String& source,
                          const FontFaceDescriptors&);

  explicit FontFace(ExecutionContext*);
  FontFace(ExecutionContext*,
           const AtomicString& family,
           const FontFaceDescriptors&);

  void InitCSSFontFace(Document*, const CSSValue* src);
  void InitCSSFontFace(const unsigned char* data, size_t);
  void SetPropertyFromString(const Document*,
                             const String&,
                             CSSPropertyID,
                             ExceptionState* = nullptr);
  bool SetPropertyFromStyle(const StylePropertySet&, CSSPropertyID);
  bool SetPropertyValue(const CSSValue*, CSSPropertyID);
  bool SetFamilyValue(const CSSValue&);
  ScriptPromise FontStatusPromise(ScriptState*);
  void RunCallbacks();

  using LoadedProperty = ScriptPromiseProperty<Member<FontFace>,
                                               Member<FontFace>,
                                               Member<DOMException>>;

  AtomicString family_;
  String ots_parse_message_;
  Member<const CSSValue> style_;
  Member<const CSSValue> weight_;
  Member<const CSSValue> stretch_;
  Member<const CSSValue> unicode_range_;
  Member<const CSSValue> variant_;
  Member<const CSSValue> feature_settings_;
  Member<const CSSValue> display_;
  LoadStatusType status_;
  Member<DOMException> error_;

  Member<LoadedProperty> loaded_property_;
  Member<CSSFontFace> css_font_face_;
  HeapVector<Member<LoadFontCallback>> callbacks_;
};

using FontFaceArray = HeapVector<Member<FontFace>>;

}  // namespace blink

#endif  // FontFace_h
