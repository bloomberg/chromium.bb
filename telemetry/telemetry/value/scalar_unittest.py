# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry import story
from telemetry import page as page_module
from telemetry.value import improvement_direction
from telemetry.value import none_values
from telemetry.value import scalar


class TestBase(unittest.TestCase):
  def setUp(self):
    story_set = story.StorySet(base_dir=os.path.dirname(__file__))
    story_set.AddStory(
        page_module.Page('http://www.bar.com/', story_set, story_set.base_dir,
                         name='http://www.bar.com/'))
    story_set.AddStory(
        page_module.Page('http://www.baz.com/', story_set, story_set.base_dir,
                         name='http://www.baz.com/'))
    story_set.AddStory(
        page_module.Page('http://www.foo.com/', story_set, story_set.base_dir,
                         name='http://www.foo.com/'))
    self.story_set = story_set

  @property
  def pages(self):
    return self.story_set.stories

class ValueTest(TestBase):
  def testRepr(self):
    page0 = self.pages[0]
    v = scalar.ScalarValue(page0, 'x', 'unit', 3, important=True,
                           description='desc',
                           improvement_direction=improvement_direction.DOWN)

    expected = ('ScalarValue(http://www.bar.com/, x, unit, 3, important=True, '
                'description=desc, improvement_direction=down)')

    self.assertEquals(expected, str(v))

  def testScalarSamePageMerging(self):
    page0 = self.pages[0]
    v0 = scalar.ScalarValue(page0, 'x', 'unit', 1,
                            description='important metric',
                            improvement_direction=improvement_direction.UP)
    v1 = scalar.ScalarValue(page0, 'x', 'unit', 2,
                            description='important metric',
                            improvement_direction=improvement_direction.UP)
    self.assertTrue(v1.IsMergableWith(v0))

    vM = scalar.ScalarValue.MergeLikeValuesFromSamePage([v0, v1])
    self.assertEquals(page0, vM.page)
    self.assertEquals('x', vM.name)
    self.assertEquals('unit', vM.units)
    self.assertEquals('important metric', vM.description)
    self.assertEquals(True, vM.important)
    self.assertEquals([1, 2], vM.values)
    self.assertEquals(improvement_direction.UP, vM.improvement_direction)

  def testScalarDifferentPageMerging(self):
    page0 = self.pages[0]
    page1 = self.pages[1]
    v0 = scalar.ScalarValue(page0, 'x', 'unit', 1,
                            description='important metric',
                            improvement_direction=improvement_direction.UP)
    v1 = scalar.ScalarValue(page1, 'x', 'unit', 2,
                            description='important metric',
                            improvement_direction=improvement_direction.UP)

    vM = scalar.ScalarValue.MergeLikeValuesFromDifferentPages([v0, v1])
    self.assertEquals(None, vM.page)
    self.assertEquals('x', vM.name)
    self.assertEquals('unit', vM.units)
    self.assertEquals('important metric', vM.description)
    self.assertEquals(True, vM.important)
    self.assertEquals([1, 2], vM.values)
    self.assertEquals(improvement_direction.UP, vM.improvement_direction)

  def testScalarWithNoneValueMerging(self):
    page0 = self.pages[0]
    v0 = scalar.ScalarValue(
        page0, 'x', 'unit', 1, improvement_direction=improvement_direction.DOWN)
    v1 = scalar.ScalarValue(page0, 'x', 'unit', None, none_value_reason='n',
                            improvement_direction=improvement_direction.DOWN)
    self.assertTrue(v1.IsMergableWith(v0))

    vM = scalar.ScalarValue.MergeLikeValuesFromSamePage([v0, v1])
    self.assertEquals(None, vM.values)
    expected_none_value_reason = (
        'Merging values containing a None value results in a None value. '
        'None values: [ScalarValue(http://www.bar.com/, x, unit, None, '
        'important=True, description=None, improvement_direction=down)]')
    self.assertEquals(expected_none_value_reason, vM.none_value_reason)

  def testScalarWithNoneValueMustHaveNoneReason(self):
    page0 = self.pages[0]
    self.assertRaises(none_values.NoneValueMissingReason,
                      lambda: scalar.ScalarValue(
                          page0, 'x', 'unit', None,
                          improvement_direction=improvement_direction.UP))

  def testScalarWithNoneReasonMustHaveNoneValue(self):
    page0 = self.pages[0]
    self.assertRaises(none_values.ValueMustHaveNoneValue,
                      lambda: scalar.ScalarValue(
                          page0, 'x', 'unit', 1, none_value_reason='n',
                          improvement_direction=improvement_direction.UP))

  def testAsDict(self):
    v = scalar.ScalarValue(None, 'x', 'unit', 42, important=False,
                           improvement_direction=improvement_direction.DOWN)
    d = v.AsDict()

    self.assertEquals(d, {
        'value': 42,
        'important': False,
        'improvement_direction': 'down',
        'name': 'x',
        'type': 'scalar',
        'units': 'unit'
    })

  def testNoneValueAsDict(self):
    v = scalar.ScalarValue(None, 'x', 'unit', None, important=False,
                           none_value_reason='n',
                           improvement_direction=improvement_direction.DOWN)
    d = v.AsDict()

    self.assertEquals(d, {
        'value': None,
        'none_value_reason': 'n',
        'important': False,
        'improvement_direction': 'down',
        'name': 'x',
        'type': 'scalar',
        'units': 'unit'
    })
