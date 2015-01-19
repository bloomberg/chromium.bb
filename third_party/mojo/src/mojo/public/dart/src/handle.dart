// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of core;

class _MojoHandleNatives {
  static int register(MojoEventStream eventStream) native "MojoHandle_Register";
  static int close(int handle) native "MojoHandle_Close";
  static List wait(int handle, int signals, int deadline)
      native "MojoHandle_Wait";
  static List waitMany(
      List<int> handles, List<int> signals, int deadline)
      native "MojoHandle_WaitMany";
}


class MojoHandle {
  static const int INVALID = 0;
  static const int DEADLINE_INDEFINITE = -1;

  int h;

  MojoHandle(this.h);

  MojoResult close() {
    int result = _MojoHandleNatives.close(h);
    h = INVALID;
    return new MojoResult(result);
  }

  MojoWaitResult wait(int signals, int deadline) {
    List result = _MojoHandleNatives.wait(h, signals, deadline);
    return new MojoWaitResult(new MojoResult(result[0]), result[1]);
  }

  bool _ready(MojoHandleSignals signal) {
    MojoWaitResult mwr = wait(signal.value, 0);
    switch (mwr.result) {
      case MojoResult.OK:
        return true;
      case MojoResult.DEADLINE_EXCEEDED:
      case MojoResult.CANCELLED:
      case MojoResult.INVALID_ARGUMENT:
      case MojoResult.FAILED_PRECONDITION:
        return false;
      default:
        // Should be unreachable.
        throw "Unexpected result $res for wait on $h";
    }
  }

  bool get readyRead => _ready(MojoHandleSignals.READABLE);
  bool get readyWrite => _ready(MojoHandleSignals.WRITABLE);

  static MojoWaitManyResult waitMany(
      List<int> handles, List<int> signals, int deadline) {
    List result = _MojoHandleNatives.waitMany(handles, signals, deadline);
    return new MojoWaitManyResult(
        new MojoResult(result[0]), result[1], result[2]);
  }

  static MojoResult register(MojoEventStream eventStream) {
    return new MojoResult(_MojoHandleNatives.register(eventStream));
  }

  bool get isValid => (h != INVALID);

  String toString() => "$h";

  bool operator ==(MojoHandle other) {
    return h == other.h;
  }
}
