// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <math.h>
#include <vector>
#include <utility>

#include <base/logging.h>
#include <gtest/gtest.h>

#include "gestures/include/gestures.h"
#include "gestures/include/palm_classifying_filter_interpreter.h"

using std::deque;
using std::make_pair;
using std::pair;
using std::vector;

namespace gestures {

class PalmClassifyingFilterInterpreterTest : public ::testing::Test {};

class PalmClassifyingFilterInterpreterTestInterpreter : public Interpreter {
 public:
  PalmClassifyingFilterInterpreterTestInterpreter()
      : expected_flags_(0) {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate, stime_t* timeout) {
    if (hwstate->finger_cnt > 0) {
      EXPECT_EQ(expected_flags_, hwstate->fingers[0].flags);
    }
    return NULL;
  }

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout) {
    EXPECT_TRUE(false);
    return NULL;
  }

  virtual void SetHardwareProperties(const HardwareProperties& hw_props) {};

  unsigned expected_flags_;
};

TEST(PalmClassifyingFilterInterpreterTest, PalmTest) {
  PalmClassifyingFilterInterpreter pci(NULL, NULL, NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    1000,  // right edge
    1000,  // bottom edge
    500,  // x pixels/TP width
    500,  // y pixels/TP height
    96,  // x screen DPI
    96,  // y screen DPI
    2,  // max fingers
    5,  // max touch
    0,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };
  pci.SetHardwareProperties(hwprops);

  const int kBig = pci.palm_pressure_.val_ + 1;  // big (palm) pressure
  const int kSml = pci.palm_pressure_.val_ - 1;  // low pressure

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, kSml, 0, 600, 500, 1, 0},
    {0, 0, 0, 0, kSml, 0, 500, 500, 2, 0},

    {0, 0, 0, 0, kSml, 0, 600, 500, 1, 0},
    {0, 0, 0, 0, kBig, 0, 500, 500, 2, 0},

    {0, 0, 0, 0, kSml, 0, 600, 500, 1, 0},
    {0, 0, 0, 0, kSml, 0, 500, 500, 2, 0},

    {0, 0, 0, 0, kSml, 0, 600, 500, 3, 0},
    {0, 0, 0, 0, kBig, 0, 500, 500, 4, 0},

    {0, 0, 0, 0, kSml, 0, 600, 500, 3, 0},
    {0, 0, 0, 0, kSml, 0, 500, 500, 4, 0}
  };
  HardwareState hardware_state[] = {
    // time, buttons, finger count, touch count, finger states pointer
    { 200000, 0, 2, 2, &finger_states[0] },
    { 200001, 0, 2, 2, &finger_states[2] },
    { 200002, 0, 2, 2, &finger_states[4] },
    { 200003, 0, 2, 2, &finger_states[6] },
    { 200004, 0, 2, 2, &finger_states[8] },
  };

  for (size_t i = 0; i < 5; ++i) {
    pci.SyncInterpret(&hardware_state[i], NULL);
    switch (i) {
      case 0:
        EXPECT_TRUE(SetContainsValue(pci.pointing_, 1));
        EXPECT_FALSE(SetContainsValue(pci.palm_, 1));
        EXPECT_TRUE(SetContainsValue(pci.pointing_, 2));
        EXPECT_FALSE(SetContainsValue(pci.palm_, 2));
        break;
      case 1:  // fallthrough
      case 2:
        EXPECT_TRUE(SetContainsValue(pci.pointing_, 1));
        EXPECT_FALSE(SetContainsValue(pci.palm_, 1));
        EXPECT_FALSE(SetContainsValue(pci.pointing_, 2));
        EXPECT_TRUE(SetContainsValue(pci.palm_, 2));
        break;
      case 3:  // fallthrough
      case 4:
        EXPECT_TRUE(SetContainsValue(pci.pointing_, 3)) << "i=" << i;
        EXPECT_FALSE(SetContainsValue(pci.palm_, 3));
        EXPECT_FALSE(SetContainsValue(pci.pointing_, 4));
        EXPECT_TRUE(SetContainsValue(pci.palm_, 4));
        break;
    }
  }
}

TEST(PalmClassifyingFilterInterpreterTest, StationaryPalmTest) {
  PalmClassifyingFilterInterpreter pci(NULL, NULL, NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    100,  // right edge
    100,  // bottom edge
    1,  // x pixels/TP width
    1,  // y pixels/TP height
    1,  // x screen DPI
    1,  // y screen DPI
    5,  // max fingers
    5,  // max touch
    0,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };
  pci.SetHardwareProperties(hwprops);

  const int kPr = pci.palm_pressure_.val_ / 2;

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID, flags
    // Palm is id 1, finger id 2
    {0, 0, 0, 0, kPr, 0,  0, 40, 1, 0},
    {0, 0, 0, 0, kPr, 0, 30, 37, 2, 0},

    {0, 0, 0, 0, kPr, 0,  0, 40, 1, 0},
    {0, 0, 0, 0, kPr, 0, 30, 40, 2, 0},

    {0, 0, 0, 0, kPr, 0,  0, 40, 1, 0},
    {0, 0, 0, 0, kPr, 0, 30, 43, 2, 0},
  };
  HardwareState hardware_state[] = {
    // time, buttons, finger count, touch count, finger states pointer
    { 0.00, 0, 1, 1, &finger_states[0] },
    { 4.00, 0, 2, 2, &finger_states[0] },
    { 5.00, 0, 2, 2, &finger_states[2] },
    { 5.01, 0, 2, 2, &finger_states[4] },
  };

  for (size_t i = 0; i < arraysize(hardware_state); ++i) {
    pci.SyncInterpret(&hardware_state[i], NULL);
    if (i > 0) {
      // We expect after the second input frame is processed that the palm
      // is classified
      EXPECT_FALSE(SetContainsValue(pci.pointing_, 1));
      EXPECT_TRUE(SetContainsValue(pci.palm_, 1));
    }
    if (hardware_state[i].finger_cnt > 1)
      EXPECT_TRUE(SetContainsValue(pci.pointing_, 2)) << "i=" << i;
  }
}

TEST(PalmClassifyingFilterInterpreterTest, PalmAtEdgeTest) {
  PalmClassifyingFilterInterpreterTestInterpreter* base_interpreter = NULL;
  scoped_ptr<PalmClassifyingFilterInterpreter> pci(
      new PalmClassifyingFilterInterpreter(NULL, NULL, NULL));
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    100,  // right edge
    100,  // bottom edge
    1,  // x pixels/mm
    1,  // y pixels/mm
    1,  // x screen px/mm
    1,  // y screen px/mm
    5,  // max fingers
    5,  // max touch
    0,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };

  const float kBig = pci->palm_pressure_.val_ + 1.0;  // palm pressure
  const float kSml = pci->palm_pressure_.val_ - 1.0;  // small, low pressure
  const float kMid = pci->palm_pressure_.val_ / 2.0;
  const float kMidWidth =
      (pci->palm_edge_min_width_.val_ + pci->palm_edge_width_.val_) / 2.0;

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    // small contact movement in edge
    {0, 0, 0, 0, kSml, 0, 1, 40, 1, 0},
    {0, 0, 0, 0, kSml, 0, 1, 50, 1, 0},
    // small contact movement in middle
    {0, 0, 0, 0, kSml, 0, 50, 40, 1, 0},
    {0, 0, 0, 0, kSml, 0, 50, 50, 1, 0},
    // large contact movment in middle
    {0, 0, 0, 0, kBig, 0, 50, 40, 1, 0},
    {0, 0, 0, 0, kBig, 0, 50, 50, 1, 0},
    // under mid-pressure contact move at mid-width
    {0, 0, 0, 0, kMid - 1.0, 0, kMidWidth, 40, 1, 0},
    {0, 0, 0, 0, kMid - 1.0, 0, kMidWidth, 50, 1, 0},
    // over mid-pressure contact move at mid-width
    {0, 0, 0, 0, kMid + 1.0, 0, kMidWidth, 40, 1, 0},
    {0, 0, 0, 0, kMid + 1.0, 0, kMidWidth, 50, 1, 0},
  };
  HardwareState hardware_state[] = {
    // time, buttons, finger count, touch count, finger states pointer
    // slow movement at edge
    { 0.0, 0, 1, 1, &finger_states[0] },
    { 1.0, 0, 1, 1, &finger_states[1] },
    // slow small contact movement in middle
    { 0.0, 0, 1, 1, &finger_states[2] },
    { 1.0, 0, 1, 1, &finger_states[3] },
    // slow large contact movement in middle
    { 0.0, 0, 1, 1, &finger_states[4] },
    { 1.0, 0, 1, 1, &finger_states[5] },
    // under mid-pressure at mid-width
    { 0.0, 0, 1, 1, &finger_states[6] },
    { 1.0, 0, 1, 1, &finger_states[7] },
    // over mid-pressure at mid-width
    { 0.0, 0, 1, 1, &finger_states[8] },
    { 1.0, 0, 1, 1, &finger_states[9] },
  };

  for (size_t i = 0; i < arraysize(hardware_state); ++i) {
    if ((i % 2) == 0) {
      base_interpreter = new PalmClassifyingFilterInterpreterTestInterpreter;
      pci.reset(new PalmClassifyingFilterInterpreter(NULL, base_interpreter,
                                                     NULL));
      pci->SetHardwareProperties(hwprops);
    }
    switch (i) {
      case 2:  // fallthough
      case 3:
      case 6:
      case 7:
        base_interpreter->expected_flags_ = 0;
        break;
      case 0:  // fallthrough
      case 1:
      case 4:
      case 5:
      case 8:
      case 9:
        base_interpreter->expected_flags_ = GESTURES_FINGER_PALM;
        break;
      default:
        ADD_FAILURE() << "Should be unreached.";
        break;
    }
    LOG(INFO) << "iteration i = " << i;
    pci->SyncInterpret(&hardware_state[i], NULL);
  }
}

struct PalmReevaluateTestInputs {
  stime_t now_;
  float x_, y_, pressure_;
};

// This tests that a palm that doesn't start out as a palm, but actually is,
// and can be classified as one shortly after it starts, doesn't cause motion.
TEST(PalmClassifyingFilterInterpreterTest, PalmReevaluateTest) {
  PalmClassifyingFilterInterpreter pci(NULL, NULL, NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    106.666672,  // right edge
    68.000000,  // bottom edge
    1,  // pixels/TP width
    1,  // pixels/TP height
    25.4,  // screen DPI x
    25.4,  // screen DPI y
    15,  // max fingers
    5,  // max touch
    0,  // t5r2
    0,  // semi-mt
    true  // is button pad
  };

  pci.SetHardwareProperties(hwprops);

  PalmReevaluateTestInputs inputs[] = {
    { 5.8174, 10.25, 46.10,  15.36 },
    { 5.8277, 10.25, 46.10,  21.18 },
    { 5.8377, 10.25, 46.10,  19.24 },
    { 5.8479,  9.91, 45.90,  15.36 },
    { 5.8578,  9.08, 44.90,  19.24 },
    { 5.8677,  9.08, 44.90,  28.94 },
    { 5.8777,  8.66, 44.70,  61.93 },
    { 5.8879,  8.41, 44.60,  58.04 },
    { 5.8973,  8.08, 44.20,  73.57 },
    { 5.9073,  7.83, 44.20,  87.15 },
    { 5.9171,  7.50, 44.40,  89.09 },
    { 5.9271,  7.25, 44.20,  87.15 },
    { 5.9369,  7.00, 44.40,  83.27 },
    { 5.9466,  6.33, 44.60,  89.09 },
    { 5.9568,  6.00, 44.70,  85.21 },
    { 5.9666,  5.75, 44.70,  87.15 },
    { 5.9763,  5.41, 44.79,  75.51 },
    { 5.9862,  5.08, 45.20,  75.51 },
    { 5.9962,  4.50, 45.50,  75.51 },
    { 6.0062,  4.41, 45.79,  81.33 },
    { 6.0160,  4.08, 46.40,  77.45 },
    { 6.0263,  3.83, 46.90,  91.03 },
    { 6.0363,  3.58, 47.50,  98.79 },
    { 6.0459,  3.25, 47.90, 114.32 },
    { 6.0560,  3.25, 47.90, 149.24 },
    { 6.0663,  3.33, 48.10, 170.59 },
    { 6.0765,  3.50, 48.29, 180.29 },
    { 6.0866,  3.66, 48.40, 188.05 },
    { 6.0967,  3.83, 48.50, 188.05 },
    { 6.1067,  3.91, 48.79, 182.23 },
    { 6.1168,  4.00, 48.79, 180.29 },
    { 6.1269,  4.08, 48.79, 180.29 },
    { 6.1370,  4.16, 48.90, 176.41 },
    { 6.1473,  4.25, 49.20, 162.82 },
    { 6.1572,  4.58, 51.00, 135.66 },
    { 6.1669,  4.66, 51.00, 114.32 },
    { 6.1767,  4.66, 51.40,  73.57 },
    { 6.1868,  4.66, 52.00,  40.58 },
    { 6.1970,  4.66, 52.40,  21.18 },
    { 6.2068,  6.25, 51.90,  13.42 },
  };
  for (size_t i = 0; i < arraysize(inputs); i++) {
    FingerState fs =
        { 0, 0, 0, 0, inputs[i].pressure_, 0.0,
          inputs[i].x_, inputs[i].y_, 1, 0 };
    HardwareState hs = { inputs[i].now_, 0, 1, 1, &fs };

    stime_t timeout = -1.0;
    pci.SyncInterpret(&hs, &timeout);
    // Allow movement at first:
    stime_t age = inputs[i].now_ - inputs[0].now_;
    if (age < pci.palm_eval_timeout_.val_)
      continue;
    EXPECT_FALSE(SetContainsValue(pci.pointing_, 1));
  }
}

}  // namespace gestures
