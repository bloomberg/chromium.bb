// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/ime/ime_context.h"

namespace {

#ifdef USE_DUMMY_IME_CONTEXT
class DummyIMEContext : public views::IMEContext {
 public:
  explicit DummyIMEContext(views::View* view)
    : views::IMEContext(view) {}
  virtual ~DummyIMEContext() {}

  // views::IMEContext implementations:
  virtual void Focus() {}
  virtual void Blur() {}
  virtual void Reset() {}
  virtual bool FilterKeyEvent(const views::KeyEvent& event) {
    return false;
  }
  virtual void SetCursorLocation(const gfx::Rect& caret_rect) {}
  virtual void SetSurrounding(const string16& text, int cursor_pos) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyIMEContext);
};
#endif  // USE_DUMMY_IME_CONTEXT

}  // namespace

namespace views {

IMEContext::IMEContext(View* view)
  : view_(view) ,
    commit_text_listener_(NULL),
    composition_listener_(NULL),
    forward_key_event_listener_(NULL),
    surrounding_listener_(NULL) {
}

void IMEContext::CommitText(const string16& text) {
  if (commit_text_listener_)
    commit_text_listener_->OnCommitText(this, text);
}

void IMEContext::StartComposition() {
  if (composition_listener_)
    composition_listener_->OnStartComposition(this);
}

void IMEContext::EndComposition() {
  if (composition_listener_)
    composition_listener_->OnEndComposition(this);
}

void IMEContext::SetComposition(const string16& text,
                                const CompositionAttributeList& attributes,
                                uint32 cursor_pos) {
  if (composition_listener_)
    composition_listener_->OnSetComposition(this, text, attributes, cursor_pos);
}

void IMEContext::ForwardKeyEvent(const KeyEvent& event) {
  if (forward_key_event_listener_)
    forward_key_event_listener_->OnForwardKeyEvent(this, event);
}

bool IMEContext::SetSurroundingActive(bool active) {
  if (surrounding_listener_)
    return surrounding_listener_->OnSetSurroundingActive(this, active);
  return false;
}

bool IMEContext::DeleteSurrounding(int offset, int nchars) {
  if (surrounding_listener_)
    return surrounding_listener_->OnDeleteSurrounding(this, offset, nchars);
  return false;
}

#ifdef USE_DUMMY_IME_CONTEXT
IMEContext* IMEContext::Create(View* view) {
  return new DummyIMEContext(view);
}
#endif  // USE_DUMMY_IME_CONTEXT

}  // namespace views
