// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/logging.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/models/combobox_model.h"
#include "ui/gfx/rect.h"
#include "views/background.h"
#include "views/border.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/combobox/native_combobox_wrapper.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/controls/native/native_view_host.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/scroll_view.h"
#include "views/controls/tabbed_pane/native_tabbed_pane_wrapper.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/focus/accelerator_handler.h"
#include "views/widget/root_view.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"
#include "views/window/window_delegate.h"

#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#include "views/window/window_win.h"
#elif defined(OS_LINUX)
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#include "views/window/window_gtk.h"
#endif

using ui::ComboboxModel;  // TODO(beng): remove

namespace {
const int kWindowWidth = 600;
const int kWindowHeight = 500;

int count = 1;

const int kTopCheckBoxID = count++;  // 1
const int kLeftContainerID = count++;
const int kAppleLabelID = count++;
const int kAppleTextfieldID = count++;
const int kOrangeLabelID = count++;  // 5
const int kOrangeTextfieldID = count++;
const int kBananaLabelID = count++;
const int kBananaTextfieldID = count++;
const int kKiwiLabelID = count++;
const int kKiwiTextfieldID = count++;  // 10
const int kFruitButtonID = count++;
const int kFruitCheckBoxID = count++;
const int kComboboxID = count++;

const int kRightContainerID = count++;
const int kAsparagusButtonID = count++;  // 15
const int kBroccoliButtonID = count++;
const int kCauliflowerButtonID = count++;

const int kInnerContainerID = count++;
const int kScrollViewID = count++;
const int kRosettaLinkID = count++;  // 20
const int kStupeurEtTremblementLinkID = count++;
const int kDinerGameLinkID = count++;
const int kRidiculeLinkID = count++;
const int kClosetLinkID = count++;
const int kVisitingLinkID = count++;  // 25
const int kAmelieLinkID = count++;
const int kJoyeuxNoelLinkID = count++;
const int kCampingLinkID = count++;
const int kBriceDeNiceLinkID = count++;
const int kTaxiLinkID = count++;  // 30
const int kAsterixLinkID = count++;

const int kOKButtonID = count++;
const int kCancelButtonID = count++;
const int kHelpButtonID = count++;


const int kStyleContainerID = count++;  // 35
const int kBoldCheckBoxID = count++;
const int kItalicCheckBoxID = count++;
const int kUnderlinedCheckBoxID = count++;
const int kStyleHelpLinkID = count++;
const int kStyleTextEditID = count++;  // 40

const int kSearchContainerID = count++;
const int kSearchTextfieldID = count++;
const int kSearchButtonID = count++;
const int kHelpLinkID = count++;

const int kThumbnailContainerID = count++;  // 45
const int kThumbnailStarID = count++;
const int kThumbnailSuperStarID = count++;
}

namespace views {

class FocusManagerTest : public testing::Test, public WindowDelegate {
 public:
  FocusManagerTest()
      : window_(NULL),
        content_view_(NULL),
        focus_change_listener_(NULL) {
#if defined(OS_WIN)
    OleInitialize(NULL);
#endif
  }

  ~FocusManagerTest() {
#if defined(OS_WIN)
    OleUninitialize();
#endif
  }

  virtual void SetUp() {
    window_ = Window::CreateChromeWindow(NULL, bounds(), this);
    InitContentView();
    window_->Show();
  }

  virtual void TearDown() {
    if (focus_change_listener_)
      GetFocusManager()->RemoveFocusChangeListener(focus_change_listener_);
    window_->Close();

    // Flush the message loop to make Purify happy.
    message_loop()->RunAllPending();
  }

  FocusManager* GetFocusManager() {
#if defined(OS_WIN)
    return static_cast<WindowWin*>(window_)->GetFocusManager();
#elif defined(OS_LINUX)
    return static_cast<WindowGtk*>(window_)->GetFocusManager();
#else
    NOTIMPLEMENTED();
#endif
  }

  void FocusNativeView(gfx::NativeView native_view) {
#if defined(OS_WIN)
  ::SendMessage(native_view, WM_SETFOCUS, NULL, NULL);
#else
    gint return_val;
    GdkEventFocus event;
    event.type = GDK_FOCUS_CHANGE;
    event.window =
        gtk_widget_get_root_window(GTK_WIDGET(window_->GetNativeWindow()));
    event.send_event = TRUE;
    event.in = TRUE;
    gtk_signal_emit_by_name(GTK_OBJECT(native_view), "focus-in-event",
                            &event, &return_val);
#endif
  }

  // WindowDelegate Implementation.
  virtual View* GetContentsView() {
    if (!content_view_)
      content_view_ = new View();
    return content_view_;
  }

  virtual void InitContentView() {
  }

 protected:
  virtual gfx::Rect bounds() {
    return gfx::Rect(0, 0, 500, 500);
  }

  // Mocks activating/deactivating the window.
  void SimulateActivateWindow() {
#if defined(OS_WIN)
    ::SendMessage(window_->GetNativeWindow(), WM_ACTIVATE, WA_ACTIVE, NULL);
#else
    gboolean result;
    g_signal_emit_by_name(G_OBJECT(window_->GetNativeWindow()),
                          "focus_in_event", 0, &result);
#endif
  }
  void SimulateDeactivateWindow() {
#if defined(OS_WIN)
    ::SendMessage(window_->GetNativeWindow(), WM_ACTIVATE, WA_INACTIVE, NULL);
#else
    gboolean result;
    g_signal_emit_by_name(G_OBJECT(window_->GetNativeWindow()),
                          "focus_out_event", 0, & result);
#endif
  }

  MessageLoopForUI* message_loop() { return &message_loop_; }

  Window* window_;
  View* content_view_;

  void AddFocusChangeListener(FocusChangeListener* listener) {
    ASSERT_FALSE(focus_change_listener_);
    focus_change_listener_ = listener;
    GetFocusManager()->AddFocusChangeListener(listener);
  }

#if defined(OS_WIN)
  void PostKeyDown(ui::KeyboardCode key_code) {
    ::PostMessage(window_->GetNativeWindow(), WM_KEYDOWN, key_code, 0);
  }

  void PostKeyUp(ui::KeyboardCode key_code) {
    ::PostMessage(window_->GetNativeWindow(), WM_KEYUP, key_code, 0);
  }
#elif defined(OS_LINUX)
  void PostKeyDown(ui::KeyboardCode key_code) {
    PostKeyEvent(key_code, true);
  }

  void PostKeyUp(ui::KeyboardCode key_code) {
    PostKeyEvent(key_code, false);
  }

  void PostKeyEvent(ui::KeyboardCode key_code, bool pressed) {
    int keyval = GdkKeyCodeForWindowsKeyCode(key_code, false);
    GdkKeymapKey* keys;
    gint n_keys;
    gdk_keymap_get_entries_for_keyval(
        gdk_keymap_get_default(),
        keyval,
        &keys,
        &n_keys);
    GdkEvent* event = gdk_event_new(pressed ? GDK_KEY_PRESS : GDK_KEY_RELEASE);
    GdkEventKey* key_event = reinterpret_cast<GdkEventKey*>(event);
    int modifier = 0;
    if (pressed)
      key_event->state = modifier | GDK_KEY_PRESS_MASK;
    else
      key_event->state = modifier | GDK_KEY_RELEASE_MASK;

    key_event->window = GTK_WIDGET(window_->GetNativeWindow())->window;
    DCHECK(key_event->window != NULL);
    g_object_ref(key_event->window);
    key_event->send_event = true;
    key_event->time = GDK_CURRENT_TIME;
    key_event->keyval = keyval;
    key_event->hardware_keycode = keys[0].keycode;
    key_event->group = keys[0].group;

    g_free(keys);

    gdk_event_put(event);
    gdk_event_free(event);
  }
#endif

 private:
  FocusChangeListener* focus_change_listener_;
  MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(FocusManagerTest);
};

// BorderView is a view containing a native window with its own view hierarchy.
// It is interesting to test focus traversal from a view hierarchy to an inner
// view hierarchy.
class BorderView : public NativeViewHost {
 public:
  explicit BorderView(View* child) : child_(child), widget_(NULL) {
    DCHECK(child);
    SetFocusable(false);
  }

  virtual ~BorderView() {}

  virtual RootView* GetContentsRootView() {
    return widget_->GetRootView();
  }

  virtual FocusTraversable* GetFocusTraversable() {
    return widget_->GetRootView();
  }

  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child) {
    NativeViewHost::ViewHierarchyChanged(is_add, parent, child);

    if (child == this && is_add) {
      if (!widget_) {
#if defined(OS_WIN)
        WidgetWin* widget_win = new WidgetWin();
        widget_win->Init(parent->GetRootView()->GetWidget()->GetNativeView(),
                         gfx::Rect(0, 0, 0, 0));
        widget_win->SetFocusTraversableParentView(this);
        widget_ = widget_win;
#else
        WidgetGtk* widget_gtk = new WidgetGtk(WidgetGtk::TYPE_CHILD);
        widget_gtk->Init(native_view(), gfx::Rect(0, 0, 0, 0));
        widget_gtk->SetFocusTraversableParentView(this);
        widget_ = widget_gtk;
#endif
        widget_->SetContentsView(child_);
      }

      // We have been added to a view hierarchy, attach the native view.
      Attach(widget_->GetNativeView());
      // Also update the FocusTraversable parent so the focus traversal works.
      widget_->GetRootView()->SetFocusTraversableParent(
          GetWidget()->GetFocusTraversable());
    }
  }

 private:
  View* child_;
  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(BorderView);
};

class DummyComboboxModel : public ComboboxModel {
 public:
  virtual int GetItemCount() { return 10; }

  virtual string16 GetItemAt(int index) {
    return ASCIIToUTF16("Item ") + base::IntToString16(index);
  }
};

// A View that can act as a pane.
class PaneView : public View, public FocusTraversable {
 public:
  PaneView() : focus_search_(NULL) {}

  // If this method is called, this view will use GetPaneFocusTraversable to
  // have this provided FocusSearch used instead of the default one, allowing
  // you to trap focus within the pane.
  void EnablePaneFocus(FocusSearch* focus_search) {
    focus_search_ = focus_search;
  }

  // Overridden from views::View:
  virtual FocusTraversable* GetPaneFocusTraversable() {
    if (focus_search_)
      return this;
    else
      return NULL;
  }

  // Overridden from views::FocusTraversable:
  virtual views::FocusSearch* GetFocusSearch() {
    return focus_search_;
  }
  virtual FocusTraversable* GetFocusTraversableParent() {
    return NULL;
  }
  virtual View* GetFocusTraversableParentView() {
    return NULL;
  }

 private:
  FocusSearch* focus_search_;
};

class FocusTraversalTest : public FocusManagerTest {
 public:
  ~FocusTraversalTest();

  virtual void InitContentView();

 protected:
  FocusTraversalTest();

  virtual gfx::Rect bounds() {
    return gfx::Rect(0, 0, 600, 460);
  }

  View* FindViewByID(int id) {
    View* view = GetContentsView()->GetViewByID(id);
    if (view)
      return view;
    view = style_tab_->GetSelectedTab()->GetViewByID(id);
    if (view)
      return view;
    view = search_border_view_->GetContentsRootView()->GetViewByID(id);
    if (view)
      return view;
    return NULL;
  }

 protected:
  TabbedPane* style_tab_;
  BorderView* search_border_view_;
  DummyComboboxModel combobox_model_;
  PaneView* left_container_;
  PaneView* right_container_;

  DISALLOW_COPY_AND_ASSIGN(FocusTraversalTest);
};

////////////////////////////////////////////////////////////////////////////////
// FocusTraversalTest
////////////////////////////////////////////////////////////////////////////////

FocusTraversalTest::FocusTraversalTest()
  : style_tab_(NULL),
    search_border_view_(NULL) {
}

FocusTraversalTest::~FocusTraversalTest() {
}

void FocusTraversalTest::InitContentView() {
  // Create a complicated view hierarchy with lots of control types for
  // use by all of the focus traversal tests.
  //
  // Class name, ID, and asterisk next to focusable views:
  //
  // View
  //   Checkbox            * kTopCheckBoxID
  //   PaneView              kLeftContainerID
  //     Label               kAppleLabelID
  //     Textfield         * kAppleTextfieldID
  //     Label               kOrangeLabelID
  //     Textfield         * kOrangeTextfieldID
  //     Label               kBananaLabelID
  //     Textfield         * kBananaTextfieldID
  //     Label               kKiwiLabelID
  //     Textfield         * kKiwiTextfieldID
  //     NativeButton      * kFruitButtonID
  //     Checkbox          * kFruitCheckBoxID
  //     Combobox          * kComboboxID
  //   PaneView              kRightContainerID
  //     RadioButton       * kAsparagusButtonID
  //     RadioButton       * kBroccoliButtonID
  //     RadioButton       * kCauliflowerButtonID
  //     View                kInnerContainerID
  //       ScrollView        kScrollViewID
  //         View
  //           Link        * kRosettaLinkID
  //           Link        * kStupeurEtTremblementLinkID
  //           Link        * kDinerGameLinkID
  //           Link        * kRidiculeLinkID
  //           Link        * kClosetLinkID
  //           Link        * kVisitingLinkID
  //           Link        * kAmelieLinkID
  //           Link        * kJoyeuxNoelLinkID
  //           Link        * kCampingLinkID
  //           Link        * kBriceDeNiceLinkID
  //           Link        * kTaxiLinkID
  //           Link        * kAsterixLinkID
  //   NativeButton        * kOKButtonID
  //   NativeButton        * kCancelButtonID
  //   NativeButton        * kHelpButtonID
  //   TabbedPane          * kStyleContainerID
  //     View
  //       Checkbox        * kBoldCheckBoxID
  //       Checkbox        * kItalicCheckBoxID
  //       Checkbox        * kUnderlinedCheckBoxID
  //       Link            * kStyleHelpLinkID
  //       Textfield       * kStyleTextEditID
  //     Other
  //   BorderView            kSearchContainerID
  //     View
  //       Textfield       * kSearchTextfieldID
  //       NativeButton    * kSearchButtonID
  //       Link            * kHelpLinkID
  //   View                * kThumbnailContainerID
  //     NativeButton      * kThumbnailStarID
  //     NativeButton      * kThumbnailSuperStarID

  content_view_->set_background(
      Background::CreateSolidBackground(SK_ColorWHITE));

  Checkbox* cb = new Checkbox(L"This is a checkbox");
  content_view_->AddChildView(cb);
  // In this fast paced world, who really has time for non hard-coded layout?
  cb->SetBounds(10, 10, 200, 20);
  cb->SetID(kTopCheckBoxID);

  left_container_ = new PaneView();
  left_container_->set_border(Border::CreateSolidBorder(1, SK_ColorBLACK));
  left_container_->set_background(
      Background::CreateSolidBackground(240, 240, 240));
  left_container_->SetID(kLeftContainerID);
  content_view_->AddChildView(left_container_);
  left_container_->SetBounds(10, 35, 250, 200);

  int label_x = 5;
  int label_width = 50;
  int label_height = 15;
  int text_field_width = 150;
  int y = 10;
  int gap_between_labels = 10;

  Label* label = new Label(L"Apple:");
  label->SetID(kAppleLabelID);
  left_container_->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  Textfield* text_field = new Textfield();
  text_field->SetID(kAppleTextfieldID);
  left_container_->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  label = new Label(L"Orange:");
  label->SetID(kOrangeLabelID);
  left_container_->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  text_field = new Textfield();
  text_field->SetID(kOrangeTextfieldID);
  left_container_->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  label = new Label(L"Banana:");
  label->SetID(kBananaLabelID);
  left_container_->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  text_field = new Textfield();
  text_field->SetID(kBananaTextfieldID);
  left_container_->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  label = new Label(L"Kiwi:");
  label->SetID(kKiwiLabelID);
  left_container_->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  text_field = new Textfield();
  text_field->SetID(kKiwiTextfieldID);
  left_container_->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  NativeButton* button = new NativeButton(NULL, L"Click me");
  button->SetBounds(label_x, y + 10, 80, 30);
  button->SetID(kFruitButtonID);
  left_container_->AddChildView(button);
  y += 40;

  cb =  new Checkbox(L"This is another check box");
  cb->SetBounds(label_x + label_width + 5, y, 180, 20);
  cb->SetID(kFruitCheckBoxID);
  left_container_->AddChildView(cb);
  y += 20;

  Combobox* combobox =  new Combobox(&combobox_model_);
  combobox->SetBounds(label_x + label_width + 5, y, 150, 30);
  combobox->SetID(kComboboxID);
  left_container_->AddChildView(combobox);

  right_container_ = new PaneView();
  right_container_->set_border(Border::CreateSolidBorder(1, SK_ColorBLACK));
  right_container_->set_background(
      Background::CreateSolidBackground(240, 240, 240));
  right_container_->SetID(kRightContainerID);
  content_view_->AddChildView(right_container_);
  right_container_->SetBounds(270, 35, 300, 200);

  y = 10;
  int radio_button_height = 18;
  int gap_between_radio_buttons = 10;
  RadioButton* radio_button = new RadioButton(L"Asparagus", 1);
  radio_button->SetID(kAsparagusButtonID);
  right_container_->AddChildView(radio_button);
  radio_button->SetBounds(5, y, 70, radio_button_height);
  radio_button->SetGroup(1);
  y += radio_button_height + gap_between_radio_buttons;
  radio_button = new RadioButton(L"Broccoli", 1);
  radio_button->SetID(kBroccoliButtonID);
  right_container_->AddChildView(radio_button);
  radio_button->SetBounds(5, y, 70, radio_button_height);
  radio_button->SetGroup(1);
  RadioButton* radio_button_to_check = radio_button;
  y += radio_button_height + gap_between_radio_buttons;
  radio_button = new RadioButton(L"Cauliflower", 1);
  radio_button->SetID(kCauliflowerButtonID);
  right_container_->AddChildView(radio_button);
  radio_button->SetBounds(5, y, 70, radio_button_height);
  radio_button->SetGroup(1);
  y += radio_button_height + gap_between_radio_buttons;

  View* inner_container = new View();
  inner_container->set_border(Border::CreateSolidBorder(1, SK_ColorBLACK));
  inner_container->set_background(
      Background::CreateSolidBackground(230, 230, 230));
  inner_container->SetID(kInnerContainerID);
  right_container_->AddChildView(inner_container);
  inner_container->SetBounds(100, 10, 150, 180);

  ScrollView* scroll_view = new ScrollView();
  scroll_view->SetID(kScrollViewID);
  inner_container->AddChildView(scroll_view);
  scroll_view->SetBounds(1, 1, 148, 178);

  View* scroll_content = new View();
  scroll_content->SetBounds(0, 0, 200, 200);
  scroll_content->set_background(
      Background::CreateSolidBackground(200, 200, 200));
  scroll_view->SetContents(scroll_content);

  static const wchar_t* const kTitles[] = {
      L"Rosetta", L"Stupeur et tremblement", L"The diner game",
      L"Ridicule", L"Le placard", L"Les Visiteurs", L"Amelie",
      L"Joyeux Noel", L"Camping", L"Brice de Nice",
      L"Taxi", L"Asterix"
  };

  static const int kIDs[] = {
      kRosettaLinkID, kStupeurEtTremblementLinkID, kDinerGameLinkID,
      kRidiculeLinkID, kClosetLinkID, kVisitingLinkID, kAmelieLinkID,
      kJoyeuxNoelLinkID, kCampingLinkID, kBriceDeNiceLinkID,
      kTaxiLinkID, kAsterixLinkID
  };

  DCHECK(arraysize(kTitles) == arraysize(kIDs));

  y = 5;
  for (size_t i = 0; i < arraysize(kTitles); ++i) {
    Link* link = new Link(kTitles[i]);
    link->SetHorizontalAlignment(Label::ALIGN_LEFT);
    link->SetID(kIDs[i]);
    scroll_content->AddChildView(link);
    link->SetBounds(5, y, 300, 15);
    y += 15;
  }

  y = 250;
  int width = 60;
  button = new NativeButton(NULL, L"OK");
  button->SetID(kOKButtonID);
  button->SetIsDefault(true);

  content_view_->AddChildView(button);
  button->SetBounds(150, y, width, 30);

  button = new NativeButton(NULL, L"Cancel");
  button->SetID(kCancelButtonID);
  content_view_->AddChildView(button);
  button->SetBounds(220, y, width, 30);

  button = new NativeButton(NULL, L"Help");
  button->SetID(kHelpButtonID);
  content_view_->AddChildView(button);
  button->SetBounds(290, y, width, 30);

  y += 40;

  // Left bottom box with style checkboxes.
  View* contents = new View();
  contents->set_background(Background::CreateSolidBackground(SK_ColorWHITE));
  cb = new Checkbox(L"Bold");
  contents->AddChildView(cb);
  cb->SetBounds(10, 10, 50, 20);
  cb->SetID(kBoldCheckBoxID);

  cb = new Checkbox(L"Italic");
  contents->AddChildView(cb);
  cb->SetBounds(70, 10, 50, 20);
  cb->SetID(kItalicCheckBoxID);

  cb = new Checkbox(L"Underlined");
  contents->AddChildView(cb);
  cb->SetBounds(130, 10, 70, 20);
  cb->SetID(kUnderlinedCheckBoxID);

  Link* link = new Link(L"Help");
  contents->AddChildView(link);
  link->SetBounds(10, 35, 70, 10);
  link->SetID(kStyleHelpLinkID);

  text_field = new Textfield();
  contents->AddChildView(text_field);
  text_field->SetBounds(10, 50, 100, 20);
  text_field->SetID(kStyleTextEditID);

  style_tab_ = new TabbedPane();
  style_tab_->SetID(kStyleContainerID);
  content_view_->AddChildView(style_tab_);
  style_tab_->SetBounds(10, y, 210, 100);
  style_tab_->AddTab(L"Style", contents);
  style_tab_->AddTab(L"Other", new View());

  // Right bottom box with search.
  contents = new View();
  contents->set_background(Background::CreateSolidBackground(SK_ColorWHITE));
  text_field = new Textfield();
  contents->AddChildView(text_field);
  text_field->SetBounds(10, 10, 100, 20);
  text_field->SetID(kSearchTextfieldID);

  button = new NativeButton(NULL, L"Search");
  contents->AddChildView(button);
  button->SetBounds(112, 5, 60, 30);
  button->SetID(kSearchButtonID);

  link = new Link(L"Help");
  link->SetHorizontalAlignment(Label::ALIGN_LEFT);
  link->SetID(kHelpLinkID);
  contents->AddChildView(link);
  link->SetBounds(175, 10, 30, 20);

  search_border_view_ = new BorderView(contents);
  search_border_view_->SetID(kSearchContainerID);

  content_view_->AddChildView(search_border_view_);
  search_border_view_->SetBounds(300, y, 240, 50);

  y += 60;

  contents = new View();
  contents->SetFocusable(true);
  contents->set_background(Background::CreateSolidBackground(SK_ColorBLUE));
  contents->SetID(kThumbnailContainerID);
  button = new NativeButton(NULL, L"Star");
  contents->AddChildView(button);
  button->SetBounds(5, 5, 50, 30);
  button->SetID(kThumbnailStarID);
  button = new NativeButton(NULL, L"SuperStar");
  contents->AddChildView(button);
  button->SetBounds(60, 5, 100, 30);
  button->SetID(kThumbnailSuperStarID);

  content_view_->AddChildView(contents);
  contents->SetBounds(250, y, 200, 50);
  // We can only call RadioButton::SetChecked() on the radio-button is part of
  // the view hierarchy.
  radio_button_to_check->SetChecked(true);
}

////////////////////////////////////////////////////////////////////////////////
// The tests
////////////////////////////////////////////////////////////////////////////////

enum FocusTestEventType {
  ON_FOCUS = 0,
  ON_BLUR
};

struct FocusTestEvent {
  FocusTestEvent(FocusTestEventType type, int view_id)
      : type(type),
        view_id(view_id) {
  }

  FocusTestEventType type;
  int view_id;
};

class SimpleTestView : public View {
 public:
  SimpleTestView(std::vector<FocusTestEvent>* event_list, int view_id)
      : event_list_(event_list) {
    SetFocusable(true);
    SetID(view_id);
  }

  virtual void OnFocus() {
    event_list_->push_back(FocusTestEvent(ON_FOCUS, GetID()));
  }

  virtual void OnBlur() {
    event_list_->push_back(FocusTestEvent(ON_BLUR, GetID()));
  }

 private:
  std::vector<FocusTestEvent>* event_list_;
};

// Tests that the appropriate Focus related methods are called when a View
// gets/loses focus.
TEST_F(FocusManagerTest, ViewFocusCallbacks) {
  std::vector<FocusTestEvent> event_list;
  const int kView1ID = 1;
  const int kView2ID = 2;

  SimpleTestView* view1 = new SimpleTestView(&event_list, kView1ID);
  SimpleTestView* view2 = new SimpleTestView(&event_list, kView2ID);
  content_view_->AddChildView(view1);
  content_view_->AddChildView(view2);

  view1->RequestFocus();
  ASSERT_EQ(1, static_cast<int>(event_list.size()));
  EXPECT_EQ(ON_FOCUS, event_list[0].type);
  EXPECT_EQ(kView1ID, event_list[0].view_id);

  event_list.clear();
  view2->RequestFocus();
  ASSERT_EQ(2, static_cast<int>(event_list.size()));
  EXPECT_EQ(ON_BLUR, event_list[0].type);
  EXPECT_EQ(kView1ID, event_list[0].view_id);
  EXPECT_EQ(ON_FOCUS, event_list[1].type);
  EXPECT_EQ(kView2ID, event_list[1].view_id);

  event_list.clear();
  GetFocusManager()->ClearFocus();
  ASSERT_EQ(1, static_cast<int>(event_list.size()));
  EXPECT_EQ(ON_BLUR, event_list[0].type);
  EXPECT_EQ(kView2ID, event_list[0].view_id);
}

typedef std::pair<View*, View*> ViewPair;
class TestFocusChangeListener : public FocusChangeListener {
 public:
  virtual void FocusWillChange(View* focused_before, View* focused_now) {
    focus_changes_.push_back(ViewPair(focused_before, focused_now));
  }

  const std::vector<ViewPair>& focus_changes() const { return focus_changes_; }
  void ClearFocusChanges() { focus_changes_.clear(); }

 private:
  // A vector of which views lost/gained focus.
  std::vector<ViewPair> focus_changes_;
};

TEST_F(FocusManagerTest, FocusChangeListener) {
  View* view1 = new View();
  view1->SetFocusable(true);
  View* view2 = new View();
  view2->SetFocusable(true);
  content_view_->AddChildView(view1);
  content_view_->AddChildView(view2);

  TestFocusChangeListener listener;
  AddFocusChangeListener(&listener);

  // Visual Studio 2010 has problems converting NULL to the null pointer for
  // std::pair.  See http://connect.microsoft.com/VisualStudio/feedback/details/520043/error-converting-from-null-to-a-pointer-type-in-std-pair
  // It will work if we pass nullptr.
#if defined(_MSC_VER) && _MSC_VER >= 1600
  views::View* null_view = nullptr;
#else
  views::View* null_view = NULL;
#endif

  view1->RequestFocus();
  ASSERT_EQ(1, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(null_view, view1));
  listener.ClearFocusChanges();

  view2->RequestFocus();
  ASSERT_EQ(1, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(view1, view2));
  listener.ClearFocusChanges();

  GetFocusManager()->ClearFocus();
  ASSERT_EQ(1, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(view2, null_view));
}

class TestNativeButton : public NativeButton {
 public:
  explicit TestNativeButton(const std::wstring& text)
      : NativeButton(NULL, text) {
  };
  virtual gfx::NativeView TestGetNativeControlView() {
    return native_wrapper_->GetTestingHandle();
  }
};

class TestCheckbox : public Checkbox {
 public:
  explicit TestCheckbox(const std::wstring& text) : Checkbox(text) {
  };
  virtual gfx::NativeView TestGetNativeControlView() {
    return native_wrapper_->GetTestingHandle();
  }
};

class TestRadioButton : public RadioButton {
 public:
  explicit TestRadioButton(const std::wstring& text) : RadioButton(text, 1) {
  };
  virtual gfx::NativeView TestGetNativeControlView() {
    return native_wrapper_->GetTestingHandle();
  }
};

class TestTextfield : public Textfield {
 public:
  TestTextfield() { }
  virtual gfx::NativeView TestGetNativeControlView() {
    return native_wrapper_->GetTestingHandle();
  }
};

class TestCombobox : public Combobox, public ComboboxModel {
 public:
  TestCombobox() : Combobox(this) { }
  virtual gfx::NativeView TestGetNativeControlView() {
    return native_wrapper_->GetTestingHandle();
  }
  virtual int GetItemCount() {
    return 10;
  }
  virtual string16 GetItemAt(int index) {
    return ASCIIToUTF16("Hello combo");
  }
};

class TestTabbedPane : public TabbedPane {
 public:
  TestTabbedPane() { }
  virtual gfx::NativeView TestGetNativeControlView() {
    return native_tabbed_pane_->GetTestingHandle();
  }
};

// Tests that NativeControls do set the focus View appropriately on the
// FocusManager.
TEST_F(FocusManagerTest, FocusNativeControls) {
  TestNativeButton* button = new TestNativeButton(L"Press me");
  TestCheckbox* checkbox = new TestCheckbox(L"Checkbox");
  TestRadioButton* radio_button = new TestRadioButton(L"RadioButton");
  TestTextfield* textfield = new TestTextfield();
  TestCombobox* combobox = new TestCombobox();
  TestTabbedPane* tabbed_pane = new TestTabbedPane();
  TestNativeButton* tab_button = new TestNativeButton(L"tab button");

  content_view_->AddChildView(button);
  content_view_->AddChildView(checkbox);
  content_view_->AddChildView(radio_button);
  content_view_->AddChildView(textfield);
  content_view_->AddChildView(combobox);
  content_view_->AddChildView(tabbed_pane);
  tabbed_pane->AddTab(L"Awesome tab", tab_button);

  // Simulate the native view getting the native focus (such as by user click).
  FocusNativeView(button->TestGetNativeControlView());
  EXPECT_EQ(button, GetFocusManager()->GetFocusedView());

  FocusNativeView(checkbox->TestGetNativeControlView());
  EXPECT_EQ(checkbox, GetFocusManager()->GetFocusedView());

  FocusNativeView(radio_button->TestGetNativeControlView());
  EXPECT_EQ(radio_button, GetFocusManager()->GetFocusedView());

  FocusNativeView(textfield->TestGetNativeControlView());
  EXPECT_EQ(textfield, GetFocusManager()->GetFocusedView());

  FocusNativeView(combobox->TestGetNativeControlView());
  EXPECT_EQ(combobox, GetFocusManager()->GetFocusedView());

  FocusNativeView(tabbed_pane->TestGetNativeControlView());
  EXPECT_EQ(tabbed_pane, GetFocusManager()->GetFocusedView());

  FocusNativeView(tab_button->TestGetNativeControlView());
  EXPECT_EQ(tab_button, GetFocusManager()->GetFocusedView());
}

// Test that when activating/deactivating the top window, the focus is stored/
// restored properly.
TEST_F(FocusManagerTest, FocusStoreRestore) {
  // Simulate an activate, otherwise the deactivate isn't going to do anything.
  SimulateActivateWindow();

  NativeButton* button = new NativeButton(NULL, L"Press me");
  View* view = new View();
  view->SetFocusable(true);

  content_view_->AddChildView(button);
  button->SetBounds(10, 10, 200, 30);
  content_view_->AddChildView(view);
  message_loop()->RunAllPending();

  TestFocusChangeListener listener;
  AddFocusChangeListener(&listener);

  view->RequestFocus();
  message_loop()->RunAllPending();
  //  MessageLoopForUI::current()->Run(new AcceleratorHandler());

  // Visual Studio 2010 has problems converting NULL to the null pointer for
  // std::pair.  See http://connect.microsoft.com/VisualStudio/feedback/details/520043/error-converting-from-null-to-a-pointer-type-in-std-pair
  // It will work if we pass nullptr.
#if defined(_MSC_VER) && _MSC_VER >= 1600
  views::View* null_view = nullptr;
#else
  views::View* null_view = NULL;
#endif

  // Deacivate the window, it should store its focus.
  SimulateDeactivateWindow();
  EXPECT_EQ(NULL, GetFocusManager()->GetFocusedView());
  ASSERT_EQ(2, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(null_view, view));
  EXPECT_TRUE(listener.focus_changes()[1] == ViewPair(view, null_view));
  listener.ClearFocusChanges();

  // Reactivate, focus should come-back to the previously focused view.
  SimulateActivateWindow();
  EXPECT_EQ(view, GetFocusManager()->GetFocusedView());
  ASSERT_EQ(1, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(null_view, view));
  listener.ClearFocusChanges();

  // Same test with a NativeControl.
  button->RequestFocus();
  SimulateDeactivateWindow();
  EXPECT_EQ(NULL, GetFocusManager()->GetFocusedView());
  ASSERT_EQ(2, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(view, button));
  EXPECT_TRUE(listener.focus_changes()[1] == ViewPair(button, null_view));
  listener.ClearFocusChanges();

  SimulateActivateWindow();
  EXPECT_EQ(button, GetFocusManager()->GetFocusedView());
  ASSERT_EQ(1, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(null_view, button));
  listener.ClearFocusChanges();

  /*
  // Now test that while the window is inactive we can change the focused view
  // (we do that in several places).
  SimulateDeactivateWindow();
  // TODO: would have to mock the window being inactive (with a TestWidgetWin
  //       that would return false on IsActive()).
  GetFocusManager()->SetFocusedView(view);
  ::SendMessage(window_->GetNativeWindow(), WM_ACTIVATE, WA_ACTIVE, NULL);

  EXPECT_EQ(view, GetFocusManager()->GetFocusedView());
  ASSERT_EQ(2, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(button, null_view));
  EXPECT_TRUE(listener.focus_changes()[1] == ViewPair(null_view, view));
  */
}

TEST_F(FocusManagerTest, ContainsView) {
  View* view = new View();
  scoped_ptr<View> detached_view(new View());
  TabbedPane* tabbed_pane = new TabbedPane();
  TabbedPane* nested_tabbed_pane = new TabbedPane();
  NativeButton* tab_button = new NativeButton(NULL, L"tab button");

  content_view_->AddChildView(view);
  content_view_->AddChildView(tabbed_pane);
  // Adding a View inside a TabbedPane to test the case of nested root view.

  tabbed_pane->AddTab(L"Awesome tab", nested_tabbed_pane);
  nested_tabbed_pane->AddTab(L"Awesomer tab", tab_button);

  EXPECT_TRUE(GetFocusManager()->ContainsView(view));
  EXPECT_TRUE(GetFocusManager()->ContainsView(tabbed_pane));
  EXPECT_TRUE(GetFocusManager()->ContainsView(nested_tabbed_pane));
  EXPECT_TRUE(GetFocusManager()->ContainsView(tab_button));
  EXPECT_FALSE(GetFocusManager()->ContainsView(detached_view.get()));
}

TEST_F(FocusTraversalTest, NormalTraversal) {
  const int kTraversalIDs[] = { kTopCheckBoxID,  kAppleTextfieldID,
      kOrangeTextfieldID, kBananaTextfieldID, kKiwiTextfieldID,
      kFruitButtonID, kFruitCheckBoxID, kComboboxID, kBroccoliButtonID,
      kRosettaLinkID, kStupeurEtTremblementLinkID,
      kDinerGameLinkID, kRidiculeLinkID, kClosetLinkID, kVisitingLinkID,
      kAmelieLinkID, kJoyeuxNoelLinkID, kCampingLinkID, kBriceDeNiceLinkID,
      kTaxiLinkID, kAsterixLinkID, kOKButtonID, kCancelButtonID, kHelpButtonID,
      kStyleContainerID, kBoldCheckBoxID, kItalicCheckBoxID,
      kUnderlinedCheckBoxID, kStyleHelpLinkID, kStyleTextEditID,
      kSearchTextfieldID, kSearchButtonID, kHelpLinkID,
      kThumbnailContainerID, kThumbnailStarID, kThumbnailSuperStarID };

  // Uncomment the following line if you want to test manually the UI of this
  // test.
  // MessageLoopForUI::current()->Run(new AcceleratorHandler());

  // Let's traverse the whole focus hierarchy (several times, to make sure it
  // loops OK).
  GetFocusManager()->ClearFocus();
  for (int i = 0; i < 3; ++i) {
    for (size_t j = 0; j < arraysize(kTraversalIDs); j++) {
      GetFocusManager()->AdvanceFocus(false);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }

  // Let's traverse in reverse order.
  GetFocusManager()->ClearFocus();
  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kTraversalIDs) - 1; j >= 0; --j) {
      GetFocusManager()->AdvanceFocus(true);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }
}

TEST_F(FocusTraversalTest, TraversalWithNonEnabledViews) {
  const int kDisabledIDs[] = {
      kBananaTextfieldID, kFruitCheckBoxID, kComboboxID, kAsparagusButtonID,
      kCauliflowerButtonID, kClosetLinkID, kVisitingLinkID, kBriceDeNiceLinkID,
      kTaxiLinkID, kAsterixLinkID, kHelpButtonID, kBoldCheckBoxID,
      kSearchTextfieldID, kHelpLinkID };

  const int kTraversalIDs[] = { kTopCheckBoxID,  kAppleTextfieldID,
      kOrangeTextfieldID, kKiwiTextfieldID, kFruitButtonID, kBroccoliButtonID,
      kRosettaLinkID, kStupeurEtTremblementLinkID, kDinerGameLinkID,
      kRidiculeLinkID, kAmelieLinkID, kJoyeuxNoelLinkID, kCampingLinkID,
      kOKButtonID, kCancelButtonID, kStyleContainerID, kItalicCheckBoxID,
      kUnderlinedCheckBoxID, kStyleHelpLinkID, kStyleTextEditID,
      kSearchButtonID, kThumbnailContainerID, kThumbnailStarID,
      kThumbnailSuperStarID };

  // Let's disable some views.
  for (size_t i = 0; i < arraysize(kDisabledIDs); i++) {
    View* v = FindViewByID(kDisabledIDs[i]);
    ASSERT_TRUE(v != NULL);
    v->SetEnabled(false);
  }

  // Uncomment the following line if you want to test manually the UI of this
  // test.
  // MessageLoopForUI::current()->Run(new AcceleratorHandler());

  View* focused_view;
  // Let's do one traversal (several times, to make sure it loops ok).
  GetFocusManager()->ClearFocus();
  for (int i = 0; i < 3; ++i) {
    for (size_t j = 0; j < arraysize(kTraversalIDs); j++) {
      GetFocusManager()->AdvanceFocus(false);
      focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }

  // Same thing in reverse.
  GetFocusManager()->ClearFocus();
  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kTraversalIDs) - 1; j >= 0; --j) {
      GetFocusManager()->AdvanceFocus(true);
      focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }
}

TEST_F(FocusTraversalTest, TraversalWithInvisibleViews) {
  const int kInvisibleIDs[] = { kTopCheckBoxID, kOKButtonID,
      kThumbnailContainerID };

  const int kTraversalIDs[] = { kAppleTextfieldID, kOrangeTextfieldID,
      kBananaTextfieldID, kKiwiTextfieldID, kFruitButtonID, kFruitCheckBoxID,
      kComboboxID, kBroccoliButtonID, kRosettaLinkID,
      kStupeurEtTremblementLinkID, kDinerGameLinkID, kRidiculeLinkID,
      kClosetLinkID, kVisitingLinkID, kAmelieLinkID, kJoyeuxNoelLinkID,
      kCampingLinkID, kBriceDeNiceLinkID, kTaxiLinkID, kAsterixLinkID,
      kCancelButtonID, kHelpButtonID, kStyleContainerID, kBoldCheckBoxID,
      kItalicCheckBoxID, kUnderlinedCheckBoxID, kStyleHelpLinkID,
      kStyleTextEditID, kSearchTextfieldID, kSearchButtonID, kHelpLinkID };


  // Let's make some views invisible.
  for (size_t i = 0; i < arraysize(kInvisibleIDs); i++) {
    View* v = FindViewByID(kInvisibleIDs[i]);
    ASSERT_TRUE(v != NULL);
    v->SetVisible(false);
  }

  // Uncomment the following line if you want to test manually the UI of this
  // test.
  // MessageLoopForUI::current()->Run(new AcceleratorHandler());

  View* focused_view;
  // Let's do one traversal (several times, to make sure it loops ok).
  GetFocusManager()->ClearFocus();
  for (int i = 0; i < 3; ++i) {
    for (size_t j = 0; j < arraysize(kTraversalIDs); j++) {
      GetFocusManager()->AdvanceFocus(false);
      focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }

  // Same thing in reverse.
  GetFocusManager()->ClearFocus();
  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kTraversalIDs) - 1; j >= 0; --j) {
      GetFocusManager()->AdvanceFocus(true);
      focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->GetID());
    }
  }
}

TEST_F(FocusTraversalTest, PaneTraversal) {
  // Tests trapping the traversal within a pane - useful for full
  // keyboard accessibility for toolbars.

  // First test the left container.
  const int kLeftTraversalIDs[] = {
    kAppleTextfieldID,
    kOrangeTextfieldID, kBananaTextfieldID, kKiwiTextfieldID,
    kFruitButtonID, kFruitCheckBoxID, kComboboxID };

  FocusSearch focus_search_left(left_container_, true, false);
  left_container_->EnablePaneFocus(&focus_search_left);
  FindViewByID(kComboboxID)->RequestFocus();

  // Traverse the focus hierarchy within the pane several times.
  for (int i = 0; i < 3; ++i) {
    for (size_t j = 0; j < arraysize(kLeftTraversalIDs); j++) {
      GetFocusManager()->AdvanceFocus(false);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kLeftTraversalIDs[j], focused_view->GetID());
    }
  }

  // Traverse in reverse order.
  FindViewByID(kAppleTextfieldID)->RequestFocus();
  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kLeftTraversalIDs) - 1; j >= 0; --j) {
      GetFocusManager()->AdvanceFocus(true);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kLeftTraversalIDs[j], focused_view->GetID());
    }
  }

  // Now test the right container, but this time with accessibility mode.
  // Make some links not focusable, but mark one of them as
  // "accessibility focusable", so it should show up in the traversal.
  const int kRightTraversalIDs[] = {
    kBroccoliButtonID, kDinerGameLinkID, kRidiculeLinkID,
    kClosetLinkID, kVisitingLinkID, kAmelieLinkID, kJoyeuxNoelLinkID,
    kCampingLinkID, kBriceDeNiceLinkID, kTaxiLinkID, kAsterixLinkID };

  FocusSearch focus_search_right(right_container_, true, true);
  right_container_->EnablePaneFocus(&focus_search_right);
  FindViewByID(kRosettaLinkID)->SetFocusable(false);
  FindViewByID(kStupeurEtTremblementLinkID)->SetFocusable(false);
  FindViewByID(kDinerGameLinkID)->set_accessibility_focusable(true);
  FindViewByID(kDinerGameLinkID)->SetFocusable(false);
  FindViewByID(kAsterixLinkID)->RequestFocus();

  // Traverse the focus hierarchy within the pane several times.
  for (int i = 0; i < 3; ++i) {
    for (size_t j = 0; j < arraysize(kRightTraversalIDs); j++) {
      GetFocusManager()->AdvanceFocus(false);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kRightTraversalIDs[j], focused_view->GetID());
    }
  }

  // Traverse in reverse order.
  FindViewByID(kBroccoliButtonID)->RequestFocus();
  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kRightTraversalIDs) - 1; j >= 0; --j) {
      GetFocusManager()->AdvanceFocus(true);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kRightTraversalIDs[j], focused_view->GetID());
    }
  }

}

// Counts accelerator calls.
class TestAcceleratorTarget : public AcceleratorTarget {
 public:
  explicit TestAcceleratorTarget(bool process_accelerator)
      : accelerator_count_(0), process_accelerator_(process_accelerator) {}

  virtual bool AcceleratorPressed(const Accelerator& accelerator) {
    ++accelerator_count_;
    return process_accelerator_;
  }

  int accelerator_count() const { return accelerator_count_; }

 private:
  int accelerator_count_;  // number of times that the accelerator is activated
  bool process_accelerator_;  // return value of AcceleratorPressed

  DISALLOW_COPY_AND_ASSIGN(TestAcceleratorTarget);
};

TEST_F(FocusManagerTest, CallsNormalAcceleratorTarget) {
  FocusManager* focus_manager = GetFocusManager();
  Accelerator return_accelerator(ui::VKEY_RETURN, false, false, false);
  Accelerator escape_accelerator(ui::VKEY_ESCAPE, false, false, false);

  TestAcceleratorTarget return_target(true);
  TestAcceleratorTarget escape_target(true);
  EXPECT_EQ(return_target.accelerator_count(), 0);
  EXPECT_EQ(escape_target.accelerator_count(), 0);
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(escape_accelerator));

  // Register targets.
  focus_manager->RegisterAccelerator(return_accelerator, &return_target);
  focus_manager->RegisterAccelerator(escape_accelerator, &escape_target);

  // Checks if the correct target is registered.
  EXPECT_EQ(&return_target,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));
  EXPECT_EQ(&escape_target,
            focus_manager->GetCurrentTargetForAccelerator(escape_accelerator));

  // Hitting the return key.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(escape_target.accelerator_count(), 0);

  // Hitting the escape key.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(escape_target.accelerator_count(), 1);

  // Register another target for the return key.
  TestAcceleratorTarget return_target2(true);
  EXPECT_EQ(return_target2.accelerator_count(), 0);
  focus_manager->RegisterAccelerator(return_accelerator, &return_target2);
  EXPECT_EQ(&return_target2,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key; return_target2 has the priority.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(return_target2.accelerator_count(), 1);

  // Register a target that does not process the accelerator event.
  TestAcceleratorTarget return_target3(false);
  EXPECT_EQ(return_target3.accelerator_count(), 0);
  focus_manager->RegisterAccelerator(return_accelerator, &return_target3);
  EXPECT_EQ(&return_target3,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key.
  // Since the event handler of return_target3 returns false, return_target2
  // should be called too.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(return_target2.accelerator_count(), 2);
  EXPECT_EQ(return_target3.accelerator_count(), 1);

  // Unregister return_target2.
  focus_manager->UnregisterAccelerator(return_accelerator, &return_target2);
  EXPECT_EQ(&return_target3,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key. return_target3 and return_target should be called.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 2);
  EXPECT_EQ(return_target2.accelerator_count(), 2);
  EXPECT_EQ(return_target3.accelerator_count(), 2);

  // Unregister targets.
  focus_manager->UnregisterAccelerator(return_accelerator, &return_target);
  focus_manager->UnregisterAccelerator(return_accelerator, &return_target3);
  focus_manager->UnregisterAccelerator(escape_accelerator, &escape_target);

  // Now there is no target registered.
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(escape_accelerator));

  // Hitting the return key and the escape key. Nothing should happen.
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 2);
  EXPECT_EQ(return_target2.accelerator_count(), 2);
  EXPECT_EQ(return_target3.accelerator_count(), 2);
  EXPECT_FALSE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(escape_target.accelerator_count(), 1);
}

// Unregisters itself when its accelerator is invoked.
class SelfUnregisteringAcceleratorTarget : public AcceleratorTarget {
 public:
  SelfUnregisteringAcceleratorTarget(Accelerator accelerator,
                                     FocusManager* focus_manager)
      : accelerator_(accelerator),
        focus_manager_(focus_manager),
        accelerator_count_(0) {
  }

  virtual bool AcceleratorPressed(const Accelerator& accelerator) {
    ++accelerator_count_;
    focus_manager_->UnregisterAccelerator(accelerator, this);
    return true;
  }

  int accelerator_count() const { return accelerator_count_; }

 private:
  Accelerator accelerator_;
  FocusManager* focus_manager_;
  int accelerator_count_;

  DISALLOW_COPY_AND_ASSIGN(SelfUnregisteringAcceleratorTarget);
};

TEST_F(FocusManagerTest, CallsSelfDeletingAcceleratorTarget) {
  FocusManager* focus_manager = GetFocusManager();
  Accelerator return_accelerator(ui::VKEY_RETURN, false, false, false);
  SelfUnregisteringAcceleratorTarget target(return_accelerator, focus_manager);
  EXPECT_EQ(target.accelerator_count(), 0);
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Register the target.
  focus_manager->RegisterAccelerator(return_accelerator, &target);
  EXPECT_EQ(&target,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key. The target will be unregistered.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(target.accelerator_count(), 1);
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key again; nothing should happen.
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(target.accelerator_count(), 1);
}

class MessageTrackingView : public View {
 public:
  MessageTrackingView() : accelerator_pressed_(false) {
 }

  virtual bool OnKeyPressed(const KeyEvent& e) {
    keys_pressed_.push_back(e.key_code());
    return true;
  }

  virtual bool OnKeyReleased(const KeyEvent& e) {
    keys_released_.push_back(e.key_code());
    return true;
  }

  virtual bool AcceleratorPressed(const Accelerator& accelerator) {
    accelerator_pressed_ = true;
    return true;
  }

  void Reset() {
    accelerator_pressed_ = false;
    keys_pressed_.clear();
    keys_released_.clear();
  }

  const std::vector<ui::KeyboardCode>& keys_pressed() const {
    return keys_pressed_;
  }

  const std::vector<ui::KeyboardCode>& keys_released() const {
    return keys_released_;
  }

  bool accelerator_pressed() const {
    return accelerator_pressed_;
  }

 private:
  bool accelerator_pressed_;
  std::vector<ui::KeyboardCode> keys_pressed_;
  std::vector<ui::KeyboardCode> keys_released_;

  DISALLOW_COPY_AND_ASSIGN(MessageTrackingView);
};

#if defined(OS_WIN)
// This test is now Windows only. Linux Views port does not handle accelerator
// keys in AcceleratorHandler anymore. The logic has been moved into
// WidgetGtk::OnKeyEvent().
// Tests that the keyup messages are eaten for accelerators.
TEST_F(FocusManagerTest, IgnoreKeyupForAccelerators) {
  FocusManager* focus_manager = GetFocusManager();
  MessageTrackingView* mtv = new MessageTrackingView();
  mtv->AddAccelerator(Accelerator(ui::VKEY_0, false, false, false));
  mtv->AddAccelerator(Accelerator(ui::VKEY_1, false, false, false));
  content_view_->AddChildView(mtv);
  focus_manager->SetFocusedView(mtv);

  // First send a non-accelerator key sequence.
  PostKeyDown(ui::VKEY_9);
  PostKeyUp(ui::VKEY_9);
  AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  MessageLoopForUI::current()->Run(&accelerator_handler);
  // Make sure we get a key-up and key-down.
  ASSERT_EQ(1U, mtv->keys_pressed().size());
  EXPECT_EQ(ui::VKEY_9, mtv->keys_pressed().at(0));
  ASSERT_EQ(1U, mtv->keys_released().size());
  EXPECT_EQ(ui::VKEY_9, mtv->keys_released().at(0));
  EXPECT_FALSE(mtv->accelerator_pressed());
  mtv->Reset();

  // Same thing with repeat and more than one key at once.
  PostKeyDown(ui::VKEY_9);
  PostKeyDown(ui::VKEY_9);
  PostKeyDown(ui::VKEY_8);
  PostKeyDown(ui::VKEY_9);
  PostKeyDown(ui::VKEY_7);
  PostKeyUp(ui::VKEY_9);
  PostKeyUp(ui::VKEY_7);
  PostKeyUp(ui::VKEY_8);
  MessageLoopForUI::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  MessageLoopForUI::current()->Run(&accelerator_handler);
  // Make sure we get a key-up and key-down.
  ASSERT_EQ(5U, mtv->keys_pressed().size());
  EXPECT_EQ(ui::VKEY_9, mtv->keys_pressed().at(0));
  EXPECT_EQ(ui::VKEY_9, mtv->keys_pressed().at(1));
  EXPECT_EQ(ui::VKEY_8, mtv->keys_pressed().at(2));
  EXPECT_EQ(ui::VKEY_9, mtv->keys_pressed().at(3));
  EXPECT_EQ(ui::VKEY_7, mtv->keys_pressed().at(4));
  ASSERT_EQ(3U, mtv->keys_released().size());
  EXPECT_EQ(ui::VKEY_9, mtv->keys_released().at(0));
  EXPECT_EQ(ui::VKEY_7, mtv->keys_released().at(1));
  EXPECT_EQ(ui::VKEY_8, mtv->keys_released().at(2));
  EXPECT_FALSE(mtv->accelerator_pressed());
  mtv->Reset();

  // Now send an accelerator key sequence.
  PostKeyDown(ui::VKEY_0);
  PostKeyUp(ui::VKEY_0);
  MessageLoopForUI::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  MessageLoopForUI::current()->Run(&accelerator_handler);
  EXPECT_TRUE(mtv->keys_pressed().empty());
  EXPECT_TRUE(mtv->keys_released().empty());
  EXPECT_TRUE(mtv->accelerator_pressed());
  mtv->Reset();

  // Same thing with repeat and more than one key at once.
  PostKeyDown(ui::VKEY_0);
  PostKeyDown(ui::VKEY_1);
  PostKeyDown(ui::VKEY_1);
  PostKeyDown(ui::VKEY_0);
  PostKeyDown(ui::VKEY_0);
  PostKeyUp(ui::VKEY_1);
  PostKeyUp(ui::VKEY_0);
  MessageLoopForUI::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  MessageLoopForUI::current()->Run(&accelerator_handler);
  EXPECT_TRUE(mtv->keys_pressed().empty());
  EXPECT_TRUE(mtv->keys_released().empty());
  EXPECT_TRUE(mtv->accelerator_pressed());
  mtv->Reset();
}
#endif

#if defined(OS_WIN)
// Test that the focus manager is created successfully for the first view
// window parented to a native dialog.
TEST_F(FocusManagerTest, CreationForNativeRoot) {
  // Create a window class.
  WNDCLASSEX class_ex;
  memset(&class_ex, 0, sizeof(class_ex));
  class_ex.cbSize = sizeof(WNDCLASSEX);
  class_ex.lpfnWndProc = &DefWindowProc;
  class_ex.lpszClassName = L"TestWindow";
  ATOM atom = RegisterClassEx(&class_ex);
  ASSERT_TRUE(atom);

  // Create a native dialog window.
  HWND hwnd = CreateWindowEx(0, class_ex.lpszClassName, NULL,
                             WS_OVERLAPPEDWINDOW, 0, 0, 200, 200,
                             NULL, NULL, NULL, NULL);
  ASSERT_TRUE(hwnd);

  // Create a view window parented to native dialog.
  WidgetWin window1;
  window1.set_delete_on_destroy(false);
  window1.set_window_style(WS_CHILD);
  window1.Init(hwnd, gfx::Rect(0, 0, 100, 100));

  // Get the focus manager directly from the first window.  Should exist
  // because the first window is the root widget.
  views::FocusManager* focus_manager_member1 = window1.GetFocusManager();
  EXPECT_TRUE(focus_manager_member1);

  // Create another view window parented to the first view window.
  WidgetWin window2;
  window2.set_delete_on_destroy(false);
  window2.set_window_style(WS_CHILD);
  window2.Init(window1.GetNativeView(), gfx::Rect(0, 0, 100, 100));

  // Get the focus manager directly from the second window. Should return the
  // first window's focus manager.
  views::FocusManager* focus_manager_member2 = window2.GetFocusManager();
  EXPECT_EQ(focus_manager_member2, focus_manager_member1);

  // Get the focus manager indirectly using the first window handle. Should
  // return the first window's focus manager.
  views::FocusManager* focus_manager_indirect =
      views::FocusManager::GetFocusManagerForNativeView(
          window1.GetNativeView());
  EXPECT_EQ(focus_manager_indirect, focus_manager_member1);

  // Get the focus manager indirectly using the second window handle. Should
  // return the first window's focus manager.
  focus_manager_indirect =
      views::FocusManager::GetFocusManagerForNativeView(
          window2.GetNativeView());
  EXPECT_EQ(focus_manager_indirect, focus_manager_member1);

  DestroyWindow(hwnd);
}
#endif

#if defined(OS_CHROMEOS)
class FocusManagerDtorTest : public FocusManagerTest {
 protected:
  typedef std::vector<std::string> DtorTrackVector;

  class FocusManagerDtorTracked : public FocusManager {
   public:
    FocusManagerDtorTracked(Widget* widget, DtorTrackVector* dtor_tracker)
      : FocusManager(widget),
        dtor_tracker_(dtor_tracker) {
    }

    virtual ~FocusManagerDtorTracked() {
      dtor_tracker_->push_back("FocusManagerDtorTracked");
    }

    DtorTrackVector* dtor_tracker_;
  };

  class NativeButtonDtorTracked : public NativeButton {
   public:
    NativeButtonDtorTracked(const std::wstring& text,
                            DtorTrackVector* dtor_tracker)
        : NativeButton(NULL, text),
          dtor_tracker_(dtor_tracker) {
    };
    virtual ~NativeButtonDtorTracked() {
      dtor_tracker_->push_back("NativeButtonDtorTracked");
    }

    DtorTrackVector* dtor_tracker_;
  };

  class WindowGtkDtorTracked : public WindowGtk {
   public:
    WindowGtkDtorTracked(WindowDelegate* window_delegate,
                         DtorTrackVector* dtor_tracker)
        : WindowGtk(window_delegate),
          dtor_tracker_(dtor_tracker) {
      tracked_focus_manager_ = new FocusManagerDtorTracked(this,
          dtor_tracker_);
      // Replace focus_manager_ with FocusManagerDtorTracked
      set_focus_manager(tracked_focus_manager_);

      GetNonClientView()->SetFrameView(CreateFrameViewForWindow());
      InitWindow(NULL, gfx::Rect(0, 0, 100, 100));
    }

    virtual ~WindowGtkDtorTracked() {
      dtor_tracker_->push_back("WindowGtkDtorTracked");
    }

    FocusManagerDtorTracked* tracked_focus_manager_;
    DtorTrackVector* dtor_tracker_;
  };

 public:
  virtual void SetUp() {
   // Create WindowGtkDtorTracked that uses FocusManagerDtorTracked.
   window_ = new WindowGtkDtorTracked(this, &dtor_tracker_);
   ASSERT_TRUE(GetFocusManager() ==
        static_cast<WindowGtkDtorTracked*>(window_)->tracked_focus_manager_);

   window_->Show();
  }

  virtual void TearDown() {
    if (window_) {
      window_->Close();
      message_loop()->RunAllPending();
    }
  }

  DtorTrackVector dtor_tracker_;
};

TEST_F(FocusManagerDtorTest, FocusManagerDestructedLast) {
  // Setup views hierarchy.
  TabbedPane* tabbed_pane = new TabbedPane();
  content_view_->AddChildView(tabbed_pane);

  NativeButtonDtorTracked* button = new NativeButtonDtorTracked(L"button",
                                                                &dtor_tracker_);
  tabbed_pane->AddTab(L"Awesome tab", button);

  // Close the window.
  window_->Close();
  message_loop()->RunAllPending();

  // Test window, button and focus manager should all be destructed.
  ASSERT_EQ(3, static_cast<int>(dtor_tracker_.size()));

  // Focus manager should be the last one to destruct.
  ASSERT_STREQ("FocusManagerDtorTracked", dtor_tracker_[2].c_str());

  // Clear window_ so that we don't try to close it again.
  window_ = NULL;
}

#endif  // defined(OS_CHROMEOS)

}  // namespace views
