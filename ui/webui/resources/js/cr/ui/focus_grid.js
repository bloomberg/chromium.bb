// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /**
   * A class to manage grid of focusable elements in a 2D grid. For example,
   * given this grid:
   *
   *   focusable  [focused]  focusable  (row: 0, col: 1)
   *   focusable  focusable  focusable
   *   focusable  focusable  focusable
   *
   * Pressing the down arrow would result in the focus moving down 1 row and
   * keeping the same column:
   *
   *   focusable  focusable  focusable
   *   focusable  [focused]  focusable  (row: 1, col: 1)
   *   focusable  focusable  focusable
   *
   * And pressing right at this point would move the focus to:
   *
   *   focusable  focusable  focusable
   *   focusable  focusable  [focused]  (row: 1, col: 2)
   *   focusable  focusable  focusable
   *
   * @param {Node=} opt_boundary Ignore focus events outside this node.
   * @param {cr.ui.FocusRow.Observer=} opt_observer An observer of rows.
   * @implements {cr.ui.FocusRow.Delegate}
   * @constructor
   */
  function FocusGrid(opt_boundary, opt_observer) {
    /** @type {Node|undefined} */
    this.boundary_ = opt_boundary;

    /** @private {cr.ui.FocusRow.Observer|undefined} */
    this.observer_ = opt_observer;

    /** @type {!Array.<!cr.ui.FocusRow>} */
    this.rows = [];
  }

  FocusGrid.prototype = {
    /**
     * @param {EventTarget} target A target item to find in this grid.
     * @return {?{row: number, col: number}} A position or null if not found.
     */
    getPositionForTarget: function(target) {
      for (var i = 0; i < this.rows.length; ++i) {
        for (var j = 0; j < this.rows[i].items.length; ++j) {
          if (target == this.rows[i].items[j])
            return {row: i, col: j};
        }
      }
      return null;
    },

    /** @override */
    onKeydown: function(keyRow, e) {
      var rowIndex = this.rows.indexOf(keyRow);
      assert(rowIndex >= 0);

      var row = -1;

      if (e.keyIdentifier == 'Up')
        row = rowIndex - 1;
      else if (e.keyIdentifier == 'Down')
        row = rowIndex + 1;
      else if (e.keyIdentifier == 'PageUp')
        row = 0;
      else if (e.keyIdentifier == 'PageDown')
        row = this.rows.length - 1;

      if (!this.rows[row])
        return;

      var colIndex = keyRow.items.indexOf(e.target);
      var col = Math.min(colIndex, this.rows[row].items.length - 1);

      this.rows[row].focusIndex(col);

      e.preventDefault();
    },

    /**
     * @param {!Array.<!NodeList|!Array.<!Element>>} grid A 2D array of nodes.
     */
    setGrid: function(grid) {
      this.rows.forEach(function(row) { row.destroy(); });

      this.rows = grid.map(function(row) {
        return new cr.ui.FocusRow(row, this.boundary_, this, this.observer_);
      }, this);

      if (!this.getPositionForTarget(document.activeElement) && this.rows[0])
        this.rows[0].activeIndex = 0;
    },
  };

  return {
    FocusGrid: FocusGrid,
  };
});
