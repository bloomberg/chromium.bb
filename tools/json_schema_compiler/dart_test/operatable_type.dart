// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Generated from namespace: operatable_type

part of chrome;

/**
 * Types
 */

class Operatable_typeOperatableType extends ChromeObject {
  /*
   * Private constructor
   */
  Operatable_typeOperatableType._proxy(_jsObject) : super._proxy(_jsObject);

  /*
   * Public accessors
   */
  /// Documentation for the String t.
  String get t => JS('String', '#.t', this._jsObject);

  void set t(String t) {
    JS('void', '#.t = #', this._jsObject, t);
  }


  /*
   * Methods
   */
  /// Documentation for staticFoo.
  void staticFoo() => JS('void', '#.staticFoo()', this._jsObject);

}

/**
 * Functions
 */

class API_operatable_type {
  /*
   * API connection
   */
  Object _jsObject;
  API_operatable_type(this._jsObject) {
  }
}
