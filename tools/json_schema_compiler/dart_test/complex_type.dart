// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Generated from namespace: complex_type

part of chrome;

/**
 * Types
 */

class Complex_typeComplexType extends ChromeObject {
  /*
   * Public constructor
   */
  Complex_typeComplexType({String s, int i, int l, double d, Complex_typeComplexType c}) {
    if (?s)
      this.s = s;
    if (?i)
      this.i = i;
    if (?l)
      this.l = l;
    if (?d)
      this.d = d;
    if (?c)
      this.c = c;
  }

  /*
   * Private constructor
   */
  Complex_typeComplexType._proxy(_jsObject) : super._proxy(_jsObject);

  /*
   * Public accessors
   */
  /// Documentation for the String s.
  String get s => JS('String', '#.s', this._jsObject);

  void set s(String s) {
    JS('void', '#.s = #', this._jsObject, s);
  }

  /// Documentation for the int i.
  int get i => JS('int', '#.i', this._jsObject);

  void set i(int i) {
    JS('void', '#.i = #', this._jsObject, i);
  }

  /// Documentation for the long l.
  int get l => JS('int', '#.l', this._jsObject);

  void set l(int l) {
    JS('void', '#.l = #', this._jsObject, l);
  }

  /// Documentation for the double d.
  double get d => JS('double', '#.d', this._jsObject);

  void set d(double d) {
    JS('void', '#.d = #', this._jsObject, d);
  }

  /// Documentation for the ComplexType c.
  Complex_typeComplexType get c => new Complex_typeComplexType._proxy(JS('', '#.c', this._jsObject));

  void set c(Complex_typeComplexType c) {
    JS('void', '#.c = #', this._jsObject, convertArgument(c));
  }

}

/**
 * Functions
 */

class API_complex_type {
  /*
   * API connection
   */
  Object _jsObject;
  API_complex_type(this._jsObject) {
  }
}
