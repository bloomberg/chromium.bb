// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_LOADTIME_MEASUREMENT_H__
#define NET_TOOLS_FLIP_SERVER_LOADTIME_MEASUREMENT_H__
#pragma once

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <string>
#include <vector>

// Class to handle loadtime measure related urls, which all start with testing
// The in memory server has a singleton object of this class. It includes a
// html file containing javascript to go through a list of urls and upload the
// loadtime. The users can modify urls.txt to define the urls they want to
// measure and start with downloading the html file from browser.
class LoadtimeMeasurement {
 public:
  LoadtimeMeasurement(const std::string& urls_file,
                      const std::string& pageload_html_file)
      : num_urls_(0), pageload_html_file_(pageload_html_file) {
    std::string urls_string;
    read_file_to_string(urls_file.c_str(), &urls_string);
    split_string(urls_string, '\n', &urls_);
    num_urls_ = urls_.size();
  }

  // This is the entry function for all the loadtime measure related urls
  // It handles the request to html file, get_total_iteration to get number
  // of urls in the urls file, get each url, report the loadtime for
  // each url, and the test is completed.
  void ProcessRequest(const std::string& uri, std::string& output) {
    // remove "/testing/" from uri to get the action
    std::string action = uri.substr(9);
    if (pageload_html_file_.find(action) != std::string::npos) {
      read_file_to_string(pageload_html_file_.c_str(), &output);
      return;
    }
    if (action.find("get_total_iteration") == 0) {
      char buffer[16];
      snprintf(buffer, 16, "%d", num_urls_);
      output.append(buffer, strlen(buffer));
      return;
    }
    if (action.find("geturl") == 0) {
      size_t b = action.find_first_of('=');
      if (b != std::string::npos) {
        int num = atoi(action.substr(b + 1).c_str());
        if (num < num_urls_) {
          output.append(urls_[num]);
        }
      }
      return;
    }
    if (action.find("test_complete") == 0) {
      for (std::map<std::string, int>::const_iterator it = loadtimes_.begin();
           it != loadtimes_.end(); ++it) {
        LOG(INFO) << it->first << " " << it->second;
      }
      loadtimes_.clear();
      output.append("OK");
      return;
    }
    if (action.find("record_page_load") == 0) {
      std::vector<std::string> query;
      split_string(action, '?', &query);
      std::vector<std::string> params;
      split_string(query[1], '&', &params);
      std::vector<std::string> url;
      std::vector<std::string> loadtime;
      split_string(params[1], '=', &url);
      split_string(params[2], '=', &loadtime);
      loadtimes_[url[1]] = atoi(loadtime[1].c_str());
      output.append("OK");
      return;
    }
  }

 private:
  void read_file_to_string(const char* filename, std::string* output) {
    output->clear();
    int fd = open(filename, 0, "r");
    if (fd == -1)
      return;
    char buffer[4096];
    ssize_t read_status = read(fd, buffer, sizeof(buffer));
    while (read_status > 0) {
      output->append(buffer, static_cast<size_t>(read_status));
      do {
        read_status = read(fd, buffer, sizeof(buffer));
      } while (read_status <= 0 && errno == EINTR);
    }
    close(fd);
  }

  void split_string(std::string& str, char sepa,
                    std::vector<std::string>* sub_strs) {
    size_t b = 0;
    size_t e = str.find_first_of(sepa, b);
    while (e != std::string::npos && e > b) {
      sub_strs->push_back(str.substr(b, e - b));
      b = e + 1;
      e = str.find_first_of(sepa, b);
    }
    if (b < str.size()) {
      sub_strs->push_back(str.substr(b));
    }
  }

  int num_urls_;
  std::vector<std::string> urls_;
  std::map<std::string, int> loadtimes_;
  const std::string pageload_html_file_;
};

#endif  // NET_TOOLS_FLIP_SERVER_LOADTIME_MEASUREMENT_H__

