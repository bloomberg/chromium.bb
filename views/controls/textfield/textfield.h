// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_
#define VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_LINUX)
#include <gdk/gdk.h>
#endif

#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/font.h"
#include "views/view.h"

#if !defined(OS_LINUX)
#include "base/logging.h"
#endif
#ifdef UNIT_TEST
#include "ui/gfx/native_widget_types.h"
#include "views/controls/textfield/native_textfield_wrapper.h"
#endif

namespace views {

class KeyEvent;
class NativeTextfieldWrapper;

// TextRange specifies the range of text in the Textfield.  This is
// used to specify selected text and will be used to change the
// attributes of characters in the textfield. When this is used for
// selection, the end is caret position, and the start is where
// selection started.  The range preserves the direction, and
// selecting from the end to the begining is considered "reverse"
// order. (that is, start > end is reverse)
class TextRange {
 public:
  TextRange() : start_(0), end_(0) {}
  TextRange(size_t start, size_t end);

  // Allow copy so that the omnibox can save the view state
  // for each tabs.
  explicit TextRange(const TextRange& range)
      : start_(range.start_), end_(range.end_) {}

  // Returns the start position;
  size_t start() const { return start_; }

  // Returns the end position.
  size_t end() const { return end_; }

  // Returns true if the selected text is empty.
  bool is_empty() const { return start_ == end_; }

  // Returns true if the selection is made in reverse order.
  bool is_reverse() const { return start_ > end_; }

  // Returns the min of selected range.
  size_t GetMin() const;

  // Returns the max of selected range.
  size_t GetMax() const;

  // Returns true if the the selection range is same ignoring the direction.
  bool EqualsIgnoringDirection(const TextRange& range) const {
    return GetMin() == range.GetMin() && GetMax() == range.GetMax();
  }

  // Set the range with |start| and |end|.
  void SetRange(size_t start, size_t end);

 private:
  size_t start_;
  size_t end_;

  // No assign.
  void operator=(const TextRange&);
};

// This class implements a ChromeView that wraps a native text (edit) field.
class Textfield : public View {
 public:
  // The button's class name.
  static const char kViewClassName[];

  // This defines the callback interface for other code to be notified of
  // changes in the state of a text field.
  class Controller {
   public:
    // This method is called whenever the text in the field changes.
    virtual void ContentsChanged(Textfield* sender,
                                 const string16& new_contents) = 0;

    // This method is called to get notified about keystrokes in the edit.
    // This method returns true if the message was handled and should not be
    // processed further. If it returns false the processing continues.
    virtual bool HandleKeyEvent(Textfield* sender,
                                const KeyEvent& key_event) = 0;
  };

  enum StyleFlags {
    STYLE_DEFAULT = 0,
    STYLE_PASSWORD = 1<<0,
    STYLE_MULTILINE = 1<<1,
    STYLE_LOWERCASE = 1<<2
  };

  Textfield();
  explicit Textfield(StyleFlags style);
  virtual ~Textfield();


  // Controller accessors
  void SetController(Controller* controller);
  Controller* GetController() const;

  // Gets/Sets whether or not the Textfield is read-only.
  bool read_only() const { return read_only_; }
  void SetReadOnly(bool read_only);

  // Gets/Sets whether or not this Textfield is a password field.
  bool IsPassword() const;
  void SetPassword(bool password);

  // Whether the text field is multi-line or not, must be set when the text
  // field is created, using StyleFlags.
  bool IsMultiLine() const;

  // Gets/Sets the text currently displayed in the Textfield.
  const string16& text() const { return text_; }
  void SetText(const string16& text);

  // Appends the given string to the previously-existing text in the field.
  void AppendText(const string16& text);

  // Returns the text that is currently selected.
  string16 GetSelectedText() const;

  // Causes the edit field to be fully selected.
  void SelectAll();

  // Clears the selection within the edit field and sets the caret to the end.
  void ClearSelection() const;

  // Accessor for |style_|.
  StyleFlags style() const { return style_; }

  // Gets/Sets the text color to be used when painting the Textfield.
  // Call |UseDefaultTextColor| to return to the system default colors.
  SkColor text_color() const { return text_color_; }
  void SetTextColor(SkColor color);

  // Gets/Sets whether the default text color should be used when painting the
  // Textfield.
  bool use_default_text_color() const {
    return use_default_text_color_;
  }
  void UseDefaultTextColor();

  // Gets/Sets the background color to be used when painting the Textfield.
  // Call |UseDefaultBackgroundColor| to return to the system default colors.
  SkColor background_color() const { return background_color_; }
  void SetBackgroundColor(SkColor color);

  // Gets/Sets whether the default background color should be used when painting
  // the Textfield.
  bool use_default_background_color() const {
    return use_default_background_color_;
  }
  void UseDefaultBackgroundColor();

  // Gets/Sets the font used when rendering the text within the Textfield.
  const gfx::Font& font() const { return font_; }
  void SetFont(const gfx::Font& font);

  // Sets the left and right margin (in pixels) within the text box. On Windows
  // this is accomplished by packing the left and right margin into a single
  // 32 bit number, so the left and right margins are effectively 16 bits.
  void SetHorizontalMargins(int left, int right);

  // Sets the top and bottom margins (in pixels) within the textfield.
  // NOTE: in most cases height could be changed instead.
  void SetVerticalMargins(int top, int bottom);

  // Should only be called on a multi-line text field. Sets how many lines of
  // text can be displayed at once by this text field.
  void SetHeightInLines(int num_lines);

  // Sets the default width of the text control. See default_width_in_chars_.
  void set_default_width_in_chars(int default_width) {
    default_width_in_chars_ = default_width;
  }

  // Removes the border from the edit box, giving it a 2D look.
  bool draw_border() const { return draw_border_; }
  void RemoveBorder();

  // Sets the text to display when empty.
  void set_text_to_display_when_empty(const string16& text) {
    text_to_display_when_empty_ = text;
#if !defined(OS_LINUX)
    NOTIMPLEMENTED();
#endif
  }
  const string16& text_to_display_when_empty() {
    return text_to_display_when_empty_;
  }

  // Getter for the horizontal margins that were set. Returns false if
  // horizontal margins weren't set.
  bool GetHorizontalMargins(int* left, int* right);

  // Getter for the vertical margins that were set. Returns false if vertical
  // margins weren't set.
  bool GetVerticalMargins(int* top, int* bottom);

  // Updates all properties on the textfield. This is invoked internally.
  // Users of Textfield never need to invoke this directly.
  void UpdateAllProperties();

  // Invoked by the edit control when the value changes. This method set
  // the text_ member variable to the value contained in edit control.
  // This is important because the edit control can be replaced if it has
  // been deleted during a window close.
  void SyncText();

  // Returns whether or not an IME is composing text.
  bool IsIMEComposing() const;

  // Gets the selected range. This is views-implementation only and
  // has to be called after the wrapper is created.
  void GetSelectedRange(TextRange* range) const;

  // Selects the text given by |range|. This is views-implementation only and
  // has to be called after the wrapper is created.
  void SelectRange(const TextRange& range);

  // Returns the current cursor position. This is views-implementation
  // only and has to be called after the wrapper is created.
  size_t GetCursorPosition() const;

#ifdef UNIT_TEST
  gfx::NativeView GetTestingHandle() const {
    return native_wrapper_ ? native_wrapper_->GetTestingHandle() : NULL;
  }
  NativeTextfieldWrapper* native_wrapper() const {
    return native_wrapper_;
  }
#endif

  // Overridden from View:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual bool IsFocusable() const;
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse);
  virtual bool SkipDefaultKeyEventProcessing(const KeyEvent& e);
  virtual void SetEnabled(bool enabled);
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas);
  virtual bool OnKeyPressed(const views::KeyEvent& e);
  virtual bool OnKeyReleased(const views::KeyEvent& e);
  virtual void WillGainFocus();
  virtual void DidGainFocus();
  virtual void WillLoseFocus();

  // Accessibility accessors, overridden from View:
  virtual AccessibilityTypes::Role GetAccessibleRole() OVERRIDE;
  virtual AccessibilityTypes::State GetAccessibleState() OVERRIDE;
  virtual string16 GetAccessibleValue() OVERRIDE;

 protected:
  virtual void Focus();
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual std::string GetClassName() const;

  // The object that actually implements the native text field.
  NativeTextfieldWrapper* native_wrapper_;

 private:
  // This is the current listener for events from this Textfield.
  Controller* controller_;

  // The mask of style options for this Textfield.
  StyleFlags style_;

  // The font used to render the text in the Textfield.
  gfx::Font font_;

  // The text displayed in the Textfield.
  string16 text_;

  // True if this Textfield cannot accept input and is read-only.
  bool read_only_;

  // The default number of average characters for the width of this text field.
  // This will be reported as the "desired size". Defaults to 0.
  int default_width_in_chars_;

  // Whether the border is drawn.
  bool draw_border_;

  // The text color to be used when painting the Textfield, provided
  // |use_default_text_color_| is set to false.
  SkColor text_color_;

  // When true, the system text color for Textfields is used when painting this
  // Textfield. When false, the value of |text_color_| determines the
  // Textfield's text color.
  bool use_default_text_color_;

  // The background color to be used when painting the Textfield, provided
  // |use_default_background_color_| is set to false.
  SkColor background_color_;

  // When true, the system background color for Textfields is used when painting
  // this Textfield. When false, the value of |background_color_| determines the
  // Textfield's background color.
  bool use_default_background_color_;

  // The number of lines of text this Textfield displays at once.
  int num_lines_;

  // TODO(beng): remove this once NativeTextfieldWin subclasses
  //             NativeControlWin.
  bool initialized_;

  // Holds inner textfield margins.
  gfx::Insets margins_;

  // Holds whether margins were set.
  bool horizontal_margins_were_set_;
  bool vertical_margins_were_set_;

  // Text to display when empty.
  string16 text_to_display_when_empty_;

  DISALLOW_COPY_AND_ASSIGN(Textfield);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_
