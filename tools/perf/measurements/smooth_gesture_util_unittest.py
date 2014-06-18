# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from telemetry.timeline import model as model_module
from telemetry.web_perf import timeline_interaction_record as tir_module
from measurements import smooth_gesture_util as sg_util

class SmoothGestureUtilTest(unittest.TestCase):
  def testGetAdjustedInteractionIfContainGesture(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    #      [          X          ]                   [   Y  ]
    #          [   record_1]
    #          [   record_6]
    #  [  record_2 ]          [ record_3 ]
    #  [           record_4              ]
    #                                [ record_5 ]
    renderer_main.BeginSlice('X', 'SyntheticGestureController::running', 10, 0)
    renderer_main.EndSlice(30, 30)
    renderer_main.BeginSlice('Y', 'SyntheticGestureController::running', 60, 0)
    renderer_main.EndSlice(80, 80)

    model.FinalizeImport(shift_world_to_zero=False)

    record_1 = tir_module.TimelineInteractionRecord('Gesture_included', 15, 25)
    record_2 = tir_module.TimelineInteractionRecord(
      'Gesture_overlapped_left', 5, 25)
    record_3 = tir_module.TimelineInteractionRecord(
      'Gesture_overlapped_right', 25, 35)
    record_4 = tir_module.TimelineInteractionRecord(
      'Gesture_containing', 5, 35)
    record_5 = tir_module.TimelineInteractionRecord(
      'Gesture_non_overlapped', 35, 45)
    record_6 = tir_module.TimelineInteractionRecord('Action_included', 15, 25)

    adjusted_record_1 = sg_util.GetAdjustedInteractionIfContainGesture(
      model, record_1)
    self.assertEquals(adjusted_record_1.start, 10)
    self.assertEquals(adjusted_record_1.end, 30)
    self.assertTrue(adjusted_record_1 is not record_1)

    adjusted_record_2 = sg_util.GetAdjustedInteractionIfContainGesture(
      model, record_2)
    self.assertEquals(adjusted_record_2.start, 10)
    self.assertEquals(adjusted_record_2.end, 30)

    adjusted_record_3 = sg_util.GetAdjustedInteractionIfContainGesture(
      model, record_3)
    self.assertEquals(adjusted_record_3.start, 10)
    self.assertEquals(adjusted_record_3.end, 30)

    adjusted_record_4 = sg_util.GetAdjustedInteractionIfContainGesture(
      model, record_4)
    self.assertEquals(adjusted_record_4.start, 10)
    self.assertEquals(adjusted_record_4.end, 30)

    adjusted_record_5 = sg_util.GetAdjustedInteractionIfContainGesture(
      model, record_5)
    self.assertEquals(adjusted_record_5.start, 35)
    self.assertEquals(adjusted_record_5.end, 45)
    self.assertTrue(adjusted_record_5 is not record_5)

    adjusted_record_6 = sg_util.GetAdjustedInteractionIfContainGesture(
      model, record_6)
    self.assertEquals(adjusted_record_6.start, 15)
    self.assertEquals(adjusted_record_6.end, 25)
    self.assertTrue(adjusted_record_6 is not record_6)

