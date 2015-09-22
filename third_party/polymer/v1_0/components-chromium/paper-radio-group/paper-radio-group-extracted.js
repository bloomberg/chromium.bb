
  Polymer({
    is: 'paper-radio-group',

    behaviors: [
      Polymer.IronA11yKeysBehavior,
      Polymer.IronSelectableBehavior
    ],

    hostAttributes: {
      role: 'radiogroup',
      tabindex: 0
    },

    properties: {
      /**
       * Overriden from Polymer.IronSelectableBehavior
       */
      attrForSelected: {
        type: String,
        value: 'name'
      },

      /**
       * Overriden from Polymer.IronSelectableBehavior
       */
      selectedAttribute: {
        type: String,
        value: 'checked'
      },

      /**
       * Overriden from Polymer.IronSelectableBehavior
       */
      selectable: {
        type: String,
        value: 'paper-radio-button'
      }
    },

    keyBindings: {
      'left up': 'selectPrevious',
      'right down': 'selectNext',
    },

    /**
     * Selects the given value.
     */
     select: function(value) {
      if (this.selected) {
        var oldItem = this._valueToItem(this.selected);

        // Do not allow unchecking the selected item.
        if (this.selected == value) {
          oldItem.checked = true;
          return;
        }

        if (oldItem)
          oldItem.checked = false;
      }

      Polymer.IronSelectableBehavior.select.apply(this, [value]);
      this.fire('paper-radio-group-changed');
    },

    /**
     * Selects the previous item. If the previous item is disabled, then it is
     * skipped, and its previous item is selected
     */
    selectPrevious: function() {
      var length = this.items.length;
      var newIndex = Number(this._valueToIndex(this.selected));

      do {
        newIndex = (newIndex - 1 + length) % length;
      } while (this.items[newIndex].disabled)

      this.select(this._indexToValue(newIndex));
    },

    /**
     * Selects the next item. If the next item is disabled, then it is
     * skipped, and the next item after it is selected.
     */
    selectNext: function() {
      var length = this.items.length;
      var newIndex = Number(this._valueToIndex(this.selected));

      do {
        newIndex = (newIndex + 1 + length) % length;
      } while (this.items[newIndex].disabled)

      this.select(this._indexToValue(newIndex));
    },
  });
