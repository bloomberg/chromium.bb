// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/imports/HTMLImportTreeRoot.h"

#include "core/css/StyleEngine.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/html/imports/HTMLImportChild.h"

namespace blink {

HTMLImportTreeRoot* HTMLImportTreeRoot::Create(Document* document) {
  return new HTMLImportTreeRoot(document);
}

HTMLImportTreeRoot::HTMLImportTreeRoot(Document* document)
    : HTMLImport(HTMLImport::kSync),
      document_(document),
      recalc_timer_(
          document->GetFrame()->GetTaskRunner(TaskType::kUnspecedTimer),
          this,
          &HTMLImportTreeRoot::RecalcTimerFired) {
  ScheduleRecalcState();  // This recomputes initial state.
}

HTMLImportTreeRoot::~HTMLImportTreeRoot() {}

void HTMLImportTreeRoot::Dispose() {
  for (const auto& import_child : imports_)
    import_child->Dispose();
  imports_.clear();
  document_ = nullptr;
  recalc_timer_.Stop();
}

Document* HTMLImportTreeRoot::GetDocument() const {
  return document_;
}

bool HTMLImportTreeRoot::HasFinishedLoading() const {
  return !document_->Parsing() &&
         document_->GetStyleEngine().HaveScriptBlockingStylesheetsLoaded();
}

void HTMLImportTreeRoot::StateWillChange() {
  ScheduleRecalcState();
}

void HTMLImportTreeRoot::StateDidChange() {
  HTMLImport::StateDidChange();

  if (GetState().IsReady())
    document_->CheckCompleted();
}

void HTMLImportTreeRoot::ScheduleRecalcState() {
  DCHECK(document_);
  if (recalc_timer_.IsActive() || !document_->IsActive())
    return;
  recalc_timer_.StartOneShot(TimeDelta(), BLINK_FROM_HERE);
}

HTMLImportChild* HTMLImportTreeRoot::Add(HTMLImportChild* child) {
  imports_.push_back(child);
  return imports_.back().Get();
}

HTMLImportChild* HTMLImportTreeRoot::Find(const KURL& url) const {
  for (const auto& candidate : imports_) {
    if (EqualIgnoringFragmentIdentifier(candidate->Url(), url))
      return candidate;
  }

  return nullptr;
}

void HTMLImportTreeRoot::RecalcTimerFired(TimerBase*) {
  DCHECK(document_);
  HTMLImport::RecalcTreeState(this);
}

void HTMLImportTreeRoot::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(imports_);
  HTMLImport::Trace(visitor);
}

void HTMLImportTreeRoot::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(document_);
}

}  // namespace blink
