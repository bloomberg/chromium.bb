// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_KEY_MAP_US_H_
#define REMOTING_CLIENT_IOS_KEY_MAP_US_H_

// A mapping for the US keyboard on a US IPAD to Chromoting Scancodes.

const int kKeyboardKeyMaxUS = 126;

// Index for specific keys
const uint32_t kShiftIndex = 128;
const uint32_t kBackspaceIndex = 129;
const uint32_t kCtrlIndex = 130;
const uint32_t kAltIndex = 131;
const uint32_t kDelIndex = 132;

struct KeyCodeMeta {
  uint32_t code;
  BOOL needsShift;
};

const KeyCodeMeta kKeyCodeMetaUS[] = {
    {0, NO},          // [0]      Numbering fields by index, not by count
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0x070028, NO},   // [10]     ENTER
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          // [20]
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          //
    {0, NO},          // [30]
    {0, NO},          //
    {0x07002c, NO},   //          SPACE
    {0x07001e, YES},  //          !
    {0x070034, YES},  //          "
    {0x070020, YES},  //          #
    {0x070021, YES},  //          $
    {0x070022, YES},  //          %
    {0x070024, YES},  //          &
    {0x070034, NO},   //          '
    {0x070026, YES},  // [40]     (
    {0x070027, YES},  //          )
    {0x070025, YES},  //          *
    {0x07002e, YES},  //          +
    {0x070036, NO},   //          ,
    {0x07002d, NO},   //          -
    {0x070037, NO},   //          .
    {0x070038, NO},   //          /
    {0x070027, NO},   //          0
    {0x07001e, NO},   //          1
    {0x07001f, NO},   // [50]     2
    {0x070020, NO},   //          3
    {0x070021, NO},   //          4
    {0x070022, NO},   //          5
    {0x070023, NO},   //          6
    {0x070024, NO},   //          7
    {0x070025, NO},   //          8
    {0x070026, NO},   //          9
    {0x070033, YES},  //          :
    {0x070033, NO},   //          ;
    {0x070036, YES},  // [60]     <
    {0x07002e, NO},   //          =
    {0x070037, YES},  //          >
    {0x070038, YES},  //          ?
    {0x07001f, YES},  //          @
    {0x070004, YES},  //          A
    {0x070005, YES},  //          B
    {0x070006, YES},  //          C
    {0x070007, YES},  //          D
    {0x070008, YES},  //          E
    {0x070009, YES},  // [70]     F
    {0x07000a, YES},  //          G
    {0x07000b, YES},  //          H
    {0x07000c, YES},  //          I
    {0x07000d, YES},  //          J
    {0x07000e, YES},  //          K
    {0x07000f, YES},  //          L
    {0x070010, YES},  //          M
    {0x070011, YES},  //          N
    {0x070012, YES},  //          O
    {0x070013, YES},  // [80]     P
    {0x070014, YES},  //          Q
    {0x070015, YES},  //          R
    {0x070016, YES},  //          S
    {0x070017, YES},  //          T
    {0x070018, YES},  //          U
    {0x070019, YES},  //          V
    {0x07001a, YES},  //          W
    {0x07001b, YES},  //          X
    {0x07001c, YES},  //          Y
    {0x07001d, YES},  // [90]     Z
    {0x07002f, NO},   //          [
    {0x070031, NO},   //          BACKSLASH
    {0x070030, NO},   //          ]
    {0x070023, YES},  //          ^
    {0x07002d, YES},  //          _
    {0, NO},          //
    {0x070004, NO},   //          a
    {0x070005, NO},   //          b
    {0x070006, NO},   //          c
    {0x070007, NO},   // [100]    d
    {0x070008, NO},   //          e
    {0x070009, NO},   //          f
    {0x07000a, NO},   //          g
    {0x07000b, NO},   //          h
    {0x07000c, NO},   //          i
    {0x07000d, NO},   //          j
    {0x07000e, NO},   //          k
    {0x07000f, NO},   //          l
    {0x070010, NO},   //          m
    {0x070011, NO},   // [110]    n
    {0x070012, NO},   //          o
    {0x070013, NO},   //          p
    {0x070014, NO},   //          q
    {0x070015, NO},   //          r
    {0x070016, NO},   //          s
    {0x070017, NO},   //          t
    {0x070018, NO},   //          u
    {0x070019, NO},   //          v
    {0x07001a, NO},   //          w
    {0x07001b, NO},   // [120]    x
    {0x07001c, NO},   //          y
    {0x07001d, NO},   //          z
    {0x07002f, YES},  //          {
    {0x070031, YES},  //          |
    {0x070030, YES},  //          }
    {0x070035, YES},  //          ~
    {0, NO},          // [127]
    {0x0700e1, NO},   //          SHIFT
    {0x07002a, NO},   //          BACKSPACE
    {0x0700e0, NO},   //          CTRL
    {0x0700e2, NO},   //          ALT
    {0x07004c, NO}    //          DEL
};

#endif  // REMOTING_CLIENT_IOS_KEY_MAP_US_H_
