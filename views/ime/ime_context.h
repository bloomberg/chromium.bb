// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_IME_IME_CONTEXT_H_
#define VIEWS_IME_IME_CONTEXT_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Rect;
}

namespace views {

class IMEContext;
class KeyEvent;
class View;

// Duplicate WebKit::WebCompositionUnderline
struct CompositionUnderline {
  CompositionUnderline()
    : start_offset(0),
      end_offset(0),
      color(0),
      thick(false) {}

  CompositionUnderline(uint32 s, uint32 e, SkColor c, bool t)
    : start_offset(s),
      end_offset(e),
      color(c),
      thick(t) {}

  uint32 start_offset;
  uint32 end_offset;
  SkColor color;
  bool thick;
};

// WebKit only supports underline attribue for composition, so we use
// CompositionUnderline as CompositionAttribute right now.
typedef struct CompositionUnderline CompositionAttribute;
typedef std::vector<CompositionAttribute> CompositionAttributeList;

// TODO(penghuang) more attributes (background, foreground color and etc)
// class CompositionAttribute {
//  public:
//   enum CompositionAttributeType{
//     CAT_UNDERLINE = 1,
//     CAT_FORGROUND = 2,
//     CAT_BACKGRUND = 3,
//   };
//
//   CompositionAttributeType GetType() const { return type_; }
//   unsigned GetStartOffset() const { return start_offset_; }
//   unsigned GetEndOffset() const { return end_offset_; }
//   unsigned GetValue() const { return value_; }
//
//  private:
//   CompositionAttributeType type_;
//   unsigned start_offset_;
//   unsigned end_offset_;
//   unsigned value_;
// };

// CommitTextListener is notified when a text is commited from the context.
class CommitTextListener {
 public:
  virtual void OnCommitText(IMEContext* sender,
                            const string16& text) = 0;

 protected:
  virtual ~CommitTextListener() {}
};

// CompositionListener is notified when composition of the context is changed.
class CompositionListener {
 public:
  virtual void OnStartComposition(IMEContext* sender) = 0;
  virtual void OnEndComposition(IMEContext* sender) = 0;
  virtual void OnSetComposition(IMEContext* sender,
                                const string16& text,
                                const CompositionAttributeList& attributes,
                                uint32 cursor_pos) = 0;

 protected:
  virtual ~CompositionListener() {}
};

// CompositionListener is notified when a key event is forwarded from
// the context.
class ForwardKeyEventListener {
 public:
  virtual void OnForwardKeyEvent(IMEContext* sender,
                                 const KeyEvent& event) = 0;

 protected:
  virtual ~ForwardKeyEventListener() {}
};

// SurroundingListener is notified when an input method context needs to
// manipulate the text surround the input cursor. If associated view supports
// surrounding, it should set the listener to input method context. Some input
// methods generate different result depends on the chars before or after the
// input cursor.
//
// For example:
// Some Korean input method:
//      Key events                  Unicode char
// 1    'C'                          +U314a
// 2    'K'                          +U314f
// 3    'C' 'K'                      +Ucc28
//
// In case 2 and 3, when users press 'K', the input method will check the char
// before input cursor. If the char is +U314a, it will remove the char and
// insert a new char +Ucc28. If no char before input cursor, it will insert char
// +U314f.
//
// See also OnSetSurroundingActive() and OnDeleteSurrounding().
class SurroundingListener {
 public:
  // Activate or inactivate surrounding support. When surrounding is activated,
  // The assoicated view should call SetSurrounding() to notify any changes of
  // text surround the input cursor.
  // Return true if associated view can support surrounding.
  virtual bool OnSetSurroundingActive(IMEContext* sender,
                                      bool activate) = 0;

  // Delete a picse of text surround the input cursor.
  // Return true if associated view delete the surrounding text successfully.
  virtual bool OnDeleteSurrounding(IMEContext* sender,
                                   int offset,
                                   int chars) = 0;
 protected:
  virtual ~SurroundingListener() {}
};

class IMEContext {
 public:
  virtual ~IMEContext() {}

  // Set associated view.
  void set_view(View* view) { view_ = view; }

  // Get associated view.
  const View* view() const { return view_; }

  // Set a listener to receive a callback when im context commits a text.
  void set_commit_text_listener(CommitTextListener* listener) {
    commit_text_listener_ = listener;
  }

  // Set a listener to receive a callback when im context changes composition.
  void set_composition_listener(CompositionListener* listener) {
    composition_listener_ = listener;
  }

  // Set a listener to receive a callback when im context forwards a key event.
  void set_forward_key_event_listener(ForwardKeyEventListener* listener) {
    forward_key_event_listener_ = listener;
  }

  // Set a listener to receive a callback when im context need operater
  // surrounding.
  void set_surrounding_listener(SurroundingListener* listener) {
    surrounding_listener_ = listener;
  }

  // Tell the context it got/lost focus.
  virtual void Focus() = 0;
  virtual void Blur() = 0;

  // Reset context, context will be reset to initial state.
  virtual void Reset()  = 0;

  // Filter key event, returns true, if the key event is handled,
  // associated widget should ignore it.
  virtual bool FilterKeyEvent(const views::KeyEvent& event) = 0;

  // Set text input cursor location on screen.
  virtual void SetCursorLocation(const gfx::Rect& caret_rect) = 0;

  // Set surrounding context.
  virtual void SetSurrounding(const string16& text,
                              int cursor_pos) = 0;

  // Create an im context.
  static IMEContext* Create(View* view);

 protected:
  explicit IMEContext(View* view);

  // Commit text.
  void CommitText(const string16& text);

  // Start compostion.
  void StartComposition();

  // End composition.
  void EndComposition();

  // Set composition.
  void SetComposition(const string16& text,
                      const CompositionAttributeList& attributes,
                      uint32 cursor_pos);

  // Forward key event.
  void ForwardKeyEvent(const KeyEvent& event);

  // Notify the associated view whether or not the input context needs
  // surrounding. When surrounding is activated, associated view should
  // call SetSurrounding() to update any changes of text arround the input
  // cursor.
  // Return true if associated view can support surrounding.
  bool SetSurroundingActive(bool activate);

  // Delete surrouding arround cursor. Return true, if it is handled.
  bool DeleteSurrounding(int offset, int nchars);

 private:
  // Client view.
  View* view_;

  // Listeners:
  CommitTextListener* commit_text_listener_;
  CompositionListener* composition_listener_;
  ForwardKeyEventListener* forward_key_event_listener_;
  SurroundingListener* surrounding_listener_;

  DISALLOW_COPY_AND_ASSIGN(IMEContext);
};

}  // namespace views

#endif  // VIEWS_IME_IM_CONTEXT_H_
