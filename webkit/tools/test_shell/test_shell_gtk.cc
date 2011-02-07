// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell.h"

#include <errno.h>
#include <fcntl.h>
#include <fontconfig/fontconfig.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <unistd.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "grit/test_shell_resources.h"
#include "grit/webkit_resources.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/data_pack.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_webview_delegate.h"

using WebKit::WebPoint;
using WebKit::WebWidget;

namespace {

// Convert a FilePath into an FcChar* (used by fontconfig).
// The pointer only lives for the duration for the expression.
const FcChar8* FilePathAsFcChar(const FilePath& path) {
  return reinterpret_cast<const FcChar8*>(path.value().c_str());
}

// Data resources on linux.  This is a pointer to the mmapped resources file.
ui::DataPack* g_resource_data_pack = NULL;

void TerminationSignalHandler(int signatl) {
  TestShell::ShutdownTestShell();
  exit(0);
}

// GTK callbacks ------------------------------------------------------

// Callback for when the main window is destroyed.
gboolean MainWindowDestroyed(GtkWindow* window, TestShell* shell) {
  TestShell::RemoveWindowFromList(window);

  if (TestShell::windowList()->empty() || shell->is_modal()) {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     new MessageLoop::QuitTask());
  }

  delete shell;

  return FALSE;  // Don't stop this message.
}

gboolean MainWindowLostFocus(GtkWidget* widget, GdkEventFocus* event,
                             TestShell* shell) {
  shell->ClosePopup();
  return FALSE;
}

// Callback for when you click the back button.
void BackButtonClicked(GtkButton* button, TestShell* shell) {
  shell->GoBackOrForward(-1);
}

// Callback for when you click the forward button.
void ForwardButtonClicked(GtkButton* button, TestShell* shell) {
  shell->GoBackOrForward(1);
}

// Callback for when you click the stop button.
void StopButtonClicked(GtkButton* button, TestShell* shell) {
  shell->webView()->mainFrame()->stopLoading();
}

// Callback for when you click the reload button.
void ReloadButtonClicked(GtkButton* button, TestShell* shell) {
  shell->Reload();
}

// Callback for when you press enter in the URL box.
void URLEntryActivate(GtkEntry* entry, TestShell* shell) {
  const gchar* url = gtk_entry_get_text(entry);
  shell->LoadURL(GURL(url));
}

// Callback for Debug > Dump body text... menu item.
gboolean DumpBodyTextActivated(GtkWidget* widget, TestShell* shell) {
  shell->DumpDocumentText();
  return FALSE;  // Don't stop this message.
}

// Callback for Debug > Dump render tree... menu item.
gboolean DumpRenderTreeActivated(GtkWidget* widget, TestShell* shell) {
  shell->DumpRenderTree();
  return FALSE;  // Don't stop this message.
}

// Callback for Debug > Show web inspector... menu item.
gboolean ShowWebInspectorActivated(GtkWidget* widget, TestShell* shell) {
  shell->webView()->inspectElementAt(WebPoint());
  return FALSE;  // Don't stop this message.
}

// GTK utility functions ----------------------------------------------

GtkWidget* AddMenuEntry(GtkWidget* menu_widget, const char* text,
                        GCallback callback, TestShell* shell) {
  GtkWidget* entry = gtk_menu_item_new_with_label(text);
  g_signal_connect(entry, "activate", callback, shell);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_widget), entry);
  return entry;
}

GtkWidget* CreateMenu(GtkWidget* menu_bar, const char* text) {
  GtkWidget* menu_widget = gtk_menu_new();
  GtkWidget* menu_header = gtk_menu_item_new_with_label(text);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_header), menu_widget);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), menu_header);
  return menu_widget;
}

GtkWidget* CreateMenuBar(TestShell* shell) {
  GtkWidget* menu_bar = gtk_menu_bar_new();
  GtkWidget* debug_menu = CreateMenu(menu_bar, "Debug");
  AddMenuEntry(debug_menu, "Dump body text...",
               G_CALLBACK(DumpBodyTextActivated), shell);
  AddMenuEntry(debug_menu, "Dump render tree...",
               G_CALLBACK(DumpRenderTreeActivated), shell);
  AddMenuEntry(debug_menu, "Show web inspector...",
               G_CALLBACK(ShowWebInspectorActivated), shell);
  return menu_bar;
}

}  // namespace

// static
void TestShell::InitializeTestShell(bool layout_test_mode,
                                    bool allow_external_pages) {
  window_list_ = new WindowList;
  layout_test_mode_ = layout_test_mode;
  allow_external_pages_ = allow_external_pages;

  web_prefs_ = new WebPreferences;

  g_resource_data_pack = new ui::DataPack;
  FilePath data_path;
  PathService::Get(base::DIR_EXE, &data_path);
  data_path = data_path.Append("test_shell.pak");
  if (!g_resource_data_pack->Load(data_path)) {
    LOG(FATAL) << "failed to load test_shell.pak";
  }

  FilePath resources_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &resources_dir);
  resources_dir = resources_dir.Append("webkit/tools/test_shell/resources");

  ResetWebPreferences();

  // We wish to make the layout tests reproducable with respect to fonts. Skia
  // uses fontconfig to resolve font family names from WebKit into actual font
  // files found on the current system. This means that fonts vary based on the
  // system and also on the fontconfig configuration.
  //
  // To avoid this we initialise fontconfig here and install a configuration
  // which only knows about a few, select, fonts.

  // We have fontconfig parse a config file from our resources file. This
  // sets a number of aliases ("sans"->"Arial" etc), but doesn't include any
  // font directories.
  if (layout_test_mode) {
    FcInit();

    FcConfig* fontcfg = FcConfigCreate();
    FilePath fontconfig_path = resources_dir.Append("fonts.conf");
    if (!FcConfigParseAndLoad(fontcfg, FilePathAsFcChar(fontconfig_path),
                              true)) {
      LOG(FATAL) << "Failed to parse fontconfig config file";
    }

    // This is the list of fonts that fontconfig will know about. It
    // will try its best to match based only on the fonts here in. The
    // paths are where these fonts are found on our Ubuntu boxes.
    static const char *const fonts[] = {
      "/usr/share/fonts/truetype/kochi/kochi-gothic.ttf",
      "/usr/share/fonts/truetype/kochi/kochi-mincho.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold_Italic.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Arial_Italic.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS_Bold.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Courier_New.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold_Italic.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Italic.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Georgia.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Georgia_Bold.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Georgia_Bold_Italic.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Georgia_Italic.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Impact.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS_Bold.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS_Bold_Italic.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS_Italic.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold_Italic.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Italic.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Verdana.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Verdana_Bold.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Verdana_Bold_Italic.ttf",
      "/usr/share/fonts/truetype/msttcorefonts/Verdana_Italic.ttf",
      // The DejaVuSans font is used by the css2.1 tests.
      "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/ttf-indic-fonts-core/lohit_ta.ttf",
      "/usr/share/fonts/truetype/ttf-indic-fonts-core/MuktiNarrow.ttf",
    };
    for (size_t i = 0; i < arraysize(fonts); ++i) {
      if (access(fonts[i], R_OK)) {
        LOG(FATAL) << "You are missing " << fonts[i] << ". "
                   << "Try installing msttcorefonts. Also see "
                   << "http://code.google.com/p/chromium/wiki/"
                   << "LinuxBuildInstructions";
      }
      if (!FcConfigAppFontAddFile(fontcfg, (FcChar8 *) fonts[i]))
        LOG(FATAL) << "Failed to load font " << fonts[i];
    }

    // We special case these fonts because they're only needed in a
    // few layout tests.
    static const char* const optional_fonts[] = {
      "/usr/share/fonts/truetype/ttf-lucida/LucidaSansRegular.ttf",

      // This font changed paths across Ubuntu releases.
      "/usr/share/fonts/truetype/ttf-indic-fonts-core/lohit_pa.ttf",
      "/usr/share/fonts/truetype/ttf-punjabi-fonts/lohit_pa.ttf",
    };
    for (size_t i = 0; i < arraysize(optional_fonts); ++i) {
      const char* font = optional_fonts[i];
      if (access(font, R_OK) < 0) {
        LOG(WARNING) << "You are missing " << font << ". "
                     << "Without this, some layout tests will fail. "
                     << "See http://code.google.com/p/chromium/wiki/"
                     << "LinuxBuildInstructionsPrerequisites for more.";
      } else {
        if (!FcConfigAppFontAddFile(fontcfg, (FcChar8 *) font))
          LOG(FATAL) << "Failed to load font " << font;
      }
    }

    // Also load the layout-test-specific "Ahem" font.
    FilePath ahem_path = resources_dir.Append("AHEM____.TTF");
    if (!FcConfigAppFontAddFile(fontcfg, FilePathAsFcChar(ahem_path))) {
      LOG(FATAL) << "Failed to load font " << ahem_path.value().c_str();
    }

    if (!FcConfigSetCurrent(fontcfg))
      LOG(FATAL) << "Failed to set the default font configuration";
  }

  // Install an signal handler so we clean up after ourselves.
  signal(SIGINT, TerminationSignalHandler);
  signal(SIGTERM, TerminationSignalHandler);
}

void TestShell::PlatformShutdown() {
  delete g_resource_data_pack;
  g_resource_data_pack = NULL;
}

void TestShell::PlatformCleanUp() {
  if (m_mainWnd) {
    // Disconnect our MainWindowDestroyed handler so that we don't go through
    // the shutdown process more than once.
    g_signal_handlers_disconnect_by_func(m_mainWnd,
        reinterpret_cast<gpointer>(MainWindowDestroyed), this);
    gtk_widget_destroy(GTK_WIDGET(m_mainWnd));
  }
}

void TestShell::EnableUIControl(UIControl control, bool is_enabled) {
  // TODO(darin): Implement me.
}

bool TestShell::Initialize(const GURL& starting_url) {
  m_mainWnd = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  gtk_window_set_title(m_mainWnd, "Test Shell");
  // Null out m_mainWnd when it is destroyed so we don't destroy it twice.
  g_signal_connect(G_OBJECT(m_mainWnd), "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &m_mainWnd);
  g_signal_connect(G_OBJECT(m_mainWnd), "destroy",
                   G_CALLBACK(MainWindowDestroyed), this);
  g_signal_connect(G_OBJECT(m_mainWnd), "focus-out-event",
                   G_CALLBACK(MainWindowLostFocus), this);
  g_object_set_data(G_OBJECT(m_mainWnd), "test-shell", this);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);

  GtkWidget* menu_bar = CreateMenuBar(this);

  gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);

  GtkWidget* toolbar = gtk_toolbar_new();
  // Turn off the labels on the toolbar buttons.
  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

  GtkToolItem* back = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
  g_signal_connect(back, "clicked",
                   G_CALLBACK(BackButtonClicked), this);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), back, -1 /* append */);

  GtkToolItem* forward = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
  g_signal_connect(forward, "clicked",
                   G_CALLBACK(ForwardButtonClicked), this);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), forward, -1 /* append */);

  GtkToolItem* reload = gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH);
  g_signal_connect(reload, "clicked",
                   G_CALLBACK(ReloadButtonClicked), this);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), reload, -1 /* append */);

  GtkToolItem* stop = gtk_tool_button_new_from_stock(GTK_STOCK_STOP);
  g_signal_connect(stop, "clicked",
                   G_CALLBACK(StopButtonClicked), this);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), stop, -1 /* append */);

  m_editWnd = gtk_entry_new();
  g_signal_connect(G_OBJECT(m_editWnd), "activate",
                   G_CALLBACK(URLEntryActivate), this);
  gtk_entry_set_text(GTK_ENTRY(m_editWnd), starting_url.spec().c_str());

  GtkToolItem* tool_item = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(tool_item), m_editWnd);
  gtk_tool_item_set_expand(tool_item, TRUE);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool_item, -1 /* append */);

  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
  m_webViewHost.reset(
      WebViewHost::Create(vbox, delegate_.get(), 0, *TestShell::web_prefs_));

  gtk_container_add(GTK_CONTAINER(m_mainWnd), vbox);
  gtk_widget_show_all(GTK_WIDGET(m_mainWnd));

  // LoadURL will do a resize, so make sure we don't call LoadURL
  // until we've completed all of our GTK setup.
  if (starting_url.is_valid())
    LoadURL(starting_url);

  if (IsSVGTestURL(starting_url))
    SizeToSVG();
  else
    SizeToDefault();

  return true;
}

void TestShell::TestFinished() {
  if(!test_is_pending_)
    return;

  test_is_pending_ = false;
  MessageLoop::current()->Quit();
}

void TestShell::SizeTo(int width, int height) {
  GtkWidget* widget = m_webViewHost->view_handle();
  if (widget->allocation.width == width &&
      widget->allocation.height == height) {
    // Nothing to do.
    return;
  }

  gtk_widget_set_size_request(widget, width, height);
  if (widget->allocation.width > width ||
      widget->allocation.height > height) {
    // We've been sized smaller.  Shrink the window so it snaps back to the
    // appropriate size.
    gtk_window_resize(GTK_WINDOW(m_mainWnd), 1, 1);
  }

  // We've been asked to size the content area to a particular size.
  // GTK works asynchronously: you request a size and then it
  // eventually becomes that size.  But layout tests need to be sure
  // the resize has gone into WebKit by the time SizeTo() returns.
  // Force the webkit resize to happen now.
  m_webViewHost->Resize(gfx::Size(width, height));
}

static void AlarmHandler(int signatl) {
  // If the alarm alarmed, kill the process since we have a really bad hang.
  puts("\n#TEST_TIMED_OUT\n");
  puts("#EOF\n");
  fflush(stdout);
  TestShell::ShutdownTestShell();
  exit(0);
}

void TestShell::WaitTestFinished() {
  DCHECK(!test_is_pending_) << "cannot be used recursively";

  test_is_pending_ = true;

  // Install an alarm signal handler that will kill us if we time out.
  signal(SIGALRM, AlarmHandler);
  alarm(GetLayoutTestTimeoutForWatchDog() / 1000);

  // TestFinished() will post a quit message to break this loop when the page
  // finishes loading.
  while (test_is_pending_)
    MessageLoop::current()->Run();

  // Remove the alarm.
  alarm(0);
  signal(SIGALRM, SIG_DFL);
}

void TestShell::InteractiveSetFocus(WebWidgetHost* host, bool enable) {
  GtkWidget* widget = GTK_WIDGET(host->view_handle());

  if (enable) {
    gtk_widget_grab_focus(widget);
  } else if (gtk_widget_is_focus(widget)) {
    GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
    if (GTK_WIDGET_TOPLEVEL(toplevel))
      gtk_window_set_focus(GTK_WINDOW(toplevel), NULL);
  }
}

void TestShell::DestroyWindow(gfx::NativeWindow windowHandle) {
  RemoveWindowFromList(windowHandle);
  gtk_widget_destroy(GTK_WIDGET(windowHandle));
}

WebWidget* TestShell::CreatePopupWidget() {
  GtkWidget* popupwindow = gtk_window_new(GTK_WINDOW_POPUP);
  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
  WebWidgetHost* host = WebWidgetHost::Create(vbox, popup_delegate_.get());
  gtk_container_add(GTK_CONTAINER(popupwindow), vbox);
  m_popupHost = host;

  // Grab all input to the test shell and funnel it to the popup.
  // The popup will detect if mouseclicks are outside its bounds and destroy
  // itself if so. Clicks that are outside the test_shell window will destroy
  // the popup by taking focus away from the main window.
  gtk_grab_add(host->view_handle());

  return host->webwidget();
}

void TestShell::ClosePopup() {
  if (!m_popupHost)
    return;
  GtkWidget* drawing_area = m_popupHost->view_handle();
  // gtk_widget_destroy will recursively call ClosePopup, so to avoid GTK
  // warnings set m_popupHost to null here before making the call.
  m_popupHost = NULL;
  GtkWidget* window =
      gtk_widget_get_parent(gtk_widget_get_parent(drawing_area));
  gtk_widget_destroy(window);
}

void TestShell::ResizeSubViews() {
  // This function is used on Windows to re-layout the window on a resize.
  // GTK manages layout for us so we do nothing.
}

/* static */ void TestShell::DumpAllBackForwardLists(string16* result) {
  result->clear();
  for (WindowList::iterator iter = TestShell::windowList()->begin();
       iter != TestShell::windowList()->end(); iter++) {
    GtkWindow* window = *iter;
    TestShell* shell =
        static_cast<TestShell*>(g_object_get_data(G_OBJECT(window),
                                                  "test-shell"));
    shell->DumpBackForwardList(result);
  }
}

void TestShell::LoadURLForFrame(const GURL& url,
                                const std::wstring& frame_name) {
  if (!url.is_valid())
    return;

  if (IsSVGTestURL(url)) {
    SizeToSVG();
  } else {
    // only resize back to the default when running tests
    if (layout_test_mode())
      SizeToDefault();
  }

  navigation_controller_->LoadEntry(
      new TestNavigationEntry(-1, url, std::wstring(), frame_name));
}

bool TestShell::PromptForSaveFile(const wchar_t* prompt_title,
                                  FilePath* result) {
  GtkWidget* dialog;
  dialog = gtk_file_chooser_dialog_new(WideToUTF8(prompt_title).c_str(),
                                       GTK_WINDOW(m_mainWnd),
                                       GTK_FILE_CHOOSER_ACTION_SAVE,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                       NULL);  // Terminate (button, id) pairs.
  gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),
                                                 TRUE);
  int dialog_result = gtk_dialog_run(GTK_DIALOG(dialog));
  if (dialog_result != GTK_RESPONSE_ACCEPT) {
    gtk_widget_destroy(dialog);
    return false;
  }
  char* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
  gtk_widget_destroy(dialog);
  *result = FilePath(path);
  g_free(path);
  return true;
}

// static
std::string TestShell::RewriteLocalUrl(const std::string& url) {
  // Convert file:///tmp/LayoutTests urls to the actual location on disk.
  const char kPrefix[] = "file:///tmp/LayoutTests/";
  const int kPrefixLen = arraysize(kPrefix) - 1;

  std::string new_url(url);
  if (url.compare(0, kPrefixLen, kPrefix, kPrefixLen) == 0) {
    FilePath replace_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &replace_path);
    replace_path = replace_path.Append(
        "third_party/WebKit/LayoutTests/");
    new_url = std::string("file://") + replace_path.value() +
        url.substr(kPrefixLen);
  }

  return new_url;
}

// static
void TestShell::ShowStartupDebuggingDialog() {
  GtkWidget* dialog = gtk_message_dialog_new(
    NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "attach to me?");
  gtk_window_set_title(GTK_WINDOW(dialog), "test_shell");
  gtk_dialog_run(GTK_DIALOG(dialog));  // Runs a nested message loop.
  gtk_widget_destroy(dialog);
}

// static
base::StringPiece TestShell::ResourceProvider(int key) {
  base::StringPiece res;
  g_resource_data_pack->GetStringPiece(key, &res);
  return res;
}

//-----------------------------------------------------------------------------

namespace webkit_glue {

string16 GetLocalizedString(int message_id) {
  base::StringPiece res;
  if (!g_resource_data_pack->GetStringPiece(message_id, &res)) {
    LOG(FATAL) << "failed to load webkit string with id " << message_id;
  }

  return string16(reinterpret_cast<const char16*>(res.data()),
                  res.length() / 2);
}

base::StringPiece GetDataResource(int resource_id) {
  switch (resource_id) {
    case IDR_BROKENIMAGE:
      resource_id = IDR_BROKENIMAGE_TESTSHELL;
      break;
    case IDR_TEXTAREA_RESIZER:
      resource_id = IDR_TEXTAREA_RESIZER_TESTSHELL;
      break;
  }
  return TestShell::ResourceProvider(resource_id);
}

}  // namespace webkit_glue
