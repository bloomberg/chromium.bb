// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PROPERTIES_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PROPERTIES_H_

#include "chrome/browser/performance_manager/observers/graph_observer.h"

namespace performance_manager {

// Helper classes for setting properties and invoking observer callbacks based
// on the value change. Note that by contract the NodeType must have a member
// function "observers()" that returns an iterable collection of
// ObserverType pointers. This is templated on the observer type to allow
// easy testing.
template <typename NodeType, typename ObserverType>
class ObservedPropertyImpl {
 public:
  // Helper class for node properties that represent measurements that are taken
  // periodically, and for which a notification should be sent every time a
  // new sample is recorded, even if identical in value to the last.
  template <typename PropertyType,
            void (ObserverType::*NotifyFunctionPtr)(NodeType*)>
  class NotifiesAlways {
   public:
    NotifiesAlways() {}
    explicit NotifiesAlways(PropertyType initial_value)
        : value_(initial_value) {}

    ~NotifiesAlways() {}

    // Sets the property and sends a notification.
    void SetAndNotify(NodeType* node, PropertyType value) {
      value_ = value;
      for (auto& observer : node->observers())
        ((observer).*(NotifyFunctionPtr))(node);
    }

    PropertyType value() const { return value_; }

   private:
    PropertyType value_;
  };

  // Helper class for node properties that represent states, for which
  // notifications should only be sent when the value of the property actually
  // changes. Calls to SetAndMaybeNotify do not notify if the provided value is
  // the same as the current value.
  template <typename PropertyType,
            void (ObserverType::*NotifyFunctionPtr)(NodeType*)>
  class NotifiesOnlyOnChanges {
   public:
    NotifiesOnlyOnChanges() {}
    explicit NotifiesOnlyOnChanges(PropertyType initial_value)
        : value_(initial_value) {}

    ~NotifiesOnlyOnChanges() {}

    // Sets the property and sends a notification if needed. Returns true if a
    // notification was sent, false otherwise.
    bool SetAndMaybeNotify(NodeType* node, PropertyType value) {
      if (value_ == value)
        return false;
      value_ = value;
      for (auto& observer : node->observers())
        ((observer).*(NotifyFunctionPtr))(node);
      return true;
    }

    PropertyType value() const { return value_; }

   private:
    PropertyType value_;
  };
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PROPERTIES_H_
