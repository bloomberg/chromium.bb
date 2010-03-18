// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper program to analyze the time that Chrome's renderers spend in system
// calls. Start Chrome like this:
//
// SECCOMP_SANDBOX_DEBUGGING=1 chrome --enable-seccomp-sandbox 2>&1 | timestats
//
// The program prints CPU time (0-100%) spent within system calls. This gives
// a general idea of where it is worthwhile to spend effort optimizing Chrome.
//
// Caveats:
//  - there currently is no way to estimate what the overhead is for running
//    inside of the sandbox vs. running without a sandbox.
//  - we currently use a very simple heuristic to decide whether a system call
//    is blocking or not. Blocking system calls should not be included in the
//    computations. But it is quite possible for the numbers to be somewhat
//    wrong, because the heuristic failed.
//  - in order to collect this data, we have to turn on sandbox debugging.
//    There is a measurable performance penalty to doing so. Production numbers
//    are strictly better than the numbers reported by this tool.
#include <set>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const int kAvgWindowSizeMs  =    500;
static const int kPeakWindowSizeMs = 2*1000;

// Class containing information on a single system call. Most notably, it
// contains the time when the system call happened, and the time that it
// took to complete.
class Datum {
  friend class Data;
 public:
  Datum(const char* name, double ms)
    : name_(name),
      ms_(ms) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    timestamp_ = tv.tv_sec*1000.0 + tv.tv_usec/1000.0;
  }
  virtual ~Datum() { }

  double operator-(const Datum& b) {
    return timestamp_ - b.timestamp_;
  }

 protected:
  const char* name_;
  double      ms_;
  double      timestamp_;
};

// Class containing data on the most recent system calls. It maintains
// sliding averages for total CPU time used, and it also maintains a peak
// CPU usage. The peak usage is usually updated slower than the average
// usage, as that makes it easier to inspect visually.
class Data {
 public:
  Data() { }
  virtual ~Data() { }

  void addData(const char* name, double ms) {
    average_.push_back(Datum(name, ms));
    peak_.push_back(Datum(name, ms));

    // Prune entries outside of the window
    std::vector<Datum>::iterator iter;
    for (iter = average_.begin();
         *average_.rbegin() - *iter > kAvgWindowSizeMs;
         ++iter) {
    }
    average_.erase(average_.begin(), iter);

    for (iter = peak_.begin();
         *peak_.rbegin() - *iter > kPeakWindowSizeMs;
         ++iter){
    }
    peak_.erase(peak_.begin(), iter);

    // Add the total usage of all system calls inside of the window
    double total = 0;
    for (iter = average_.begin(); iter != average_.end(); ++iter) {
      total += iter->ms_;
    }

    // Compute the peak CPU usage during the last window
    double peak = 0;
    double max = 0;
    std::vector<Datum>::iterator tail = peak_.begin();
    for (iter = tail; iter != peak_.end(); ++iter) {
      while (*iter - *tail > kAvgWindowSizeMs) {
        peak -= tail->ms_;
        ++tail;
      }
      peak += iter->ms_;
      if (peak > max) {
        max = peak;
      }
    }

    // Print the average CPU usage in the last window
    char buf[80];
    total *= 100.0/kAvgWindowSizeMs;
    max   *= 100.0/kAvgWindowSizeMs;
    sprintf(buf, "%6.2f%% (peak=%6.2f%%) ", total, max);

    // Animate the actual usage, displaying both average and peak values
    int len   = strlen(buf);
    int space = sizeof(buf) - len - 1;
    int mark  = (total * space + 50)/100;
    int bar   = (max   * space + 50)/100;
    for (int i = 0; i < mark; ++i) {
      buf[len++] = '*';
    }
    if (mark == bar) {
      if (bar) {
        len--;
      }
    } else {
      for (int i = 0; i < bar - mark - 1; ++i) {
        buf[len++] = ' ';
      }
    }
    buf[len++] = '|';
    while (len < static_cast<int>(sizeof(buf))) {
      buf[len++] = ' ';
    }
    strcpy(buf + len, "\r");
    fwrite(buf, len + 1, 1, stdout);
    fflush(stdout);
  }

 private:
  std::vector<Datum> average_;
  std::vector<Datum> peak_;
};
static Data data;


int main(int argc, char *argv[]) {
  char buf[80];
  bool expensive = false;
  while (fgets(buf, sizeof(buf), stdin)) {
    // Allow longer delays for expensive system calls
    if (strstr(buf, "This is an expensive system call")) {
      expensive = true;
      continue;
    }

    // Parse the string and extract the elapsed time
    const char elapsed[] = "Elapsed time: ";
    char* ms_string = strstr(buf, elapsed);
    char* endptr;
    double ms;
    char* colon = strchr(buf, ':');

    // If this string doesn't match, then it must be some other type of
    // message. Just ignore it.
    // It is quite likely that we will regularly encounter debug messages
    // that either should be parsed by a completely different tool, or
    // messages that were intended for humans to read.
    if (!ms_string ||
        ((ms = strtod(ms_string + sizeof(elapsed) - 1, &endptr)),
         endptr == ms_string) ||
        !colon) {
      continue;
    }

    // Filter out system calls that were probably just blocking
    // TODO(markus): automatically compute the cut-off for blocking calls
    if (!expensive && ms > 0.05) {
      continue;
    }
    expensive = false;

    // Extract the name of the system call
    *colon = '\000';

    // Add the data point and update the display
    data.addData(buf, ms);
  }
  puts("");
  return 0;
}
