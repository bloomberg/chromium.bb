// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/textfield/native_textfield_wrapper.h"
#include "ui/views/view.h"

#if !defined(OS_LINUX)
#include "base/logging.h"
#endif

namespace gfx {
class Range;
class ImageSkia;
}

namespace ui {
class TextInputClient;
}  // namespace ui

namespace views {

class ImageView;
class Painter;
class TextfieldController;

// This class implements a View that wraps a native text (edit) field.
class VIEWS_EXPORT Textfield : public View {
 public:
  // The textfield's class name.
  static const char kViewClassName[];

  enum StyleFlags {
    STYLE_DEFAULT   = 0,
    STYLE_OBSCURED  = 1 << 0,
    STYLE_LOWERCASE = 1 << 1
  };

  // Returns the text cursor blink time in milliseconds, or 0 for no blinking.
  static size_t GetCaretBlinkMs();

  Textfield();
  explicit Textfield(StyleFlags style);
  virtual ~Textfield();

  // TextfieldController accessors
  void SetController(TextfieldController* controller);
  TextfieldController* GetController() const;

  // Gets/Sets whether or not the Textfield is read-only.
  bool read_only() const { return read_only_; }
  void SetReadOnly(bool read_only);

  // Gets/sets the STYLE_OBSCURED bit, controlling whether characters in this
  // Textfield are displayed as asterisks/bullets.
  bool IsObscured() const;
  void SetObscured(bool obscured);

  // Gets/sets the duration to reveal the last typed char when the obscured bit
  // is set. A duration of zero effectively disables the feature. Other values
  // cause the last typed char to be shown for the defined duration. Note this
  // only works with NativeTextfieldViews.
  const base::TimeDelta& obscured_reveal_duration() const {
    return obscured_reveal_duration_;
  }
  void set_obscured_reveal_duration(const base::TimeDelta& duration) {
    obscured_reveal_duration_ = duration;
  }

  // Gets/Sets the input type of this textfield.
  ui::TextInputType GetTextInputType() const;
  void SetTextInputType(ui::TextInputType type);

  // Gets/Sets the text currently displayed in the Textfield.
  const string16& text() const { return text_; }

  // Sets the text currently displayed in the Textfield.  This doesn't
  // change the cursor position if the current cursor is within the
  // new text's range, or moves the cursor to the end if the cursor is
  // out of the new text's range.
  void SetText(const string16& text);

  // Appends the given string to the previously-existing text in the field.
  void AppendText(const string16& text);

  // Inserts |text| at the current cursor position, replacing any selected text.
  void InsertOrReplaceText(const string16& text);

  // Returns the text direction.
  base::i18n::TextDirection GetTextDirection() const;

  // Returns the text that is currently selected.
  string16 GetSelectedText() const;

  // Select the entire text range. If |reversed| is true, the range will end at
  // the logical beginning of the text; this generally shows the leading portion
  // of text that overflows its display area.
  void SelectAll(bool reversed);

  // Clears the selection within the edit field and sets the caret to the end.
  void ClearSelection() const;

  // Checks if there is any selected text.
  bool HasSelection() const;

  // Accessor for |style_|.
  StyleFlags style() const { return style_; }

  // Gets/Sets the text color to be used when painting the Textfield.
  // Call |UseDefaultTextColor| to restore the default system color.
  SkColor GetTextColor() const;
  void SetTextColor(SkColor color);
  void UseDefaultTextColor();

  // Gets/Sets the background color to be used when painting the Textfield.
  // Call |UseDefaultBackgroundColor| to restore the default system color.
  SkColor GetBackgroundColor() const;
  void SetBackgroundColor(SkColor color);
  void UseDefaultBackgroundColor();

  // Gets/Sets whether or not the cursor is enabled.
  bool GetCursorEnabled() const;
  void SetCursorEnabled(bool enabled);

  // Gets/Sets the fonts used when rendering the text within the Textfield.
  const gfx::FontList& font_list() const { return font_list_; }
  void SetFontList(const gfx::FontList& font_list);
  const gfx::Font& GetPrimaryFont() const;
  void SetFont(const gfx::Font& font);

  // Sets the left and right margin (in pixels) within the text box. On Windows
  // this is accomplished by packing the left and right margin into a single
  // 32 bit number, so the left and right margins are effectively 16 bits.
  void SetHorizontalMargins(int left, int right);

  // Sets the top and bottom margins (in pixels) within the textfield.
  // NOTE: in most cases height could be changed instead.
  void SetVerticalMargins(int top, int bottom);

  // Sets the default width of the text control. See default_width_in_chars_.
  void set_default_width_in_chars(int default_width) {
    default_width_in_chars_ = default_width;
  }

  // Removes the border from the edit box, giving it a 2D look.
  bool draw_border() const { return draw_border_; }
  void RemoveBorder();

  // Sets the text to display when empty.
  void set_placeholder_text(const string16& text) {
    placeholder_text_ = text;
  }
  virtual base::string16 GetPlaceholderText() const;

  SkColor placeholder_text_color() const { return placeholder_text_color_; }
  void set_placeholder_text_color(SkColor color) {
    placeholder_text_color_ = color;
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
  // TODO(msw): Return a const reference when NativeTextfieldWin is gone.
  gfx::Range GetSelectedRange() const;

  // Selects the text given by |range|. This is views-implementation only and
  // has to be called after the wrapper is created.
  void SelectRange(const gfx::Range& range);

  // Gets the selection model. This is views-implementation only and
  // has to be called after the wrapper is created.
  // TODO(msw): Return a const reference when NativeTextfieldWin is gone.
  gfx::SelectionModel GetSelectionModel() const;

  // Selects the text given by |sel|. This is views-implementation only and
  // has to be called after the wrapper is created.
  void SelectSelectionModel(const gfx::SelectionModel& sel);

  // Returns the current cursor position. This is views-implementation
  // only and has to be called after the wrapper is created.
  size_t GetCursorPosition() const;

  // Set the text color over the entire text or a logical character range.
  // Empty and invalid ranges are ignored. This is views-implementation only and
  // has to be called after the wrapper is created.
  void SetColor(SkColor value);
  void ApplyColor(SkColor value, const gfx::Range& range);

  // Set various text styles over the entire text or a logical character range.
  // The respective |style| is applied if |value| is true, or removed if false.
  // Empty and invalid ranges are ignored. This is views-implementation only and
  // has to be called after the wrapper is created.
  void SetStyle(gfx::TextStyle style, bool value);
  void ApplyStyle(gfx::TextStyle style, bool value, const gfx::Range& range);

  // Clears Edit history.
  void ClearEditHistory();

  // Set the accessible name of the text field.
  void SetAccessibleName(const string16& name);

  // Performs the action associated with the specified command id.
  void ExecuteCommand(int command_id);

  void SetFocusPainter(scoped_ptr<Painter> focus_painter);

  // Provided only for testing:
  gfx::NativeView GetTestingHandle() const {
    return native_wrapper_ ? native_wrapper_->GetTestingHandle() : NULL;
  }
  NativeTextfieldWrapper* GetNativeWrapperForTesting() const {
    return native_wrapper_;
  }

  // Returns whether there is a drag operation originating from the textfield.
  bool HasTextBeingDragged();

  // Overridden from View:
  virtual void Layout() OVERRIDE;
  virtual int GetBaseline() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse) OVERRIDE;
  virtual bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) OVERRIDE;
  virtual void OnEnabledChanged() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& e) OVERRIDE;
  virtual bool OnKeyReleased(const ui::KeyEvent& e) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& e) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual ui::TextInputClient* GetTextInputClient() OVERRIDE;
  virtual gfx::Point GetKeyboardContextMenuLocation() OVERRIDE;

 protected:
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

  // The object that actually implements the native text field.
  NativeTextfieldWrapper* native_wrapper_;

 private:
  // Returns the insets to the rectangle where text is actually painted.
  gfx::Insets GetTextInsets() const;

  // Handles a request to change the value of this text field from software
  // using an accessibility API (typically automation software, screen readers
  // don't normally use this). Sets the value and clears the selection.
  void AccessibilitySetValue(const string16& new_value);

  // This is the current listener for events from this Textfield.
  TextfieldController* controller_;

  // The mask of style options for this Textfield.
  StyleFlags style_;

  // The fonts used to render the text in the Textfield.
  gfx::FontList font_list_;

  // The text displayed in the Textfield.
  string16 text_;

  // True if this Textfield cannot accept input and is read-only.
  bool read_only_;

  // The default number of average characters for the width of this text field.
  // This will be reported as the "desired size". Defaults to 0.
  int default_width_in_chars_;

  // Whether the border is drawn.
  bool draw_border_;

  // Text color.  Only used if |use_default_text_color_| is false.
  SkColor text_color_;

  // Should we use the system text color instead of |text_color_|?
  bool use_default_text_color_;

  // Background color.  Only used if |use_default_background_color_| is false.
  SkColor background_color_;

  // Should we use the system background color instead of |background_color_|?
  bool use_default_background_color_;

  // Holds inner textfield margins.
  gfx::Insets margins_;

  // Holds whether margins were set.
  bool horizontal_margins_were_set_;
  bool vertical_margins_were_set_;

  // Text to display when empty.
  string16 placeholder_text_;

  // Placeholder text color.
  SkColor placeholder_text_color_;

  // The accessible name of the text field.
  string16 accessible_name_;

  // The input type of this text field.
  ui::TextInputType text_input_type_;

  // The duration to reveal the last typed char for obscured textfields.
  base::TimeDelta obscured_reveal_duration_;

  // Used to bind callback functions to this object.
  base::WeakPtrFactory<Textfield> weak_ptr_factory_;

  scoped_ptr<Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(Textfield);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_
