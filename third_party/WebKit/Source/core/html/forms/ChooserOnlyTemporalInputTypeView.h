/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ChooserOnlyTemporalInputTypeView_h
#define ChooserOnlyTemporalInputTypeView_h

#include "core/html/forms/BaseTemporalInputType.h"
#include "core/html/forms/DateTimeChooser.h"
#include "core/html/forms/DateTimeChooserClient.h"
#include "core/html/forms/KeyboardClickableInputTypeView.h"
#include "platform/heap/Handle.h"

namespace blink {

class ChooserOnlyTemporalInputTypeView final
    : public GarbageCollectedFinalized<ChooserOnlyTemporalInputTypeView>,
      public KeyboardClickableInputTypeView,
      public DateTimeChooserClient {
  USING_GARBAGE_COLLECTED_MIXIN(ChooserOnlyTemporalInputTypeView);
  USING_PRE_FINALIZER(ChooserOnlyTemporalInputTypeView, CloseDateTimeChooser);

 public:
  static ChooserOnlyTemporalInputTypeView* Create(HTMLInputElement&,
                                                  BaseTemporalInputType&);
  ~ChooserOnlyTemporalInputTypeView() override;
  virtual void Trace(blink::Visitor*);

 private:
  ChooserOnlyTemporalInputTypeView(HTMLInputElement&, BaseTemporalInputType&);
  void CloseDateTimeChooser();

  // InputTypeView functions:
  void CreateShadowSubtree() override;
  void ClosePopupView() override;
  void DidSetValue(const String&, bool value_changed) override;
  void HandleDOMActivateEvent(Event*) override;
  void UpdateView() override;

  // DateTimeChooserClient functions:
  Element& OwnerElement() const override;
  void DidChooseValue(const String&) override;
  void DidChooseValue(double) override;
  void DidEndChooser() override;

  Member<BaseTemporalInputType> input_type_;
  Member<DateTimeChooser> date_time_chooser_;
};

}  // namespace blink

#endif  // ChooserOnlyTemporalInputTypeView_h
