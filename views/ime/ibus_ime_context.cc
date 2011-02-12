// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ibus.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "gfx/rect.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#include "views/events/event.h"
#include "views/ime/ime_context.h"

namespace {

class IBusIMEContext;

struct ProcessKeyEventData {
  ProcessKeyEventData(IBusIMEContext* context,
                      const views::KeyEvent& event)
    : context(context),
      event(event.type(), event.key_code(), event.flags()) {
  }

  IBusIMEContext* context;
  views::KeyEvent event;
};

// Implements IMEContext to integrate ibus input method framework
class IBusIMEContext : public views::IMEContext {
 public:
  explicit IBusIMEContext(views::View* view);
  virtual ~IBusIMEContext();

  // views::IMEContext implementations:
  virtual void Focus();
  virtual void Blur();
  virtual void Reset();
  virtual bool FilterKeyEvent(const views::KeyEvent& event);
  virtual void SetCursorLocation(const gfx::Rect& caret_rect);
  virtual void SetSurrounding(const string16& text, int cursor_pos);

 private:
  void CreateContext();
  void DestroyContext();

  // Event handlers for IBusBus:
  CHROMEG_CALLBACK_0(IBusIMEContext, void, OnConnected, IBusBus*);
  CHROMEG_CALLBACK_0(IBusIMEContext, void, OnDisconnected, IBusBus*);

  // Event handlers for IBusIMEContext:
  CHROMEG_CALLBACK_1(IBusIMEContext, void, OnCommitText,
                     IBusInputContext*, IBusText*);
  CHROMEG_CALLBACK_3(IBusIMEContext, void, OnForwardKeyEvent,
                     IBusInputContext*, guint, guint, guint);
  CHROMEG_CALLBACK_3(IBusIMEContext, void, OnUpdatePreeditText,
                     IBusInputContext*, IBusText*, guint, gboolean);
  CHROMEG_CALLBACK_0(IBusIMEContext, void, OnShowPreeditText,
                     IBusInputContext*);
  CHROMEG_CALLBACK_0(IBusIMEContext, void, OnHidePreeditText,
                     IBusInputContext*);
  CHROMEG_CALLBACK_0(IBusIMEContext, void, OnEnable, IBusInputContext*);
  CHROMEG_CALLBACK_0(IBusIMEContext, void, OnDisable, IBusInputContext*);
  CHROMEG_CALLBACK_0(IBusIMEContext, void, OnDestroy, IBusInputContext*);

  static void ProcessKeyEventDone(IBusInputContext* context,
                                  GAsyncResult* res,
                                  ProcessKeyEventData *data);

  IBusInputContext* context_;
  bool is_focused_;
  gfx::Rect caret_rect_;

  static IBusBus* bus_;

  DISALLOW_COPY_AND_ASSIGN(IBusIMEContext);
};

IBusBus* IBusIMEContext::bus_ = NULL;

IBusIMEContext::IBusIMEContext(views::View* view)
  : views::IMEContext(view),
    context_(NULL),
    is_focused_(false) {
  // init ibus
  if (!bus_) {
    ibus_init();
    bus_ = ibus_bus_new();
  }

  // connect bus signals
  g_signal_connect(bus_,
                   "connected",
                   G_CALLBACK(OnConnectedThunk),
                   this);
  g_signal_connect(bus_,
                   "disconnected",
                   G_CALLBACK(OnDisconnectedThunk),
                   this);

  if (ibus_bus_is_connected(bus_))
    CreateContext();
}

IBusIMEContext::~IBusIMEContext() {
  // disconnect bus signals
  g_signal_handlers_disconnect_by_func(bus_,
      reinterpret_cast<gpointer>(OnConnectedThunk), this);
  g_signal_handlers_disconnect_by_func(bus_,
      reinterpret_cast<gpointer>(OnDisconnectedThunk), this);

  DestroyContext();
}

void IBusIMEContext::Focus() {
  if (is_focused_)
    return;
  is_focused_ = true;

  if (context_)
    ibus_input_context_focus_in(context_);
}

void IBusIMEContext::Blur() {
  if (!is_focused_)
    return;

  is_focused_ = false;

  if (context_)
    ibus_input_context_focus_out(context_);
}

void IBusIMEContext::Reset() {
  if (context_)
    ibus_input_context_reset(context_);
}

bool IBusIMEContext::FilterKeyEvent(const views::KeyEvent& event) {
  guint keyval = ui::GdkKeyCodeForWindowsKeyCode(event.key_code(),
      event.IsShiftDown() ^ event.IsCapsLockDown());

  if (context_) {
    guint modifiers = 0;

    if (event.type() == ui::ET_KEY_RELEASED)
      modifiers |= IBUS_RELEASE_MASK;

    if (event.IsShiftDown())
      modifiers |= IBUS_SHIFT_MASK;
    if (event.IsControlDown())
      modifiers |= IBUS_CONTROL_MASK;
    if (event.IsAltDown())
      modifiers |= IBUS_MOD1_MASK;
    if (event.IsCapsLockDown())
      modifiers |= IBUS_LOCK_MASK;

    ibus_input_context_process_key_event(context_,
        keyval, 0, modifiers,
        -1,
        NULL,
        reinterpret_cast<GAsyncReadyCallback>(ProcessKeyEventDone),
        new ProcessKeyEventData(this, event));
    return true;
  }

  return false;
}

void IBusIMEContext::SetCursorLocation(const gfx::Rect& caret_rect) {
  if (context_ && caret_rect_ != caret_rect) {
    caret_rect_ = caret_rect;
    ibus_input_context_set_cursor_location(context_,
                                           caret_rect_.x(),
                                           caret_rect_.y(),
                                           caret_rect_.width(),
                                           caret_rect_.height());
  }
}

void IBusIMEContext::SetSurrounding(const string16& text,
                                    int cursor_pos) {
  // TODO(penghuang) support surrounding
}

void IBusIMEContext::CreateContext() {
  DCHECK(bus_ != NULL);
  DCHECK(ibus_bus_is_connected(bus_));

  context_ = ibus_bus_create_input_context(bus_, "chrome");

  // connect input context signals
  g_signal_connect(context_,
                   "commit-text",
                   G_CALLBACK(OnCommitTextThunk),
                   this);
  g_signal_connect(context_,
                   "forward-key-event",
                   G_CALLBACK(OnForwardKeyEventThunk),
                   this);
  g_signal_connect(context_,
                   "update-preedit-text",
                   G_CALLBACK(OnUpdatePreeditTextThunk),
                   this);
  g_signal_connect(context_,
                   "show-preedit-text",
                   G_CALLBACK(OnShowPreeditTextThunk),
                   this);
  g_signal_connect(context_,
                   "hide-preedit-text",
                   G_CALLBACK(OnHidePreeditTextThunk),
                   this);
  g_signal_connect(context_,
                   "enabled",
                   G_CALLBACK(OnEnableThunk),
                   this);
  g_signal_connect(context_,
                   "disabled",
                   G_CALLBACK(OnDisableThunk),
                   this);
  g_signal_connect(context_,
                   "destroy",
                   G_CALLBACK(OnDestroyThunk),
                   this);

  guint32 caps = IBUS_CAP_PREEDIT_TEXT |
                 IBUS_CAP_FOCUS |
                 IBUS_CAP_SURROUNDING_TEXT;
  ibus_input_context_set_capabilities(context_, caps);

  if (is_focused_)
    ibus_input_context_focus_in(context_);

  ibus_input_context_set_cursor_location(context_,
                                         caret_rect_.x(),
                                         caret_rect_.y(),
                                         caret_rect_.width(),
                                         caret_rect_.height());
}

void IBusIMEContext::DestroyContext() {
  if (!context_)
    return;

  ibus_proxy_destroy(reinterpret_cast<IBusProxy *>(context_));

  DCHECK(!context_);
}

void IBusIMEContext::OnConnected(IBusBus *bus) {
  DCHECK(bus_ == bus);
  DCHECK(ibus_bus_is_connected(bus_));

  if (context_)
    DestroyContext();
  CreateContext();
}

void IBusIMEContext::OnDisconnected(IBusBus *bus) {
}

void IBusIMEContext::OnCommitText(IBusInputContext *context,
                                  IBusText* text) {
  DCHECK(context_ == context);
  views::IMEContext::CommitText(UTF8ToUTF16(text->text));
}

void IBusIMEContext::OnForwardKeyEvent(IBusInputContext *context,
                                       guint keyval,
                                       guint keycode,
                                       guint state) {
  DCHECK(context_ == context);

  int flags = 0;

  if (state & IBUS_LOCK_MASK)
    flags |= ui::EF_CAPS_LOCK_DOWN;
  if (state & IBUS_CONTROL_MASK)
    flags |= ui::EF_CONTROL_DOWN;
  if (state & IBUS_SHIFT_MASK)
    flags |= ui::EF_SHIFT_DOWN;
  if (state & IBUS_MOD1_MASK)
    flags |= ui::EF_ALT_DOWN;
  if (state & IBUS_BUTTON1_MASK)
    flags |= ui::EF_LEFT_BUTTON_DOWN;
  if (state & IBUS_BUTTON2_MASK)
    flags |= ui::EF_MIDDLE_BUTTON_DOWN;
  if (state & IBUS_BUTTON3_MASK)
    flags |= ui::EF_RIGHT_BUTTON_DOWN;

  ForwardKeyEvent(views::KeyEvent(
      state & IBUS_RELEASE_MASK ?
        ui::ET_KEY_RELEASED : ui::ET_KEY_PRESSED,
      ui::WindowsKeyCodeForGdkKeyCode(keyval),
      flags));
}

void IBusIMEContext::OnUpdatePreeditText(IBusInputContext *context,
                                         IBusText* text,
                                         guint cursor_pos,
                                         gboolean visible) {
  DCHECK(context_ == context);
  DCHECK(IBUS_IS_TEXT(text));

  if (visible) {
    views::CompositionAttributeList attributes;
    IBusAttrList *attrs = text->attrs;
    if (attrs) {
      int i = 0;
      while (1) {
        IBusAttribute *attr = ibus_attr_list_get(attrs, i++);
        if (!attr)
          break;
        if (attr->type == IBUS_ATTR_TYPE_UNDERLINE ||
            attr->type == IBUS_ATTR_TYPE_BACKGROUND) {
          views::CompositionAttribute attribute(attr->start_index,
                                                attr->end_index,
                                                SK_ColorBLACK,
                                                false);
          if (attr->type == IBUS_ATTR_TYPE_BACKGROUND) {
            attribute.thick =  true;
          } else if (attr->type == IBUS_ATTR_TYPE_UNDERLINE) {
            if (attr->value == IBUS_ATTR_UNDERLINE_DOUBLE) {
              attribute.thick = true;
            } else if (attr->value == IBUS_ATTR_UNDERLINE_ERROR) {
              attribute.color = SK_ColorRED;
            }
          }
          attributes.push_back(attribute);
        }
      }
    }

    SetComposition(UTF8ToUTF16(text->text),
                   attributes,
                   cursor_pos);
  } else {
    EndComposition();
  }
}

void IBusIMEContext::OnShowPreeditText(IBusInputContext *context) {
  DCHECK(context_ == context);
}

void IBusIMEContext::OnHidePreeditText(IBusInputContext *context) {
  DCHECK(context_ == context);

  EndComposition();
}

void IBusIMEContext::OnEnable(IBusInputContext *context) {
  DCHECK(context_ == context);
}

void IBusIMEContext::OnDisable(IBusInputContext *context) {
  DCHECK(context_ == context);
}

void IBusIMEContext::OnDestroy(IBusInputContext *context) {
  DCHECK(context_ == context);

  g_object_unref(context_);
  context_ = NULL;

  EndComposition();
}

void IBusIMEContext::ProcessKeyEventDone(IBusInputContext *context,
                                         GAsyncResult* res,
                                         ProcessKeyEventData *data) {
  DCHECK(data->context->context_ == context);

  gboolean processed = FALSE;

  if (!ibus_input_context_process_key_event_finish(context,
                                                   res,
                                                   &processed,
                                                   NULL))
    processed = FALSE;

  if (processed == FALSE)
    data->context->ForwardKeyEvent(data->event);

  delete data;
}

}  // namespace

namespace views {

IMEContext* IMEContext::Create(View* view) {
  return new IBusIMEContext(view);
}

}  // namespace views
