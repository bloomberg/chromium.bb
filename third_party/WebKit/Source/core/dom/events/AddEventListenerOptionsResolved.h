// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AddEventListenerOptionsResolved_h
#define AddEventListenerOptionsResolved_h

#include "core/dom/events/AddEventListenerOptions.h"

namespace blink {

// AddEventListenerOptionsResolved class represents resolved event listener
// options. An application requests AddEventListenerOptions and the user
// agent may change ('resolve') these settings (based on settings or policies)
// and the result and the reasons why changes occurred are stored in this class.
class CORE_EXPORT AddEventListenerOptionsResolved
    : public AddEventListenerOptions {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  AddEventListenerOptionsResolved();
  AddEventListenerOptionsResolved(const AddEventListenerOptions&);
  virtual ~AddEventListenerOptionsResolved();

  void SetPassiveForcedForDocumentTarget(bool forced) {
    passive_forced_for_document_target_ = forced;
  }
  bool PassiveForcedForDocumentTarget() const {
    return passive_forced_for_document_target_;
  }

  // Set whether passive was specified when the options were
  // created by callee.
  void SetPassiveSpecified(bool specified) { passive_specified_ = specified; }
  bool PassiveSpecified() const { return passive_specified_; }

  virtual void Trace(blink::Visitor*);

 private:
  bool passive_forced_for_document_target_;
  bool passive_specified_;
};

}  // namespace blink

#endif  // AddEventListenerOptionsResolved_h
