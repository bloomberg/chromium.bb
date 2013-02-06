// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "salsa_experiment_runner.h"

using std::string;

bool SalsaExperimentRunner::LoadExperiment(string const &exp_string) {
  string decoded_string = Decode(exp_string);
  if (decoded_string.empty())
    return false;

  exp_ = Experiment(decoded_string);
  return exp_.valid();
}

string SalsaExperimentRunner::Decode(string const &exp_string) const {
  // Hex encoded strings always have an even length
  if (exp_string.length() % 2 != 0)
    return "";

  // Decode the string from hex, any non-hex characters invalidate it
  string decoded_string = "";
  for (string::const_iterator it = exp_string.begin();
       it != exp_string.end(); ++it) {
    int val1 = HexDigitToInt(*it);
    int val2 = HexDigitToInt(*++it);
    if (val1 < 0 || val2 < 0)
      return "";
    else
      decoded_string.push_back(static_cast<char>(val1 * 16 + val2));
  }

  return decoded_string;
}

void SalsaExperimentRunner::EndCurses() {
  endwin();
}

void SalsaExperimentRunner::StartCurses() {
  initscr();
  cbreak();
  noecho();
  refresh();
  atexit(SalsaExperimentRunner::EndCurses);
}

void SalsaExperimentRunner::run() const {
  int current_treatment = -1;
  bool success = false;
  char key_press = '0';

  SalsaExperimentRunner::StartCurses();
  WINDOW* win = newwin(19, 59, 0, 0);
  while (key_press != 'q') {
    int selected_treatment = key_press - '0';
    if (selected_treatment >= 0 && selected_treatment < exp_.Size()) {
      current_treatment = selected_treatment;
      success = exp_.ApplyTreatment(current_treatment);
    }

    box(win, 0, 0);
    mvwprintw(win, 1, 15, "  _____       _           ");
    mvwprintw(win, 2, 15, " / ____|     | |          ");
    mvwprintw(win, 3, 15, "| (___   __ _| |___  __ _ ");
    mvwprintw(win, 4, 15, " \\___ \\ / _` | / __|/ _` |");
    mvwprintw(win, 5, 15, " ____) | (_| | \\__ \\ (_| |");
    mvwprintw(win, 6, 15, "|_____/ \\__,_|_|___/\\__,_|");

    mvwprintw(win, 9, 2, "Thanks for your participation!");

    if (success)
      mvwprintw(win, 11, 2, "You are currently experiencing treatment #%d",
                current_treatment);
    else
      mvwprintw(win, 11, 2, "There was an error applying treatment #%d. "
                            "Try again.", current_treatment);
    mvwprintw(win, 12, 2, "Available treatments: 0 -> %d", (exp_.Size() - 1));

    mvwprintw(win, 14, 2, "Commands:");
    mvwprintw(win, 15, 6, "Number keys -- Change treatment");
    mvwprintw(win, 16, 6, ("q          -- Quit and restore your old settings"));
    wrefresh(win);

    key_press = getch();
  }

  if (!exp_.Reset()) {
    wclear(win);
    wprintw(win, "WARNING! Some of your setting may not have been reset to ");
    wprintw(win, "their original values.  If you experience bad touchpad ");
    wprintw(win, "behavior, you can restore them manually by logging out ");
    wprintw(win, "and logging back in.  Sorry for the inconvenience.");
    wrefresh(win);
  }

  delwin(win);
}
