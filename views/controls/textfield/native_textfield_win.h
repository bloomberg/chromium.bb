// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_WIN_H_
#define VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_WIN_H_
#pragma once

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlmisc.h>
#include <oleacc.h>
#include <tom.h>  // For ITextDocument, a COM interface to CRichEditCtrl
#include <vsstyle.h>

#include "base/win/scoped_comptr.h"
#include "gfx/insets.h"
#include "ui/base/models/simple_menu_model.h"
#include "views/controls/textfield/native_textfield_wrapper.h"

namespace views {

class Menu2;
class NativeViewHost;
class Textfield;
class TextRange;

static const int kDefaultEditStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN |
    WS_CLIPSIBLINGS;

// TODO(beng): make a subclass of NativeControlWin instead.
class NativeTextfieldWin
    : public CWindowImpl<NativeTextfieldWin, CRichEditCtrl,
                         CWinTraits<kDefaultEditStyle> >,
      public CRichEditCommands<NativeTextfieldWin>,
      public NativeTextfieldWrapper,
      public ui::SimpleMenuModel::Delegate {
 public:
  DECLARE_WND_CLASS(L"ViewsTextfieldEdit");

  explicit NativeTextfieldWin(Textfield* parent);
  ~NativeTextfieldWin();

  // Returns true if the current point is close enough to the origin point in
  // space and time that it would be considered a double click.
  static bool IsDoubleClick(const POINT& origin,
                            const POINT& current,
                            DWORD elapsed_time);

  // Returns true if the virtual key code is a digit coming from the numeric
  // keypad (with or without NumLock on).  |extended_key| should be set to the
  // extended key flag specified in the WM_KEYDOWN/UP where the |key_code|
  // originated.
  static bool IsNumPadDigit(int key_code, bool extended_key);

  // See the code in textfield.cc that calls this for why this is here.
  void AttachHack();

  // Overridden from NativeTextfieldWrapper:
  virtual string16 GetText() const;
  virtual void UpdateText();
  virtual void AppendText(const string16& text);
  virtual string16 GetSelectedText() const;
  virtual void SelectAll();
  virtual void ClearSelection();
  virtual void UpdateBorder();
  virtual void UpdateTextColor();
  virtual void UpdateBackgroundColor();
  virtual void UpdateReadOnly();
  virtual void UpdateFont();
  virtual void UpdateIsPassword();
  virtual void UpdateEnabled();
  virtual gfx::Insets CalculateInsets();
  virtual void UpdateHorizontalMargins();
  virtual void UpdateVerticalMargins();
  virtual bool SetFocus();
  virtual View* GetView();
  virtual gfx::NativeView GetTestingHandle() const;
  virtual bool IsIMEComposing() const;
  virtual void GetSelectedRange(TextRange* range) const;
  virtual void SelectRange(const TextRange& range);
  virtual size_t GetCursorPosition() const;
  virtual bool HandleKeyPressed(const views::KeyEvent& e);
  virtual bool HandleKeyReleased(const views::KeyEvent& e);
  virtual void HandleWillGainFocus();
  virtual void HandleDidGainFocus();
  virtual void HandleWillLoseFocus();

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          ui::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // Update accessibility information.
  void InitializeAccessibilityInfo();
  void UpdateAccessibleState(uint32 state_flag, bool set_value);
  void UpdateAccessibleValue(const std::wstring& value);

  // CWindowImpl
  BEGIN_MSG_MAP(Edit)
    MSG_WM_CHAR(OnChar)
    MSG_WM_CONTEXTMENU(OnContextMenu)
    MSG_WM_COPY(OnCopy)
    MSG_WM_CUT(OnCut)
    MESSAGE_HANDLER_EX(WM_IME_CHAR, OnImeChar)
    MESSAGE_HANDLER_EX(WM_IME_STARTCOMPOSITION, OnImeStartComposition)
    MESSAGE_HANDLER_EX(WM_IME_COMPOSITION, OnImeComposition)
    MESSAGE_HANDLER_EX(WM_IME_ENDCOMPOSITION, OnImeEndComposition)
    MSG_WM_KEYDOWN(OnKeyDown)
    MSG_WM_LBUTTONDBLCLK(OnLButtonDblClk)
    MSG_WM_LBUTTONDOWN(OnLButtonDown)
    MSG_WM_LBUTTONUP(OnLButtonUp)
    MSG_WM_MBUTTONDOWN(OnNonLButtonDown)
    MSG_WM_MOUSEMOVE(OnMouseMove)
    MSG_WM_MOUSELEAVE(OnMouseLeave)
    MESSAGE_HANDLER_EX(WM_MOUSEWHEEL, OnMouseWheel)
    MSG_WM_NCCALCSIZE(OnNCCalcSize)
    MSG_WM_NCPAINT(OnNCPaint)
    MSG_WM_RBUTTONDOWN(OnNonLButtonDown)
    MSG_WM_PASTE(OnPaste)
    MSG_WM_SETFOCUS(OnSetFocus)
    MSG_WM_SYSCHAR(OnSysChar)  // WM_SYSxxx == WM_xxx with ALT down
    MSG_WM_SYSKEYDOWN(OnKeyDown)
  END_MSG_MAP()

 private:
  // This object freezes repainting of the edit until the object is destroyed.
  // Some methods of the CRichEditCtrl draw synchronously to the screen.  If we
  // don't freeze, the user will see a rapid series of calls to these as
  // flickers.
  //
  // Freezing the control while it is already frozen is permitted; the control
  // will unfreeze once both freezes are released (the freezes stack).
  class ScopedFreeze {
   public:
    ScopedFreeze(NativeTextfieldWin* edit, ITextDocument* text_object_model);
    ~ScopedFreeze();

   private:
    NativeTextfieldWin* const edit_;
    ITextDocument* const text_object_model_;

    DISALLOW_COPY_AND_ASSIGN(ScopedFreeze);
  };

  // This object suspends placing any operations on the edit's undo stack until
  // the object is destroyed.  If we don't do this, some of the operations we
  // perform behind the user's back will be undoable by the user, which feels
  // bizarre and confusing.
  class ScopedSuspendUndo {
   public:
    explicit ScopedSuspendUndo(ITextDocument* text_object_model);
    ~ScopedSuspendUndo();

   private:
    ITextDocument* const text_object_model_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSuspendUndo);
  };

  // message handlers
  void OnChar(TCHAR key, UINT repeat_count, UINT flags);
  void OnContextMenu(HWND window, const POINT& point);
  void OnCopy();
  void OnCut();
  LRESULT OnImeChar(UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT OnImeStartComposition(UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT OnImeComposition(UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT OnImeEndComposition(UINT message, WPARAM wparam, LPARAM lparam);
  void OnKeyDown(TCHAR key, UINT repeat_count, UINT flags);
  void OnLButtonDblClk(UINT keys, const CPoint& point);
  void OnLButtonDown(UINT keys, const CPoint& point);
  void OnLButtonUp(UINT keys, const CPoint& point);
  void OnMouseLeave();
  LRESULT OnMouseWheel(UINT message, WPARAM w_param, LPARAM l_param);
  void OnMouseMove(UINT keys, const CPoint& point);
  int OnNCCalcSize(BOOL w_param, LPARAM l_param);
  void OnNCPaint(HRGN region);
  void OnNonLButtonDown(UINT keys, const CPoint& point);
  void OnPaste();
  void OnSetFocus(HWND hwnd);
  void OnSysChar(TCHAR ch, UINT repeat_count, UINT flags);

  // Helper function for OnChar() and OnKeyDown() that handles keystrokes that
  // could change the text in the edit.
  void HandleKeystroke(UINT message, TCHAR key, UINT repeat_count, UINT flags);

  // Every piece of code that can change the edit should call these functions
  // before and after the change.  These functions determine if anything
  // meaningful changed, and do any necessary updating and notification.
  void OnBeforePossibleChange();

  // When a user types a BIDI mirroring character (e.g. left parenthesis
  // U+0028, which should be rendered as '(' in LTR context unless  surrounded
  // by RTL characters in both sides, and it should be rendered as ')' in RTL
  // context unless surrounded by LTR characters in both sides.), the textfield
  // does not properly mirror the character when necessary. However, if we
  // explicitly set the text in the edit to the entire current string, then
  // the BIDI mirroring characters will be mirrored properly. When
  // |should_redraw_text| is true, we explicitly set the text in the edit to
  // the entire current string any time the text changes.
  void OnAfterPossibleChange(bool should_redraw_text);

  // Given an X coordinate in client coordinates, returns that coordinate
  // clipped to be within the horizontal bounds of the visible text.
  //
  // This is used in our mouse handlers to work around quirky behaviors of the
  // underlying CRichEditCtrl like not supporting triple-click when the user
  // doesn't click on the text itself.
  //
  // |is_triple_click| should be true iff this is the third click of a triple
  // click.  Sadly, we need to clip slightly differently in this case.
  LONG ClipXCoordToVisibleText(LONG x, bool is_triple_click) const;

  // Sets whether the mouse is in the edit. As necessary this redraws the
  // edit.
  void SetContainsMouse(bool contains_mouse);

  // Getter for the text_object_model_, used by the ScopedFreeze class.  Note
  // that the pointer returned here is only valid as long as the Edit is still
  // alive.
  ITextDocument* GetTextObjectModel() const;

  // Generates the contents of the context menu.
  void BuildContextMenu();

  // The Textfield this object is bound to.
  Textfield* textfield_;

  // We need to know if the user triple-clicks, so track double click points
  // and times so we can see if subsequent clicks are actually triple clicks.
  bool tracking_double_click_;
  CPoint double_click_point_;
  DWORD double_click_time_;

  // Used to discard unnecessary WM_MOUSEMOVE events after the first such
  // unnecessary event.  See detailed comments in OnMouseMove().
  bool can_discard_mousemove_;

  // The text of this control before a possible change.
  string16 text_before_change_;

  // If true, the mouse is over the edit.
  bool contains_mouse_;

  static bool did_load_library_;

  // The contents of the context menu for the edit.
  scoped_ptr<ui::SimpleMenuModel> context_menu_contents_;
  scoped_ptr<Menu2> context_menu_;

  // Border insets.
  gfx::Insets content_insets_;

  // This interface is useful for accessing the CRichEditCtrl at a low level.
  mutable base::win::ScopedComPtr<ITextDocument> text_object_model_;

  // The position and the length of the ongoing composition string.
  // These values are used for removing a composition string from a search
  // text to emulate Firefox.
  bool ime_discard_composition_;
  int ime_composition_start_;
  int ime_composition_length_;

  // TODO(beng): remove this when we are a subclass of NativeControlWin.
  NativeViewHost* container_view_;

  COLORREF bg_color_;

  //  The accessibility state of this object.
  int accessibility_state_;

  DISALLOW_COPY_AND_ASSIGN(NativeTextfieldWin);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_WIN_H_
