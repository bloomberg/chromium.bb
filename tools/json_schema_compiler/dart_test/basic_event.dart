// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Generated from namespace: basic_event

part of chrome;
/**
 * Events
 */

/// Documentation for basicEvent.
class Event_basic_event_basicEvent extends Event {
  void addListener(void callback()) => super.addListener(callback);

  void removeListener(void callback()) => super.removeListener(callback);

  bool hasListener(void callback()) => super.hasListener(callback);

  Event_basic_event_basicEvent(jsObject) : super._(jsObject, 0);
}

/**
 * Functions
 */

class API_basic_event {
  /*
   * API connection
   */
  Object _jsObject;

  /*
   * Events
   */
  Event_basic_event_basicEvent basicEvent;
  API_basic_event(this._jsObject) {
    basicEvent = new Event_basic_event_basicEvent(JS('', '#.basicEvent', this._jsObject));
  }
}
