// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Generated from namespace: basic_function

part of chrome;
/**
 * Functions
 */

class API_basic_function {
  /*
   * API connection
   */
  Object _jsObject;

  /*
   * Functions
   */
  /// Documentation for the basic function, foo().
  void foo() => JS('void', '#.foo()', this._jsObject);

  /// Documentation for the basic static function, staticFoo().
  void staticFoo() => JS('void', '#.staticFoo()', this._jsObject);

  API_basic_function(this._jsObject) {
  }
}
