#!/usr/bin/env vpython
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

sys.path.append(os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir)))
from core import path_util

path_util.AddTelemetryToPath()

from contrib.orderfile import orderfile


class Orderfile(unittest.TestCase):
  def setUp(self):
    # Increase failed test output to make updating easier.
    self.maxDiff = None

  def testDefaults(self):
    training = [s.NAME for s in orderfile.OrderfileStorySet(
        orderfile.OrderfileStorySet.TRAINING).RunSetStories()]
    self.assertListEqual(
        ['background:social:facebook',
         'load:media:soundcloud',
         'load:news:wikipedia',
         'browse:media:imgur',
         'browse:tech:discourse_infinite_scroll',
         'browse:news:cricbuzz',
         'load:games:lazors',
         'load:tools:drive',
         'load:search:google',
         'load:tools:stackoverflow',
         'load:news:washingtonpost',
         'load:news:reddit',
         'browse:shopping:avito',
         'load:news:cnn',
         'browse:news:qq',
         'load:search:baidu',
         'load:search:ebay',
         'long_running:tools:gmail-foreground',
         'load:media:imgur',
         'background:news:nytimes',
         'load:tools:dropbox',
         'background:search:google',
         'load:chrome:blank',
         'browse:social:tumblr_infinite_scroll',
         'load:news:qq',
         'load:search:yandex',
         'load:media:dailymotion',
         'browse:tools:maps',
         'load:games:bubbles',
         'browse:shopping:amazon',
         'browse:social:instagram',
         'background:tools:gmail',
         'load:media:youtube',
         'load:media:facebook_photos',
         'browse:media:facebook_photos',
         'browse:social:facebook',
         'browse:news:reddit',
         'load:media:google_images',
         'load:tools:weather',
         'load:social:twitter',
         'browse:news:cnn',
         'browse:media:flickr_infinite_scroll',
         'load:games:spychase',
         'load:tools:docs',
         'load:news:nytimes',
         'browse:news:washingtonpost',
         'browse:social:pinterest_infinite_scroll',
         'load:news:irctc',
         'browse:media:youtube',
         'load:search:yahoo'], training)
    testing = [s.NAME for s in orderfile.OrderfileStorySet(
        orderfile.OrderfileStorySet.TESTING).RunSetStories()]
    self.assertListEqual(
        ['browse:shopping:lazada',
         'load:tools:gmail',
         'browse:news:toi',
         'browse:chrome:omnibox',
         'browse:news:globo',
         'browse:social:facebook_infinite_scroll',
         'load:search:taobao',
         'background:media:imgur'], testing)

  def test25TrainingStories(self):
    training = [s.NAME for s in orderfile.OrderfileStorySet(
        orderfile.OrderfileStorySet.TRAINING, num_training=25).RunSetStories()]
    self.assertListEqual(
        ['background:social:facebook',
         'load:media:soundcloud',
         'load:news:wikipedia',
         'browse:media:imgur',
         'browse:tech:discourse_infinite_scroll',
         'browse:news:cricbuzz',
         'load:games:lazors',
         'load:tools:drive',
         'load:search:google',
         'load:tools:stackoverflow',
         'load:news:washingtonpost',
         'load:news:reddit',
         'browse:shopping:avito',
         'load:news:cnn',
         'browse:news:qq',
         'load:search:baidu',
         'load:search:ebay',
         'long_running:tools:gmail-foreground',
         'load:media:imgur',
         'background:news:nytimes',
         'load:tools:dropbox',
         'background:search:google',
         'load:chrome:blank',
         'browse:social:tumblr_infinite_scroll',
         'load:news:qq'],
        training)

  def testTestingStories(self):
    testing = [s.NAME for s in orderfile.OrderfileStorySet(
        orderfile.OrderfileStorySet.TESTING,
        num_training=25).RunSetStories()]
    self.assertListEqual(
        ['load:search:yandex',
         'load:media:dailymotion',
         'browse:tools:maps',
         'load:games:bubbles',
         'browse:shopping:amazon',
         'browse:social:instagram',
         'background:tools:gmail',
         'load:media:youtube'],
        testing)

  def testTestingVariationStories(self):
    testing = [s.NAME for s in orderfile.OrderfileStorySet(
        orderfile.OrderfileStorySet.TESTING, num_training=25,
        num_variations=4, test_variation=0).RunSetStories()]
    self.assertListEqual(
        ['load:search:yandex',
         'load:media:dailymotion',
         'browse:tools:maps',
         'load:games:bubbles',
         'browse:shopping:amazon',
         'browse:social:instagram',
         'background:tools:gmail',
         'load:media:youtube'],
        testing)

    testing = [s.NAME for s in orderfile.OrderfileStorySet(
        orderfile.OrderfileStorySet.TESTING, num_training=25,
        num_variations=4, test_variation=1).RunSetStories()]
    self.assertListEqual(
        ['load:media:facebook_photos',
         'browse:media:facebook_photos',
         'browse:social:facebook',
         'browse:news:reddit',
         'load:media:google_images',
         'load:tools:weather',
         'load:social:twitter',
         'browse:news:cnn'],
        testing)

    testing = [s.NAME for s in orderfile.OrderfileStorySet(
        orderfile.OrderfileStorySet.TESTING, num_training=25,
        num_variations=4, test_variation=3).RunSetStories()]
    self.assertListEqual(
        ['load:search:yahoo',
         'browse:shopping:lazada',
         'load:tools:gmail',
         'browse:news:toi',
         'browse:chrome:omnibox',
         'browse:news:globo',
         'browse:social:facebook_infinite_scroll',
         'load:search:taobao'],
        testing)


if __name__ == '__main__':
  unittest.main()
