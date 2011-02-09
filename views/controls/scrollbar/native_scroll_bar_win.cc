// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/scrollbar/native_scroll_bar_win.h"

#include <algorithm>
#include <string>

#include "base/message_loop.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/win/window_impl.h"
#include "views/controls/scrollbar/native_scroll_bar.h"
#include "views/controls/scrollbar/scroll_bar.h"
#include "views/widget/widget.h"

namespace views {

/////////////////////////////////////////////////////////////////////////////
//
// ScrollBarContainer
//
// Since windows scrollbars only send notifications to their parent hwnd, we
// use instances of this class to wrap native scrollbars.
//
/////////////////////////////////////////////////////////////////////////////
class ScrollBarContainer : public ui::WindowImpl {
 public:
  explicit ScrollBarContainer(ScrollBar* parent)
      : parent_(parent),
        scrollbar_(NULL) {
    set_window_style(WS_CHILD);
    Init(parent->GetWidget()->GetNativeView(), gfx::Rect());
    ShowWindow(hwnd(), SW_SHOW);
  }

  virtual ~ScrollBarContainer() {
  }

  BEGIN_MSG_MAP_EX(ScrollBarContainer);
    MSG_WM_CREATE(OnCreate);
    MSG_WM_ERASEBKGND(OnEraseBkgnd);
    MSG_WM_PAINT(OnPaint);
    MSG_WM_SIZE(OnSize);
    MSG_WM_HSCROLL(OnHorizScroll);
    MSG_WM_VSCROLL(OnVertScroll);
  END_MSG_MAP();

  HWND GetScrollBarHWND() {
    return scrollbar_;
  }

  // Invoked when the scrollwheel is used
  void ScrollWithOffset(int o) {
    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_POS;
    GetScrollInfo(scrollbar_, SB_CTL, &si);
    int pos = si.nPos - o;

    if (pos < parent_->GetMinPosition())
      pos = parent_->GetMinPosition();
    else if (pos > parent_->GetMaxPosition())
      pos = parent_->GetMaxPosition();

    ScrollBarController* sbc = parent_->GetController();
    sbc->ScrollToPosition(parent_, pos);

    si.nPos = pos;
    si.fMask = SIF_POS;
    SetScrollInfo(scrollbar_, SB_CTL, &si, TRUE);
  }

 private:

  LRESULT OnCreate(LPCREATESTRUCT create_struct) {
    scrollbar_ = CreateWindow(L"SCROLLBAR", L"",
                              WS_CHILD | (parent_->IsHorizontal() ?
                                          SBS_HORZ : SBS_VERT),
                              0, 0, parent_->width(), parent_->height(),
                              hwnd(), NULL, NULL, NULL);
    ShowWindow(scrollbar_, SW_SHOW);
    return 1;
  }

  LRESULT OnEraseBkgnd(HDC dc) {
    return 1;
  }

  void OnPaint(HDC ignore) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd(), &ps);
    EndPaint(hwnd(), &ps);
  }

  void OnSize(int type, const CSize& sz) {
    SetWindowPos(scrollbar_,
                 0, 0, 0, sz.cx, sz.cy,
                 SWP_DEFERERASE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                 SWP_NOOWNERZORDER | SWP_NOSENDCHANGING | SWP_NOZORDER);
  }

  void OnScroll(int code, HWND source, bool is_horizontal) {
    int pos;

    if (code == SB_ENDSCROLL) {
      return;
    }

    // If we receive an event from the scrollbar, make the view
    // component focused so we actually get mousewheel events.
    if (source != NULL) {
      Widget* widget = parent_->GetWidget();
      if (widget && widget->GetNativeView() != GetFocus()) {
        parent_->RequestFocus();
      }
    }

    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_POS | SIF_TRACKPOS;
    GetScrollInfo(scrollbar_, SB_CTL, &si);
    pos = si.nPos;

    ScrollBarController* sbc = parent_->GetController();

    switch (code) {
      case SB_BOTTOM:  // case SB_RIGHT:
        pos = parent_->GetMaxPosition();
        break;
      case SB_TOP:  // case SB_LEFT:
        pos = parent_->GetMinPosition();
        break;
      case SB_LINEDOWN:  //  case SB_LINERIGHT:
        pos += sbc->GetScrollIncrement(parent_, false, true);
        pos = std::min(parent_->GetMaxPosition(), pos);
        break;
      case SB_LINEUP:  //  case SB_LINELEFT:
        pos -= sbc->GetScrollIncrement(parent_, false, false);
        pos = std::max(parent_->GetMinPosition(), pos);
        break;
      case SB_PAGEDOWN:  //  case SB_PAGERIGHT:
        pos += sbc->GetScrollIncrement(parent_, true, true);
        pos = std::min(parent_->GetMaxPosition(), pos);
        break;
      case SB_PAGEUP:  // case SB_PAGELEFT:
        pos -= sbc->GetScrollIncrement(parent_, true, false);
        pos = std::max(parent_->GetMinPosition(), pos);
        break;
      case SB_THUMBPOSITION:
      case SB_THUMBTRACK:
        pos = si.nTrackPos;
        if (pos < parent_->GetMinPosition())
          pos = parent_->GetMinPosition();
        else if (pos > parent_->GetMaxPosition())
          pos = parent_->GetMaxPosition();
        break;
      default:
        break;
    }

    sbc->ScrollToPosition(parent_, pos);

    si.nPos = pos;
    si.fMask = SIF_POS;
    SetScrollInfo(scrollbar_, SB_CTL, &si, TRUE);

    // Note: the system scrollbar modal loop doesn't give a chance
    // to our message_loop so we need to call DidProcessMessage()
    // manually.
    //
    // Sadly, we don't know what message has been processed. We may
    // want to remove the message from DidProcessMessage()
    MSG dummy;
    dummy.hwnd = NULL;
    dummy.message = 0;
    MessageLoopForUI::current()->DidProcessMessage(dummy);
  }

  // note: always ignore 2nd param as it is 16 bits
  void OnHorizScroll(int n_sb_code, int ignore, HWND source) {
    OnScroll(n_sb_code, source, true);
  }

  // note: always ignore 2nd param as it is 16 bits
  void OnVertScroll(int n_sb_code, int ignore, HWND source) {
    OnScroll(n_sb_code, source, false);
  }

  ScrollBar* parent_;
  HWND scrollbar_;

  DISALLOW_COPY_AND_ASSIGN(ScrollBarContainer);
};

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBarWin, public:

NativeScrollBarWin::NativeScrollBarWin(NativeScrollBar* scroll_bar)
    : native_scroll_bar_(scroll_bar),
      sb_container_(NULL) {
  set_focus_view(scroll_bar);
  memset(&scroll_info_, 0, sizeof(scroll_info_));
}

NativeScrollBarWin::~NativeScrollBarWin() {
  if (sb_container_.get()) {
    // We always destroy the scrollbar container explicitly to cover all
    // cases including when the container is no longer connected to a
    // widget tree.
    DestroyWindow(sb_container_->hwnd());
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBarWin, View overrides:

void NativeScrollBarWin::Layout() {
  SetBoundsRect(native_scroll_bar_->GetLocalBounds());
  NativeControlWin::Layout();
}

gfx::Size NativeScrollBarWin::GetPreferredSize() {
  if (native_scroll_bar_->IsHorizontal())
    return gfx::Size(0, GetHorizontalScrollBarHeight());
  return gfx::Size(GetVerticalScrollBarWidth(), 0);
}

bool NativeScrollBarWin::OnKeyPressed(const KeyEvent& event) {
  if (!sb_container_.get())
    return false;
  int code = -1;
  switch (event.GetKeyCode()) {
    case ui::VKEY_UP:
      if (!native_scroll_bar_->IsHorizontal())
        code = SB_LINEUP;
      break;
    case ui::VKEY_PRIOR:
      code = SB_PAGEUP;
      break;
    case ui::VKEY_NEXT:
      code = SB_PAGEDOWN;
      break;
    case ui::VKEY_DOWN:
      if (!native_scroll_bar_->IsHorizontal())
        code = SB_LINEDOWN;
      break;
    case ui::VKEY_HOME:
      code = SB_TOP;
      break;
    case ui::VKEY_END:
      code = SB_BOTTOM;
      break;
    case ui::VKEY_LEFT:
      if (native_scroll_bar_->IsHorizontal())
        code = SB_LINELEFT;
      break;
    case ui::VKEY_RIGHT:
      if (native_scroll_bar_->IsHorizontal())
        code = SB_LINERIGHT;
      break;
  }
  if (code != -1) {
    SendMessage(sb_container_->hwnd(),
                native_scroll_bar_->IsHorizontal() ? WM_HSCROLL : WM_VSCROLL,
                MAKELONG(static_cast<WORD>(code), 0), 0L);
    return true;
  }
  return false;
}

bool NativeScrollBarWin::OnMouseWheel(const MouseWheelEvent& e) {
  if (!sb_container_.get())
    return false;
  sb_container_->ScrollWithOffset(e.GetOffset());
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBarWin, NativeControlWin overrides:

void NativeScrollBarWin::CreateNativeControl() {
  sb_container_.reset(new ScrollBarContainer(native_scroll_bar_));
  NativeControlCreated(sb_container_->hwnd());
  // Reinstall scroll state if we have valid information.
  if (scroll_info_.cbSize)
    SetScrollInfo(sb_container_->GetScrollBarHWND(), SB_CTL, &scroll_info_,
                  TRUE);
}

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBarWin, NativeScrollBarWrapper overrides:

int NativeScrollBarWin::GetPosition() const {
  SCROLLINFO si;
  si.cbSize = sizeof(si);
  si.fMask = SIF_POS;
  GetScrollInfo(sb_container_->GetScrollBarHWND(), SB_CTL, &si);
  return si.nPos;
}

View* NativeScrollBarWin::GetView() {
  return this;
}

void NativeScrollBarWin::Update(int viewport_size,
                                int content_size,
                                int current_pos) {
  if (!sb_container_.get())
    return;

  if (content_size < 0)
    content_size = 0;

  if (current_pos < 0)
    current_pos = 0;

  if (current_pos > content_size)
    current_pos = content_size;

  scroll_info_.cbSize = sizeof(scroll_info_);
  scroll_info_.fMask = SIF_DISABLENOSCROLL | SIF_POS | SIF_RANGE | SIF_PAGE;
  scroll_info_.nMin = 0;
  scroll_info_.nMax = content_size;
  scroll_info_.nPos = current_pos;
  scroll_info_.nPage = viewport_size;
  SetScrollInfo(sb_container_->GetScrollBarHWND(), SB_CTL, &scroll_info_, TRUE);
}

////////////////////////////////////////////////////////////////////////////////
// NativewScrollBarWrapper, public:

// static
NativeScrollBarWrapper* NativeScrollBarWrapper::CreateWrapper(
    NativeScrollBar* scroll_bar) {
  return new NativeScrollBarWin(scroll_bar);
}

// static
int NativeScrollBarWrapper::GetHorizontalScrollBarHeight() {
  return GetSystemMetrics(SM_CYHSCROLL);
}

// static
int NativeScrollBarWrapper::GetVerticalScrollBarWidth() {
  return GetSystemMetrics(SM_CXVSCROLL);
}

}  // namespace views
