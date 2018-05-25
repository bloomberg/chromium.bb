# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections


Tag = collections.namedtuple('Tag', ['name', 'description'])


# Below are tags that describe various aspect of rendering stories.
# A story can have multiple tags. All the tags should be nouns.

GPU_RASTERIZATION = Tag(
    'gpu_rasterization', 'Story tests performance with GPU rasterization.')
SYNC_SCROLL = Tag(
    'sync_scroll', 'Story tests rendering with synchronous scrolling.')


def _ExtractAllTags():
  all_tag_names = set()
  all_tags = []
  # Collect all the tags defined in this module. Also assert that there is no
  # duplicate tag names.
  for obj in globals().values():
    if isinstance(obj, Tag):
      all_tags.append(obj)
      assert obj.name not in all_tag_names, 'Duplicate tag name: %s' % obj.name
      all_tag_names.add(obj.name)
  return all_tags


ALL_TAGS = _ExtractAllTags()
