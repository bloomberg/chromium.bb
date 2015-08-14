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
   * And pressing right or tab at this point would move the focus to:
   *
   *   focusable  focusable  focusable
   *   focusable  focusable  [focused]  (row: 1, col: 2)
   *   focusable  focusable  focusable
   *
   * @constructor
   */
  function FocusGrid() {
    /** @type {!Array<!cr.ui.FocusRow>} */
    this.rows = [];

    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker;
    this.eventTracker_.add(cr.doc, 'keydown', this.onKeydown_.bind(this));
    this.eventTracker_.add(cr.doc, 'focusin', this.onFocusin_.bind(this));

    /** @private {cr.ui.FocusRow.Delegate} */
    this.delegate_ = new FocusGrid.RowDelegate(this);
  }

  /**
   * Row delegate to overwrite the behavior of a mouse click to deselect any row
   * that wasn't clicked.
   * @param {cr.ui.FocusGrid} focusGrid
   * @constructor
   * @implements {cr.ui.FocusRow.Delegate}
   */
  FocusGrid.RowDelegate = function(focusGrid) {
    /** @private {cr.ui.FocusGrid} */
    this.focusGrid_ = focusGrid;
  };

  FocusGrid.RowDelegate.prototype = {
    /** @override */
    onKeydown: function(row, e) { return false; },

    /** @override */
    onMousedown: function(row, e) {
      // Only care about left mouse click.
      if (e.button)
        return false;

      // Only the clicked row should be active.
      var target = assertInstanceof(e.target, Node);
      this.focusGrid_.rows.forEach(function(row) {
        row.makeActive(row.root.contains(target));
      });

      return true;
    },
  };

  FocusGrid.prototype = {
    /**
     * Unregisters event handlers and removes all |this.rows|.
     */
    destroy: function() {
      this.rows.forEach(function(row) { row.destroy(); });
      this.rows.length = 0;
    },

    /**
     * @param {Node} target A target item to find in this grid.
     * @return {number} The row index. -1 if not found.
     */
    getRowIndexForTarget: function(target) {
      for (var i = 0; i < this.rows.length; ++i) {
        if (this.rows[i].getElements().indexOf(target) >= 0)
          return i;
      }
      return -1;
    },

    /**
     * @param {Element} root An element to search for.
     * @return {?cr.ui.FocusRow} The row with root of |root| or null.
     */
    getRowForRoot: function(root) {
      for (var i = 0; i < this.rows.length; ++i) {
        if (this.rows[i].root == root)
          return this.rows[i];
      }
      return null;
    },

    /**
     * Handles keyboard shortcuts to move up/down in the grid.
     * @param {Event} e The key event.
     * @private
     */
    onKeydown_: function(e) {
      var target = assertInstanceof(e.target, Node);
      var rowIndex = this.getRowIndexForTarget(target);
      if (rowIndex == -1)
        return;

      var row = -1;

      if (e.keyIdentifier == 'Up')
        row = rowIndex - 1;
      else if (e.keyIdentifier == 'Down')
        row = rowIndex + 1;
      else if (e.keyIdentifier == 'PageUp')
        row = 0;
      else if (e.keyIdentifier == 'PageDown')
        row = this.rows.length - 1;

      var rowToFocus = this.rows[row];
      if (rowToFocus) {
        this.ignoreFocusChange_ = true;
        rowToFocus.getEquivalentElement(this.lastFocused).focus();
        e.preventDefault();
      }
    },

    /**
     * Keep track of the last column that the user manually focused.
     * @param {Event} e The focusin event.
     * @private
     */
    onFocusin_: function(e) {
      if (this.ignoreFocusChange_) {
        this.ignoreFocusChange_ = false;
        return;
      }

      var target = assertInstanceof(e.target, Node);
      if (this.getRowIndexForTarget(target) != -1)
        this.lastFocused = target;
    },

    /**
     * Adds |row| to the end of this list.
     * @param {!cr.ui.FocusRow} row The row that needs to be added to this grid.
     */
    addRow: function(row) {
      this.addRowBefore(row, null);
    },

    /**
     * Adds |row| before |nextRow|. If |nextRow| is not in the list or it's
     * null, |row| is added to the end.
     * @param {!cr.ui.FocusRow} row The row that needs to be added to this grid.
     * @param {cr.ui.FocusRow} nextRow The row that should follow |row|.
     */
    addRowBefore: function(row, nextRow) {
      row.delegate = row.delegate || this.delegate_;

      var nextRowIndex = this.rows.indexOf(nextRow);
      if (nextRowIndex == -1)
        this.rows.push(row);
      else
        this.rows.splice(nextRowIndex, 0, row);
    },

    /**
     * Removes a row from the focus row. No-op if row is not in the grid.
     * @param {cr.ui.FocusRow} row The row that needs to be removed.
     */
    removeRow: function(row) {
      var nextRowIndex = this.rows.indexOf(row);
      if (nextRowIndex > -1)
        this.rows.splice(nextRowIndex, 1);
    },

    /**
     * Makes sure that at least one row is active. Should be called once, after
     * adding all rows to FocusGrid.
     */
    ensureRowActive: function() {
      if (this.rows.length == 0)
        return;

      for (var i = 0; i < this.rows.length; ++i) {
        if (this.rows[i].isActive())
          return;
      }

      this.rows[0].makeActive(true);
    },
  };

  return {
    FocusGrid: FocusGrid,
  };
});
