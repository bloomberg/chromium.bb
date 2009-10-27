// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the definition for EventSendingController.
//
// Some notes about drag and drop handling:
// Windows drag and drop goes through a system call to DoDragDrop.  At that
// point, program control is given to Windows which then periodically makes
// callbacks into the webview.  This won't work for layout tests, so instead,
// we queue up all the mouse move and mouse up events.  When the test tries to
// start a drag (by calling EvenSendingController::DoDragDrop), we take the
// events in the queue and replay them.
// The behavior of queuing events and replaying them can be disabled by a
// layout test by setting eventSender.dragMode to false.

// TODO(darin): This is very wrong.  We should not be including WebCore headers
// directly like this!!
#include "config.h"
#include "KeyboardCodes.h"

#undef LOG

#include "webkit/tools/test_shell/event_sending_controller.h"

#include <queue>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/time.h"
#include "webkit/api/public/WebDragData.h"
#include "webkit/api/public/WebDragOperation.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebView.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/test_shell.h"

#if defined(OS_WIN)
#include "webkit/api/public/win/WebInputEventFactory.h"
using WebKit::WebInputEventFactory;
#endif

// TODO(mpcomplete): layout before each event?
// TODO(mpcomplete): do we need modifiers for mouse events?

using base::Time;
using base::TimeTicks;
using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;
using WebKit::WebDragData;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebPoint;
using WebKit::WebString;
using WebKit::WebView;

TestShell* EventSendingController::shell_ = NULL;
gfx::Point EventSendingController::last_mouse_pos_;
WebMouseEvent::Button EventSendingController::pressed_button_ =
    WebMouseEvent::ButtonNone;

WebMouseEvent::Button EventSendingController::last_button_type_ =
    WebMouseEvent::ButtonNone;

namespace {

struct SavedEvent {
  enum SavedEventType {
    Unspecified,
    MouseUp,
    MouseMove,
    LeapForward
  };

  SavedEventType type;
  WebMouseEvent::Button button_type;  // For MouseUp
  gfx::Point pos;                     // For MouseMove.
  int milliseconds;                   // For LeapForward.

  SavedEvent()
    : type(Unspecified),
      button_type(WebMouseEvent::ButtonNone),
      milliseconds(0) {
  }
};

static WebDragData current_drag_data;
static WebDragOperation current_drag_effect;
static WebDragOperationsMask current_drag_effects_allowed;
static bool replaying_saved_events = false;
static std::queue<SavedEvent> mouse_event_queue;

// Time and place of the last mouse up event.
static double last_click_time_sec = 0;
static gfx::Point last_click_pos;
static int click_count = 0;

// maximum distance (in space and time) for a mouse click
// to register as a double or triple click
static const double kMultiClickTimeSec = 1;
static const int kMultiClickRadiusPixels = 5;

inline bool outside_multiclick_radius(const gfx::Point &a, const gfx::Point &b) {
  return ((a.x() - b.x()) * (a.x() - b.x()) + (a.y() - b.y()) * (a.y() - b.y())) >
    kMultiClickRadiusPixels * kMultiClickRadiusPixels;
}

// Used to offset the time the event hander things an event happened.  This is
// done so tests can run without a delay, but bypass checks that are time
// dependent (e.g., dragging has a timeout vs selection).
static uint32 time_offset_ms = 0;

double GetCurrentEventTimeSec() {
  return (TimeTicks::Now().ToInternalValue()
            / Time::kMicrosecondsPerMillisecond +
          time_offset_ms) / 1000.0;
}

void AdvanceEventTime(int32 delta_ms) {
  time_offset_ms += delta_ms;
}

void InitMouseEvent(WebInputEvent::Type t, WebMouseEvent::Button b,
                    const gfx::Point& pos, WebMouseEvent* e) {
  e->type = t;
  e->button = b;
  e->modifiers = 0;
  e->x = pos.x();
  e->y = pos.y();
  e->globalX = pos.x();
  e->globalY = pos.y();
  e->timeStampSeconds = GetCurrentEventTimeSec();
  e->clickCount = click_count;
}

void ApplyKeyModifier(const std::wstring& arg, WebKeyboardEvent* event) {
  const wchar_t* arg_string = arg.c_str();
  if (!wcscmp(arg_string, L"ctrlKey")) {
    event->modifiers |= WebInputEvent::ControlKey;
  } else if (!wcscmp(arg_string, L"shiftKey")) {
    event->modifiers |= WebInputEvent::ShiftKey;
  } else if (!wcscmp(arg_string, L"altKey")) {
    event->modifiers |= WebInputEvent::AltKey;
    event->isSystemKey = true;
  } else if (!wcscmp(arg_string, L"metaKey")) {
    event->modifiers |= WebInputEvent::MetaKey;
  }
}

void ApplyKeyModifiers(const CppVariant* arg, WebKeyboardEvent* event) {
  if (arg->isObject()) {
    std::vector<std::wstring> args = arg->ToStringVector();
    for (std::vector<std::wstring>::const_iterator i = args.begin();
         i != args.end(); ++i) {
      ApplyKeyModifier(*i, event);
    }
  } else if (arg->isString()) {
    ApplyKeyModifier(UTF8ToWide(arg->ToString()), event);
  }
}

}  // anonymous namespace

EventSendingController::EventSendingController(TestShell* shell)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  // Set static shell_ variable since we can't do it in an initializer list.
  // We also need to be careful not to assign shell_ to new windows which are
  // temporary.
  if (NULL == shell_)
    shell_ = shell;

  // Initialize the map that associates methods of this class with the names
  // they will use when called by JavaScript.  The actual binding of those
  // names to their methods will be done by calling BindToJavaScript() (defined
  // by CppBoundClass, the parent to EventSendingController).
  BindMethod("mouseDown", &EventSendingController::mouseDown);
  BindMethod("mouseUp", &EventSendingController::mouseUp);
  BindMethod("contextClick", &EventSendingController::contextClick);
  BindMethod("mouseMoveTo", &EventSendingController::mouseMoveTo);
  BindMethod("leapForward", &EventSendingController::leapForward);
  BindMethod("keyDown", &EventSendingController::keyDown);
  BindMethod("dispatchMessage", &EventSendingController::dispatchMessage);
  BindMethod("enableDOMUIEventLogging",
             &EventSendingController::enableDOMUIEventLogging);
  BindMethod("fireKeyboardEventsToElement",
             &EventSendingController::fireKeyboardEventsToElement);
  BindMethod("clearKillRing", &EventSendingController::clearKillRing);
  BindMethod("textZoomIn", &EventSendingController::textZoomIn);
  BindMethod("textZoomOut", &EventSendingController::textZoomOut);
  BindMethod("zoomPageIn", &EventSendingController::zoomPageIn);
  BindMethod("zoomPageOut", &EventSendingController::zoomPageOut);
  BindMethod("scheduleAsynchronousClick",
             &EventSendingController::scheduleAsynchronousClick);
  BindMethod("beginDragWithFiles",
             &EventSendingController::beginDragWithFiles);

  // When set to true (the default value), we batch mouse move and mouse up
  // events so we can simulate drag & drop.
  BindProperty("dragMode", &dragMode);
#if defined(OS_WIN)
  BindProperty("WM_KEYDOWN", &wmKeyDown);
  BindProperty("WM_KEYUP", &wmKeyUp);
  BindProperty("WM_CHAR", &wmChar);
  BindProperty("WM_DEADCHAR", &wmDeadChar);
  BindProperty("WM_SYSKEYDOWN", &wmSysKeyDown);
  BindProperty("WM_SYSKEYUP", &wmSysKeyUp);
  BindProperty("WM_SYSCHAR", &wmSysChar);
  BindProperty("WM_SYSDEADCHAR", &wmSysDeadChar);
#endif
}

void EventSendingController::Reset() {
  // The test should have finished a drag and the mouse button state.
  DCHECK(current_drag_data.isNull());
  current_drag_data.reset();
  current_drag_effect = WebKit::WebDragOperationNone;
  current_drag_effects_allowed = WebKit::WebDragOperationNone;
  pressed_button_ = WebMouseEvent::ButtonNone;
  dragMode.Set(true);
#if defined(OS_WIN)
  wmKeyDown.Set(WM_KEYDOWN);
  wmKeyUp.Set(WM_KEYUP);
  wmChar.Set(WM_CHAR);
  wmDeadChar.Set(WM_DEADCHAR);
  wmSysKeyDown.Set(WM_SYSKEYDOWN);
  wmSysKeyUp.Set(WM_SYSKEYUP);
  wmSysChar.Set(WM_SYSCHAR);
  wmSysDeadChar.Set(WM_SYSDEADCHAR);
#endif
  last_mouse_pos_.SetPoint(0, 0);
  last_click_time_sec = 0;
  last_click_pos.SetPoint(0, 0);
  click_count = 0;
  last_button_type_ = WebMouseEvent::ButtonNone;
  time_offset_ms = 0;
}

// static
WebView* EventSendingController::webview() {
  return shell_->webView();
}

// static
void EventSendingController::DoDragDrop(const WebKit::WebPoint &event_pos,
                                        const WebDragData& drag_data,
                                        WebDragOperationsMask mask) {
  WebMouseEvent event;
  InitMouseEvent(WebInputEvent::MouseDown, pressed_button_, last_mouse_pos_, &event);
  WebPoint client_point(event.x, event.y);
  WebPoint screen_point(event.globalX, event.globalY);
  current_drag_data = drag_data;
  current_drag_effects_allowed = mask;
  current_drag_effect = webview()->dragTargetDragEnter(
      drag_data, 0, client_point, screen_point, current_drag_effects_allowed);

  // Finish processing events.
  ReplaySavedEvents();
}

WebMouseEvent::Button EventSendingController::GetButtonTypeFromButtonNumber(
    int button_code) {
  if (button_code == 0)
    return WebMouseEvent::ButtonLeft;
  else if (button_code == 2)
    return WebMouseEvent::ButtonRight;

  return WebMouseEvent::ButtonMiddle;
}

// static
int EventSendingController::GetButtonNumberFromSingleArg(
    const CppArgumentList& args) {
  int button_code = 0;

  if (args.size() > 0 && args[0].isNumber()) {
    button_code = args[0].ToInt32();
  }

  return button_code;
}

void EventSendingController::UpdateClickCountForButton(
    WebMouseEvent::Button button_type) {
  if ((GetCurrentEventTimeSec() - last_click_time_sec < kMultiClickTimeSec) &&
      (!outside_multiclick_radius(last_mouse_pos_, last_click_pos)) &&
      (button_type == last_button_type_)) {
    ++click_count;
  } else {
    click_count = 1;
    last_button_type_ = button_type;
  }
}

//
// Implemented javascript methods.
//

void EventSendingController::mouseDown(
    const CppArgumentList& args, CppVariant* result) {
  if (result)  // Could be NULL if invoked asynchronously.
    result->SetNull();

  webview()->layout();

  int button_number = GetButtonNumberFromSingleArg(args);
  DCHECK(button_number != -1);

  WebMouseEvent::Button button_type = GetButtonTypeFromButtonNumber(
      button_number);

  UpdateClickCountForButton(button_type);

  WebMouseEvent event;
  pressed_button_ = button_type;
  InitMouseEvent(WebInputEvent::MouseDown, button_type,
                 last_mouse_pos_, &event);
  webview()->handleInputEvent(event);
}

void EventSendingController::mouseUp(
    const CppArgumentList& args, CppVariant* result) {
  if (result)  // Could be NULL if invoked asynchronously.
    result->SetNull();

  webview()->layout();

  int button_number = GetButtonNumberFromSingleArg(args);
  DCHECK(button_number != -1);

  WebMouseEvent::Button button_type = GetButtonTypeFromButtonNumber(
      button_number);

  if (drag_mode() && !replaying_saved_events) {
    SavedEvent saved_event;
    saved_event.type = SavedEvent::MouseUp;
    saved_event.button_type = button_type;
    mouse_event_queue.push(saved_event);
    ReplaySavedEvents();
  } else {
    WebMouseEvent event;
    InitMouseEvent(WebInputEvent::MouseUp, button_type,
                   last_mouse_pos_, &event);
    DoMouseUp(event);
  }
}

// static
void EventSendingController::DoMouseUp(const WebMouseEvent& e) {
  webview()->handleInputEvent(e);

  pressed_button_ = WebMouseEvent::ButtonNone;
  last_click_time_sec = e.timeStampSeconds;
  last_click_pos = last_mouse_pos_;

  // If we're in a drag operation, complete it.
  if (!current_drag_data.isNull()) {
    WebPoint client_point(e.x, e.y);
    WebPoint screen_point(e.globalX, e.globalY);

    webview()->dragSourceMovedTo(client_point, screen_point);
    current_drag_effect = webview()->dragTargetDragOver(
        client_point, screen_point, current_drag_effects_allowed);
    if (current_drag_effect) {
      webview()->dragTargetDrop(client_point, screen_point);
    } else {
      webview()->dragTargetDragLeave();
    }
    webview()->dragSourceEndedAt(
        client_point, screen_point, current_drag_effect);
    webview()->dragSourceSystemDragEnded();

    current_drag_data.reset();
  }
}

void EventSendingController::mouseMoveTo(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  if (args.size() >= 2 && args[0].isNumber() && args[1].isNumber()) {
    webview()->layout();

    gfx::Point mouse_pos;
    mouse_pos.SetPoint(args[0].ToInt32(), args[1].ToInt32());

    if (drag_mode() && pressed_button_ == WebMouseEvent::ButtonLeft &&
        !replaying_saved_events) {
      SavedEvent saved_event;
      saved_event.type = SavedEvent::MouseMove;
      saved_event.pos = mouse_pos;
      mouse_event_queue.push(saved_event);
    } else {
      WebMouseEvent event;
      InitMouseEvent(WebInputEvent::MouseMove, pressed_button_,
                     mouse_pos, &event);
      DoMouseMove(event);
    }
  }
}

// static
void EventSendingController::DoMouseMove(const WebMouseEvent& e) {
  last_mouse_pos_.SetPoint(e.x, e.y);

  webview()->handleInputEvent(e);

  if (pressed_button_ != WebMouseEvent::ButtonNone &&
      !current_drag_data.isNull()) {
    WebPoint client_point(e.x, e.y);
    WebPoint screen_point(e.globalX, e.globalY);

    webview()->dragSourceMovedTo(client_point, screen_point);
    current_drag_effect = webview()->dragTargetDragOver(
        client_point, screen_point, current_drag_effects_allowed);
  }
}

void EventSendingController::keyDown(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  bool generate_char = false;

  if (args.size() >= 1 && args[0].isString()) {
    // TODO(mpcomplete): I'm not exactly sure how we should convert the string
    // to a key event.  This seems to work in the cases I tested.
    // TODO(mpcomplete): Should we also generate a KEY_UP?
    std::wstring code_str = UTF8ToWide(args[0].ToString());

    // Convert \n -> VK_RETURN.  Some layout tests use \n to mean "Enter", when
    // Windows uses \r for "Enter".
    int code = 0;
    bool needs_shift_key_modifier = false;
    if (L"\n" == code_str) {
      generate_char = true;
      code = WebCore::VKEY_RETURN;
    } else if (L"rightArrow" == code_str) {
      code = WebCore::VKEY_RIGHT;
    } else if (L"downArrow" == code_str) {
      code = WebCore::VKEY_DOWN;
    } else if (L"leftArrow" == code_str) {
      code = WebCore::VKEY_LEFT;
    } else if (L"upArrow" == code_str) {
      code = WebCore::VKEY_UP;
    } else if (L"delete" == code_str) {
      code = WebCore::VKEY_BACK;
    } else if (L"pageUp" == code_str) {
      code = WebCore::VKEY_PRIOR;
    } else if (L"pageDown" == code_str) {
      code = WebCore::VKEY_NEXT;
    } else if (L"home" == code_str) {
      code = WebCore::VKEY_HOME;
    } else if (L"end" == code_str) {
      code = WebCore::VKEY_END;
    } else {
      // Compare the input string with the function-key names defined by the
      // DOM spec (i.e. "F1",...,"F24"). If the input string is a function-key
      // name, set its key code.
      for (int i = 1; i <= 24; ++i) {
        std::wstring function_key_name;
        function_key_name += L"F";
        function_key_name += IntToWString(i);
        if (function_key_name == code_str) {
          code = WebCore::VKEY_F1 + (i - 1);
          break;
        }
      }
      if (!code) {
        DCHECK(code_str.length() == 1);
        code = code_str[0];
        needs_shift_key_modifier = NeedsShiftModifier(code);
        generate_char = true;
      }
    }

    // For one generated keyboard event, we need to generate a keyDown/keyUp
    // pair; refer to EventSender.cpp in WebKit/WebKitTools/DumpRenderTree/win.
    // On Windows, we might also need to generate a char event to mimic the
    // Windows event flow; on other platforms we create a merged event and test
    // the event flow that that platform provides.
    WebKeyboardEvent event_down, event_char, event_up;
    event_down.type = WebInputEvent::RawKeyDown;
    event_down.modifiers = 0;
    event_down.windowsKeyCode = code;
    if (generate_char) {
      event_down.text[0] = code;
      event_down.unmodifiedText[0] = code;
    }
    event_down.setKeyIdentifierFromWindowsKeyCode();

    if (args.size() >= 2 && (args[1].isObject() || args[1].isString()))
      ApplyKeyModifiers(&(args[1]), &event_down);

    if (needs_shift_key_modifier)
      event_down.modifiers |= WebInputEvent::ShiftKey;

    event_char = event_up = event_down;
    event_up.type = WebInputEvent::KeyUp;
    // EventSendingController.m forces a layout here, with at least one
    // test (fast\forms\focus-control-to-page.html) relying on this.
    webview()->layout();

#if defined(OS_MACOSX)
    // On Mac OS, some layout tests (such as "delete-by-word-001.html") sends
    // an option-key (or alt-key) event to test it is mapped to an appropriate
    // editor command (such as "DeleteWordBackward").
    // On the other hand, EditorClientImpl::handleEditingKeyboardEvent()
    // ignores the key event whose isSystemKey value is true and cannot map
    // an editor command to an option-key event.
    // As a workaround for this problem, we set isSystemKey of RawKeyDown
    // events to false.
    event_down.isSystemKey = false;
#endif
    webview()->handleInputEvent(event_down);

    if (generate_char) {
      event_char.type = WebInputEvent::Char;
      event_char.keyIdentifier[0] = '\0';
      webview()->handleInputEvent(event_char);
    }

    webview()->handleInputEvent(event_up);
  }
}

void EventSendingController::dispatchMessage(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

#if defined(OS_WIN)
  if (args.size() == 3) {
    // Grab the message id to see if we need to dispatch it.
    int msg = args[0].ToInt32();

    // WebKit's version of this function stuffs a MSG struct and uses
    // TranslateMessage and DispatchMessage. We use a WebKeyboardEvent, which
    // doesn't need to receive the DeadChar and SysDeadChar messages.
    if (msg == WM_DEADCHAR || msg == WM_SYSDEADCHAR)
      return;

    webview()->layout();

    unsigned long lparam = static_cast<unsigned long>(args[2].ToDouble());
    webview()->handleInputEvent(WebInputEventFactory::keyboardEvent(
        NULL, msg, args[1].ToInt32(), lparam));
  } else {
    NOTREACHED() << L"Wrong number of arguments";
  }
#endif
}

bool EventSendingController::NeedsShiftModifier(int key_code) {
  // If code is an uppercase letter, assign a SHIFT key to
  // event_down.modifier, this logic comes from
  // WebKit/WebKitTools/DumpRenderTree/Win/EventSender.cpp
  if ((key_code & 0xFF) >= 'A' && (key_code & 0xFF) <= 'Z')
    return true;
  return false;
}

void EventSendingController::leapForward(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  if (args.size() <1 || !args[0].isNumber())
    return;

  int milliseconds = args[0].ToInt32();
  if (drag_mode() && pressed_button_ == WebMouseEvent::ButtonLeft &&
      !replaying_saved_events) {
    SavedEvent saved_event;
    saved_event.type = SavedEvent::LeapForward;
    saved_event.milliseconds = milliseconds;
    mouse_event_queue.push(saved_event);
  } else {
    DoLeapForward(milliseconds);
  }
}

// static
void EventSendingController::DoLeapForward(int milliseconds) {
  AdvanceEventTime(milliseconds);
}

// Apple's port of WebKit zooms by a factor of 1.2 (see
// WebKit/WebView/WebView.mm)
void EventSendingController::textZoomIn(
    const CppArgumentList& args, CppVariant* result) {
  webview()->zoomIn(true);
  result->SetNull();
}

void EventSendingController::textZoomOut(
    const CppArgumentList& args, CppVariant* result) {
  webview()->zoomOut(true);
  result->SetNull();
}

void EventSendingController::zoomPageIn(
    const CppArgumentList& args, CppVariant* result) {
  webview()->zoomIn(false);
  result->SetNull();
}

void EventSendingController::zoomPageOut(
    const CppArgumentList& args, CppVariant* result) {
  webview()->zoomOut(false);
  result->SetNull();
}

void EventSendingController::ReplaySavedEvents() {
  replaying_saved_events = true;
  while (!mouse_event_queue.empty()) {
    SavedEvent e = mouse_event_queue.front();
    mouse_event_queue.pop();

    switch (e.type) {
      case SavedEvent::MouseMove: {
        WebMouseEvent event;
        InitMouseEvent(WebInputEvent::MouseMove, pressed_button_,
                       e.pos, &event);
        DoMouseMove(event);
        break;
      }
      case SavedEvent::LeapForward:
        DoLeapForward(e.milliseconds);
        break;
      case SavedEvent::MouseUp: {
        WebMouseEvent event;
        InitMouseEvent(WebInputEvent::MouseUp, e.button_type,
                       last_mouse_pos_, &event);
        DoMouseUp(event);
        break;
      }
      default:
        NOTREACHED();
    }
  }

  replaying_saved_events = false;
}

void EventSendingController::contextClick(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  webview()->layout();

  UpdateClickCountForButton(WebMouseEvent::ButtonRight);

  // Generate right mouse down and up.

  WebMouseEvent event;
  pressed_button_ = WebMouseEvent::ButtonRight;
  InitMouseEvent(WebInputEvent::MouseDown, WebMouseEvent::ButtonRight,
                 last_mouse_pos_, &event);
  webview()->handleInputEvent(event);

  InitMouseEvent(WebInputEvent::MouseUp, WebMouseEvent::ButtonRight,
                 last_mouse_pos_, &event);
  webview()->handleInputEvent(event);

  pressed_button_ = WebMouseEvent::ButtonNone;
}

void EventSendingController::scheduleAsynchronousClick(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();

  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&EventSendingController::mouseDown,
                                        args, static_cast<CppVariant*>(NULL)));
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&EventSendingController::mouseUp,
                                        args, static_cast<CppVariant*>(NULL)));
}

void EventSendingController::beginDragWithFiles(
    const CppArgumentList& args, CppVariant* result) {
  current_drag_data.initialize();
  std::vector<std::wstring> files = args[0].ToStringVector();
  for (size_t i = 0; i < files.size(); ++i) {
    FilePath file_path = FilePath::FromWStringHack(files[i]);
    file_util::AbsolutePath(&file_path);
    current_drag_data.appendToFileNames(
        webkit_glue::FilePathStringToWebString(file_path.value()));
  }
  current_drag_effects_allowed = WebKit::WebDragOperationCopy;

  // Provide a drag source.
  WebPoint client_point(last_mouse_pos_.x(), last_mouse_pos_.y());
  WebPoint screen_point(last_mouse_pos_.x(), last_mouse_pos_.y());
  webview()->dragTargetDragEnter(
      current_drag_data, 0, client_point, screen_point,
      current_drag_effects_allowed);

  // dragMode saves events and then replays them later. We don't need/want that.
  dragMode.Set(false);

  // Make the rest of eventSender think a drag is in progress.
  pressed_button_ = WebMouseEvent::ButtonLeft;

  result->SetNull();
}

//
// Unimplemented stubs
//

void EventSendingController::enableDOMUIEventLogging(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void EventSendingController::fireKeyboardEventsToElement(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void EventSendingController::clearKillRing(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}
