#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Check that explain_binary_size_delta seems to work."""

import cStringIO
import sys
import unittest

import explain_binary_size_delta


class ExplainBinarySizeDeltaTest(unittest.TestCase):

  def testCompare(self):
    # List entries have form: symbol_name, symbol_type, symbol_size, file_path
    symbol_list1 = (
      # File with one symbol, left as-is.
      ( 'unchanged', 't', 1000, '/file_unchanged' ),
      # File with one symbol, changed.
      ( 'changed', 't', 1000, '/file_all_changed' ),
      # File with one symbol, deleted.
      ( 'removed', 't', 1000, '/file_all_deleted' ),
      # File with two symbols, one unchanged, one changed, same bucket
      ( 'unchanged', 't', 1000, '/file_pair_unchanged_changed' ),
      ( 'changed', 't', 1000, '/file_pair_unchanged_changed' ),
      # File with two symbols, one unchanged, one deleted, same bucket
      ( 'unchanged', 't', 1000, '/file_pair_unchanged_removed' ),
      ( 'removed', 't', 1000, '/file_pair_unchanged_removed' ),
      # File with two symbols, one unchanged, one added, same bucket
      ( 'unchanged', 't', 1000, '/file_pair_unchanged_added' ),
      # File with two symbols, one unchanged, one changed, different bucket
      ( 'unchanged', 't', 1000, '/file_pair_unchanged_diffbuck_changed' ),
      ( 'changed', '@', 1000, '/file_pair_unchanged_diffbuck_changed' ),
      # File with two symbols, one unchanged, one deleted, different bucket
      ( 'unchanged', 't', 1000, '/file_pair_unchanged_diffbuck_removed' ),
      ( 'removed', '@', 1000, '/file_pair_unchanged_diffbuck_removed' ),
      # File with two symbols, one unchanged, one added, different bucket
      ( 'unchanged', 't', 1000, '/file_pair_unchanged_diffbuck_added' ),
      # File with four symbols, one added, one removed,
      # one changed, one unchanged
      ( 'size_changed', 't', 1000, '/file_tetra' ),
      ( 'removed', 't', 1000, '/file_tetra' ),
      ( 'unchanged', 't', 1000, '/file_tetra' ),
    )

    symbol_list2 = (
      # File with one symbol, left as-is.
      ( 'unchanged', 't', 1000, '/file_unchanged' ),
      # File with one symbol, changed.
      ( 'changed', 't', 2000, '/file_all_changed' ),
      # File with two symbols, one unchanged, one changed, same bucket
      ( 'unchanged', 't', 1000, '/file_pair_unchanged_changed' ),
      ( 'changed', 't', 2000, '/file_pair_unchanged_changed' ),
      # File with two symbols, one unchanged, one deleted, same bucket
      ( 'unchanged', 't', 1000, '/file_pair_unchanged_removed' ),
      # File with two symbols, one unchanged, one added, same bucket
      ( 'unchanged', 't', 1000, '/file_pair_unchanged_added' ),
      ( 'added', 't', 1000, '/file_pair_unchanged_added' ),
      # File with two symbols, one unchanged, one changed, different bucket
      ( 'unchanged', 't', 1000, '/file_pair_unchanged_diffbuck_changed' ),
      ( 'changed', '@', 2000, '/file_pair_unchanged_diffbuck_changed' ),
      # File with two symbols, one unchanged, one deleted, different bucket
      ( 'unchanged', 't', 1000, '/file_pair_unchanged_diffbuck_removed' ),
      # File with two symbols, one unchanged, one added, different bucket
      ( 'unchanged', 't', 1000, '/file_pair_unchanged_diffbuck_added' ),
      ( 'added', '@', 1000, '/file_pair_unchanged_diffbuck_added' ),
      # File with four symbols, one added, one removed,
      # one changed, one unchanged
      ( 'size_changed', 't', 2000, '/file_tetra' ),
      ( 'unchanged', 't', 1000, '/file_tetra' ),
      ( 'added', 't', 1000, '/file_tetra' ),
      # New file with one symbol added
      ( 'added', 't', 1000, '/file_new' ),
    )

    # Here we go
    (added, removed, changed, unchanged) = \
        explain_binary_size_delta.Compare(symbol_list1, symbol_list2)

    # File with one symbol, left as-is.
    assert ('/file_unchanged', 't', 'unchanged', 1000, 1000) in unchanged
    # File with one symbol, changed.
    assert ('/file_all_changed', 't', 'changed', 1000, 2000) in changed
    # File with one symbol, deleted.
    assert ('/file_all_deleted', 't', 'removed', 1000, None) in removed
    # New file with one symbol added
    assert ('/file_new', 't', 'added', None, 1000) in added
    # File with two symbols, one unchanged, one changed, same bucket
    assert ('/file_pair_unchanged_changed',
            't', 'unchanged', 1000, 1000) in unchanged
    assert ('/file_pair_unchanged_changed',
            't', 'changed', 1000, 2000) in changed
    # File with two symbols, one unchanged, one removed, same bucket
    assert ('/file_pair_unchanged_removed',
            't', 'unchanged', 1000, 1000) in unchanged
    assert ('/file_pair_unchanged_removed',
            't', 'removed', 1000, None) in removed
    # File with two symbols, one unchanged, one added, same bucket
    assert ('/file_pair_unchanged_added',
            't', 'unchanged', 1000, 1000) in unchanged
    assert ('/file_pair_unchanged_added',
            't', 'added', None, 1000) in added
    # File with two symbols, one unchanged, one changed, different bucket
    assert ('/file_pair_unchanged_diffbuck_changed',
            't', 'unchanged', 1000, 1000) in unchanged
    assert ('/file_pair_unchanged_diffbuck_changed',
            '@', 'changed', 1000, 2000) in changed
    # File with two symbols, one unchanged, one removed, different bucket
    assert ('/file_pair_unchanged_diffbuck_removed',
            't', 'unchanged', 1000, 1000) in unchanged
    assert ('/file_pair_unchanged_diffbuck_removed',
            '@', 'removed', 1000, None) in removed
    # File with two symbols, one unchanged, one added, different bucket
    assert ('/file_pair_unchanged_diffbuck_added',
            't', 'unchanged', 1000, 1000) in unchanged
    assert ('/file_pair_unchanged_diffbuck_added',
            '@', 'added', None, 1000) in added
    # File with four symbols, one added, one removed, one changed, one unchanged
    assert ('/file_tetra', 't', 'size_changed', 1000, 2000) in changed
    assert ('/file_tetra', 't', 'unchanged', 1000, 1000) in unchanged
    assert ('/file_tetra', 't', 'added', None, 1000) in added
    assert ('/file_tetra', 't', 'removed', 1000, None) in removed

    # Now check final stats.
    orig_stdout = sys.stdout
    output_collector = cStringIO.StringIO()
    sys.stdout = output_collector
    try:
      explain_binary_size_delta.CrunchStats(added, removed, changed,
                                            unchanged, True, True)
    finally:
      sys.stdout = orig_stdout
    result = output_collector.getvalue()

    expected_output = """\
Total change: +4000 bytes
=========================
  4 added, totalling +4000 bytes across 4 sources
  4 removed, totalling -4000 bytes across 4 sources
  4 grown, for a net change of +4000 bytes \
(4000 bytes before, 8000 bytes after) across 4 sources
  8 unchanged, totalling 8000 bytes
Source stats:
  11 sources encountered.
  1 completely new.
  1 removed completely.
  8 partially changed.
  1 completely unchanged.
Per-source Analysis:

--------------------------------------------------
 +1000 - Source: /file_new - (gained 1000, lost 0)
--------------------------------------------------
  New symbols:
      +1000: added type=t, size=1000 bytes

---------------------------------------------------------------------
 +1000 - Source: /file_pair_unchanged_changed - (gained 1000, lost 0)
---------------------------------------------------------------------
  Grown symbols:
      +1000: changed type=t, (was 1000 bytes, now 2000 bytes)

----------------------------------------------------------------------------
 +1000 - Source: /file_pair_unchanged_diffbuck_added - (gained 1000, lost 0)
----------------------------------------------------------------------------
  New symbols:
      +1000: added type=@, size=1000 bytes

-------------------------------------------------------------------
 +1000 - Source: /file_pair_unchanged_added - (gained 1000, lost 0)
-------------------------------------------------------------------
  New symbols:
      +1000: added type=t, size=1000 bytes

------------------------------------------------------------------------------
 +1000 - Source: /file_pair_unchanged_diffbuck_changed - (gained 1000, lost 0)
------------------------------------------------------------------------------
  Grown symbols:
      +1000: changed type=@, (was 1000 bytes, now 2000 bytes)

----------------------------------------------------------
 +1000 - Source: /file_all_changed - (gained 1000, lost 0)
----------------------------------------------------------
  Grown symbols:
      +1000: changed type=t, (was 1000 bytes, now 2000 bytes)

-------------------------------------------------------
 +1000 - Source: /file_tetra - (gained 2000, lost 1000)
-------------------------------------------------------
  New symbols:
      +1000: added type=t, size=1000 bytes
  Removed symbols:
      -1000: removed type=t, size=1000 bytes
  Grown symbols:
      +1000: size_changed type=t, (was 1000 bytes, now 2000 bytes)

------------------------------------------------------------------------------
 -1000 - Source: /file_pair_unchanged_diffbuck_removed - (gained 0, lost 1000)
------------------------------------------------------------------------------
  Removed symbols:
      -1000: removed type=@, size=1000 bytes

----------------------------------------------------------
 -1000 - Source: /file_all_deleted - (gained 0, lost 1000)
----------------------------------------------------------
  Removed symbols:
      -1000: removed type=t, size=1000 bytes

---------------------------------------------------------------------
 -1000 - Source: /file_pair_unchanged_removed - (gained 0, lost 1000)
---------------------------------------------------------------------
  Removed symbols:
      -1000: removed type=t, size=1000 bytes
"""

    self.maxDiff = None
    self.assertMultiLineEqual(expected_output, result)
    print "explain_binary_size_delta_unittest: All tests passed"


if __name__ == '__main__':
  unittest.main()
