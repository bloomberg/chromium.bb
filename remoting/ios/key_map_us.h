// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_KEY_MAP_US_H_
#define REMOTING_IOS_KEY_MAP_US_H_

// A mapping for the US keyboard on a US IPAD to Chromoting Scancodes

// This must be less than or equal to the size of
// kIsShiftRequiredUS and kKeyCodeUS.
const int kKeyboardKeyMaxUS = 126;

// Index for specific keys
const uint32_t kShiftIndex = 128;
const uint32_t kBackspaceIndex = 129;
const uint32_t kCtrlIndex = 130;
const uint32_t kAltIndex = 131;
const uint32_t kDelIndex = 132;

const BOOL kIsShiftRequiredUS[] = {
    NO,   // [0]      Numbering fields by index, not by count
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   // [10]     ENTER
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   // [20]
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   //
    NO,   // [30]
    NO,   //
    NO,   //          SPACE
    YES,  //          !
    YES,  //          "
    YES,  //          #
    YES,  //          $
    YES,  //          %
    YES,  //          &
    NO,   //          '
    YES,  // [40]     (
    YES,  //          )
    YES,  //          *
    YES,  //          +
    NO,   //          ,
    NO,   //          -
    NO,   //          .
    NO,   //          /
    NO,   //          0
    NO,   //          1
    NO,   // [50]     2
    NO,   //          3
    NO,   //          4
    NO,   //          5
    NO,   //          6
    NO,   //          7
    NO,   //          8
    NO,   //          9
    YES,  //          :
    NO,   //          ;
    YES,  // [60]     <
    NO,   //          =
    YES,  //          >
    YES,  //          ?
    YES,  //          @
    YES,  //          A
    YES,  //          B
    YES,  //          C
    YES,  //          D
    YES,  //          E
    YES,  // [70]     F
    YES,  //          G
    YES,  //          H
    YES,  //          I
    YES,  //          J
    YES,  //          K
    YES,  //          L
    YES,  //          M
    YES,  //          N
    YES,  //          O
    YES,  // [80]     P
    YES,  //          Q
    YES,  //          R
    YES,  //          S
    YES,  //          T
    YES,  //          U
    YES,  //          V
    YES,  //          W
    YES,  //          X
    YES,  //          Y
    YES,  // [90]     Z
    NO,   //          [
    NO,   //          BACKSLASH
    NO,   //          ]
    YES,  //          ^
    YES,  //          _
    NO,   //
    NO,   //          a
    NO,   //          b
    NO,   //          c
    NO,   // [100]    d
    NO,   //          e
    NO,   //          f
    NO,   //          g
    NO,   //          h
    NO,   //          i
    NO,   //          j
    NO,   //          k
    NO,   //          l
    NO,   //          m
    NO,   // [110]    n
    NO,   //          o
    NO,   //          p
    NO,   //          q
    NO,   //          r
    NO,   //          s
    NO,   //          t
    NO,   //          u
    NO,   //          v
    NO,   //          w
    NO,   // [120]    x
    NO,   //          y
    NO,   //          z
    YES,  //          {
    YES,  //          |
    YES,  //          }
    YES,  //          ~
    NO    // [127]
};

const uint32_t kKeyCodeUS[] = {
    0,         // [0]      Numbering fields by index, not by count
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0x070028,  // [10]     ENTER
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         // [20]
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         //
    0,         // [30]
    0,         //
    0x07002c,  //          SPACE
    0x07001e,  //          !
    0x070034,  //          "
    0x070020,  //          #
    0x070021,  //          $
    0x070022,  //          %
    0x070024,  //          &
    0x070034,  //          '
    0x070026,  // [40]     (
    0x070027,  //          )
    0x070025,  //          *
    0x07002e,  //          +
    0x070036,  //          ,
    0x07002d,  //          -
    0x070037,  //          .
    0x070038,  //          /
    0x070027,  //          0
    0x07001e,  //          1
    0x07001f,  // [50]     2
    0x070020,  //          3
    0x070021,  //          4
    0x070022,  //          5
    0x070023,  //          6
    0x070024,  //          7
    0x070025,  //          8
    0x070026,  //          9
    0x070033,  //          :
    0x070033,  //          ;
    0x070036,  // [60]     <
    0x07002e,  //          =
    0x070037,  //          >
    0x070038,  //          ?
    0x07001f,  //          @
    0x070004,  //          A
    0x070005,  //          B
    0x070006,  //          C
    0x070007,  //          D
    0x070008,  //          E
    0x070009,  // [70]     F
    0x07000a,  //          G
    0x07000b,  //          H
    0x07000c,  //          I
    0x07000d,  //          J
    0x07000e,  //          K
    0x07000f,  //          L
    0x070010,  //          M
    0x070011,  //          N
    0x070012,  //          O
    0x070013,  // [80]     P
    0x070014,  //          Q
    0x070015,  //          R
    0x070016,  //          S
    0x070017,  //          T
    0x070018,  //          U
    0x070019,  //          V
    0x07001a,  //          W
    0x07001b,  //          X
    0x07001c,  //          Y
    0x07001d,  // [90]     Z
    0x07002f,  //          [
    0x070031,  //          BACKSLASH
    0x070030,  //          ]
    0x070023,  //          ^
    0x07002d,  //          _
    0,         //
    0x070004,  //          a
    0x070005,  //          b
    0x070006,  //          c
    0x070007,  // [100]    d
    0x070008,  //          e
    0x070009,  //          f
    0x07000a,  //          g
    0x07000b,  //          h
    0x07000c,  //          i
    0x07000d,  //          j
    0x07000e,  //          k
    0x07000f,  //          l
    0x070010,  //          m
    0x070011,  // [110]    n
    0x070012,  //          o
    0x070013,  //          p
    0x070014,  //          q
    0x070015,  //          r
    0x070016,  //          s
    0x070017,  //          t
    0x070018,  //          u
    0x070019,  //          v
    0x07001a,  //          w
    0x07001b,  // [120]    x
    0x07001c,  //          y
    0x07001d,  //          z
    0x07002f,  //          {
    0x070031,  //          |
    0x070030,  //          }
    0x070035,  //          ~
    0,         // [127]
    0x0700e1,  //          SHIFT
    0x07002a,  //          BACKSPACE
    0x0700e0,  //          CTRL
    0x0700e2,  //          ALT
    0x07004c,  //          DEL
};

#endif  // REMOTING_IOS_KEY_MAP_US_H_
