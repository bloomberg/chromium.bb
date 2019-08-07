// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_URL_LIST_MANAGER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_URL_LIST_MANAGER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/observer_list.h"

class GURL;

// This class manages lists of blocked URLs in order to drive UI surfaces.
// Currently it is used by the redirect / popup blocked UIs.
//
// TODO(csharrison): Currently this object is composed within the framebust /
// popup tab helpers. Eventually those objects could be replaced almost entirely
// by shared logic here.
class UrlListManager {
 public:
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void BlockedUrlAdded(int32_t id, const GURL& url) = 0;
  };

  UrlListManager();
  ~UrlListManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyObservers(int32_t id, const GURL& url);

 private:
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(UrlListManager);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_URL_LIST_MANAGER_H_
