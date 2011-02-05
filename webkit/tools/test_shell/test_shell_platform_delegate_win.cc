// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <list>
#include <windows.h>
#include <commctrl.h>

#include "base/command_line.h"
#include "base/event_recorder.h"
#include "base/resource_util.h"
#include "base/win/win_util.h"
#include "ui/gfx/native_theme_win.h"
#include "webkit/tools/test_shell/foreground_helper.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_platform_delegate.h"

TestShellPlatformDelegate::TestShellPlatformDelegate(
    const CommandLine& command_line)
    : command_line_(command_line) {
#ifdef _CRTDBG_MAP_ALLOC
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#endif
}

TestShellPlatformDelegate::~TestShellPlatformDelegate() {
#ifdef _CRTDBG_MAP_ALLOC
  _CrtDumpMemoryLeaks();
#endif
}

void TestShellPlatformDelegate::PreflightArgs(int *argc, char ***argv) {
}



// This test approximates whether you are running the default themes for
// your platform by inspecting a couple of metrics.
// It does not catch all cases, but it does pick up on classic vs xp,
// and normal vs large fonts. Something it misses is changes to the color
// scheme (which will infact cause pixel test failures).
bool TestShellPlatformDelegate::CheckLayoutTestSystemDependencies() {
  std::list<std::string> errors;

  OSVERSIONINFOEX osvi;
  ::ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  ::GetVersionEx((OSVERSIONINFO *)&osvi);

  // Default to XP metrics, override if on Vista or win 7.
  int requiredVScrollSize = 17;
  int requiredFontSize = -11; // 8 pt
  const wchar_t* requiredFont = L"Tahoma";
  bool isVista = false;
  bool isWin7 = false;
  if (osvi.dwMajorVersion == 6
      && osvi.dwMinorVersion == 1
      && osvi.wProductType == VER_NT_WORKSTATION) {
    requiredFont = L"Segoe UI";
    requiredFontSize = -12;
    isWin7 = true;
  } else if (osvi.dwMajorVersion == 6
      && osvi.dwMinorVersion == 0
      && osvi.wProductType == VER_NT_WORKSTATION) {
    requiredFont = L"Segoe UI";
    requiredFontSize = -12; // 9 pt
    isVista = true;
  } else if (!(osvi.dwMajorVersion == 5
              && osvi.dwMinorVersion == 1
              && osvi.wProductType == VER_NT_WORKSTATION)) {
    // The above check is for XP, so that means ...
    errors.push_back("Unsupported Operating System version "
                     "(must use XP, Vista, or Windows 7).");
  }

  // This metric will be 17 when font size is "Normal".
  // The size of drop-down menus depends on it.
  int vScrollSize = ::GetSystemMetrics(SM_CXVSCROLL);
  if (vScrollSize != requiredVScrollSize) {
    errors.push_back("Must use normal size fonts (96 dpi).");
  }

  // ClearType must be disabled, because the rendering is unpredictable.
  BOOL bFontSmoothing;
  SystemParametersInfo(SPI_GETFONTSMOOTHING, (UINT)0,
                       (PVOID)&bFontSmoothing, (UINT)0);
  int fontSmoothingType;
  SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, (UINT)0,
                       (PVOID)&fontSmoothingType, (UINT)0);
  if (bFontSmoothing && (fontSmoothingType == FE_FONTSMOOTHINGCLEARTYPE)) {
    errors.push_back("ClearType must be disabled.");
  }

  // Check that we're using the default system fonts
  NONCLIENTMETRICS metrics;
  base::win::GetNonClientMetrics(&metrics);
  LOGFONTW* system_fonts[] =
  { &metrics.lfStatusFont, &metrics.lfMenuFont, &metrics.lfSmCaptionFont };

  for (size_t i = 0; i < arraysize(system_fonts); ++i) {
    if (system_fonts[i]->lfHeight != requiredFontSize ||
        wcscmp(requiredFont, system_fonts[i]->lfFaceName)) {
      if (isVista || isWin7)
        errors.push_back("Must use either the Aero or Basic theme.");
      else
        errors.push_back("Must use the default XP theme (Luna).");
      break;
    }
  }

  if (!errors.empty()) {
    fprintf(stderr, "%s",
      "##################################################################\n"
      "## Layout test system dependencies check failed.\n"
      "##\n");
    for (std::list<std::string>::iterator it = errors.begin();
         it != errors.end();
         ++it) {
      fprintf(stderr, "## %s\n", it->c_str());
    }
    fprintf(stderr, "%s",
      "##\n"
      "##################################################################\n");
  }
  return errors.empty();
}

void TestShellPlatformDelegate::SuppressErrorReporting() {
  _set_abort_behavior(0, _WRITE_ABORT_MSG);
}

void TestShellPlatformDelegate::InitializeGUI() {
  INITCOMMONCONTROLSEX InitCtrlEx;

  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&InitCtrlEx);
  TestShell::RegisterWindowClass();
}

void TestShellPlatformDelegate::SelectUnifiedTheme() {
  gfx::NativeTheme::instance()->DisableTheming();
}

void TestShellPlatformDelegate::SetWindowPositionForRecording(
    TestShell *shell) {
  // Move the window to the upper left corner for consistent
  // record/playback mode.  For automation, we want this to work
  // on build systems where the script invoking us is a background
  // process.  So for this case, make our window the topmost window
  // as well.
  ForegroundHelper::SetForeground(shell->mainWnd());
  ::SetWindowPos(shell->mainWnd(), HWND_TOP, 0, 0, 600, 800, 0);
}
