// Copyright (c) 2014, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Generated from namespace: comments

part of chrome;
/**
 * Functions
 */

class API_comments {
  /*
   * API connection
   */
  Object _jsObject;

  /*
   * Functions
   */
  /// <p>There's a blank line at the start of this comment.</p><p>Documentation
  /// for basicFunction. BasicFunction() is a great function. There is a newline
  /// after this.</p><p>It works like so:        +-----+        |     |     +--+
  ///       |     |     |  |        +-----+ --> +--+</p><p>Some other stuff here.
  ///    This paragraph starts with whitespace.    Overall, its a great function.
  /// There's also a blank line at the end of this comment.</p>
  void basicFunction() => JS('void', '#.basicFunction()', this._jsObject);

  API_comments(this._jsObject) {
  }
}
