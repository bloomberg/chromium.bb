// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/standard_out.h"

#include <vector>

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

#if defined(OS_WIN)
HANDLE hstdout;
WORD default_attributes;
#endif
bool is_console = false;

void EnsureInitialized() {
  if (initialized)
    return;
  initialized = true;

#if defined(OS_WIN)
  hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO info;
  is_console = !!::GetConsoleScreenBufferInfo(hstdout, &info);
  default_attributes = info.wAttributes;
#else
  is_console = isatty(fileno(stdout));
#endif
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
        fwrite("\e[2m", 1, 4, stdout);
        break;
      case DECORATION_RED:
        fwrite("\e[31m\e[1m", 1, 9, stdout);
        break;
      case DECORATION_GREEN:
        fwrite("\e[32m", 1, 5, stdout);
        break;
      case DECORATION_BLUE:
        fwrite("\e[34m\e[1m", 1, 9, stdout);
        break;
      case DECORATION_YELLOW:
        fwrite("\e[33m\e[1m", 1, 9, stdout);
        break;
    }
  }

  fwrite(output.data(), 1, output.size(), stdout);

  if (dec != DECORATION_NONE)
    fwrite("\e[0m", 1, 4, stdout);
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

