/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DOMTokenList_h
#define DOMTokenList_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/SpaceSplitString.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT DOMTokenListObserver : public GarbageCollectedMixin {
 public:
  // Called when the value property is set, even if the tokens in
  // the set have not changed.
  virtual void ValueWasSet() = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

class CORE_EXPORT DOMTokenList : public GarbageCollectedFinalized<DOMTokenList>,
                                 public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(DOMTokenList);

 public:
  static DOMTokenList* Create(DOMTokenListObserver* observer = nullptr) {
    return new DOMTokenList(observer);
  }

  virtual ~DOMTokenList() {}

  virtual unsigned length() const { return tokens_.size(); }
  virtual const AtomicString item(unsigned index) const;

  bool contains(const AtomicString&, ExceptionState&) const;
  virtual void add(const Vector<String>&, ExceptionState&);
  void add(const AtomicString&, ExceptionState&);
  virtual void remove(const Vector<String>&, ExceptionState&);
  void remove(const AtomicString&, ExceptionState&);
  bool toggle(const AtomicString&, ExceptionState&);
  bool toggle(const AtomicString&, bool force, ExceptionState&);
  bool supports(const AtomicString&, ExceptionState&);

  virtual const AtomicString& value() const { return value_; }
  virtual void setValue(const AtomicString&);

  const SpaceSplitString& Tokens() const { return tokens_; }
  void SetObserver(DOMTokenListObserver* observer) { observer_ = observer; }

  const AtomicString& toString() const { return value(); }

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->Trace(observer_); }

 protected:
  DOMTokenList(DOMTokenListObserver* observer) : observer_(observer) {}

  virtual void AddInternal(const AtomicString&);
  virtual bool ContainsInternal(const AtomicString&) const;
  virtual void RemoveInternal(const AtomicString&);

  bool ValidateToken(const String&, ExceptionState&) const;
  bool ValidateTokens(const Vector<String>&, ExceptionState&) const;
  virtual bool ValidateTokenValue(const AtomicString&, ExceptionState&) const;
  static AtomicString AddToken(const AtomicString&, const AtomicString&);
  static AtomicString AddTokens(const AtomicString&, const Vector<String>&);
  static AtomicString RemoveToken(const AtomicString&, const AtomicString&);
  static AtomicString RemoveTokens(const AtomicString&, const Vector<String>&);

 private:
  SpaceSplitString tokens_;
  AtomicString value_;
  WeakMember<DOMTokenListObserver> observer_;
};

}  // namespace blink

#endif  // DOMTokenList_h
