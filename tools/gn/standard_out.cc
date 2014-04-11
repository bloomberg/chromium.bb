// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/standard_out.h"

#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#else
#include <stdio.h>
#include <unistd.h>
#endif

namespace {

bool initialized = false;

static const char kSwitchColor[] = "color";
static const char kSwitchNoColor[] = "nocolor";

#if defined(OS_WIN)
HANDLE hstdout;
WORD default_attributes;
#endif
bool is_console = false;

void EnsureInitialized() {
  if (initialized)
    return;
  initialized = true;

  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(kSwitchNoColor)) {
    // Force color off.
    is_console = false;
    return;
  }

#if defined(OS_WIN)
  // On Windows, we can't force the color on. If the output handle isn't a
  // console, there's nothing we can do about it.
  hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO info;
  is_console = !!::GetConsoleScreenBufferInfo(hstdout, &info);
  default_attributes = info.wAttributes;
#else
  if (cmdline->HasSwitch(kSwitchColor))
    is_console = true;
  else
    is_console = isatty(fileno(stdout));
#endif
}

void WriteToStdOut(const std::string& output) {
  size_t written_bytes = fwrite(output.data(), 1, output.size(), stdout);
  DCHECK_EQ(output.size(), written_bytes);
}

}  // namespace

#if defined(OS_WIN)

void OutputString(const std::string& output, TextDecoration dec) {
  EnsureInitialized();
  if (is_console) {
    switch (dec) {
      case DECORATION_NONE:
        break;
      case DECORATION_DIM:
        ::SetConsoleTextAttribute(hstdout, FOREGROUND_INTENSITY);
        break;
      case DECORATION_RED:
        ::SetConsoleTextAttribute(hstdout,
                                  FOREGROUND_RED | FOREGROUND_INTENSITY);
        break;
      case DECORATION_GREEN:
        // Keep green non-bold.
        ::SetConsoleTextAttribute(hstdout, FOREGROUND_GREEN);
        break;
      case DECORATION_BLUE:
        ::SetConsoleTextAttribute(hstdout,
                                  FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        break;
      case DECORATION_YELLOW:
        ::SetConsoleTextAttribute(hstdout,
                                  FOREGROUND_RED | FOREGROUND_GREEN);
        break;
    }
  }

  DWORD written = 0;
  ::WriteFile(hstdout, output.c_str(), static_cast<DWORD>(output.size()),
              &written, NULL);

  if (is_console)
    ::SetConsoleTextAttribute(hstdout, default_attributes);
}

#else

void OutputString(const std::string& output, TextDecoration dec) {
  EnsureInitialized();
  if (is_console) {
    switch (dec) {
      case DECORATION_NONE:
        break;
      case DECORATION_DIM:
        WriteToStdOut("\e[2m");
        break;
      case DECORATION_RED:
        WriteToStdOut("\e[31m\e[1m");
        break;
      case DECORATION_GREEN:
        WriteToStdOut("\e[32m");
        break;
      case DECORATION_BLUE:
        WriteToStdOut("\e[34m\e[1m");
        break;
      case DECORATION_YELLOW:
        WriteToStdOut("\e[33m\e[1m");
        break;
    }
  }

  WriteToStdOut(output.data());

  if (is_console && dec != DECORATION_NONE)
    WriteToStdOut("\e[0m");
}

#endif

void PrintShortHelp(const std::string& line) {
  size_t colon_offset = line.find(':');
  size_t first_normal = 0;
  if (colon_offset != std::string::npos) {
    OutputString("  " + line.substr(0, colon_offset), DECORATION_YELLOW);
    first_normal = colon_offset;
  }

  // See if the colon is followed by a " [" and if so, dim the contents of [ ].
  if (first_normal > 0 &&
      line.size() > first_normal + 2 &&
      line[first_normal + 1] == ' ' && line[first_normal + 2] == '[') {
    size_t begin_bracket = first_normal + 2;
    OutputString(": ");
    first_normal = line.find(']', begin_bracket);
    if (first_normal == std::string::npos)
      first_normal = line.size();
    else
      first_normal++;
    OutputString(line.substr(begin_bracket, first_normal - begin_bracket),
                 DECORATION_DIM);
  }

  OutputString(line.substr(first_normal) + "\n");
}

void PrintLongHelp(const std::string& text) {
  std::vector<std::string> lines;
  base::SplitStringDontTrim(text, '\n', &lines);

  for (size_t i = 0; i < lines.size(); i++) {
    const std::string& line = lines[i];

    // Check for a heading line.
    if (!line.empty() && line[0] != ' ') {
      // Highlight up to the colon (if any).
      size_t chars_to_highlight = line.find(':');
      if (chars_to_highlight == std::string::npos)
        chars_to_highlight = line.size();
      OutputString(line.substr(0, chars_to_highlight), DECORATION_YELLOW);
      OutputString(line.substr(chars_to_highlight) + "\n");
      continue;
    }

    // Check for a comment.
    TextDecoration dec = DECORATION_NONE;
    for (size_t char_i = 0; char_i < line.size(); char_i++) {
      if (line[char_i] == '#') {
        // Got a comment, draw dimmed.
        dec = DECORATION_DIM;
        break;
      } else if (line[char_i] != ' ') {
        break;
      }
    }

    OutputString(line + "\n", dec);
  }
}

