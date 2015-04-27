

  Polymer({
    is: 'x-array-selector',
    
    properties: {

      /**
       * An array containing items from which selection will be made.
       */
      items: {
        type: Array,
        observer: '_itemsChanged'
      },

      /**
       * When `multi` is true, this is an array that contains any selected.
       * When `multi` is false, this is the currently selected item, or `null`
       * if no item is selected.
       */
      selected: {
        type: Object,
        notify: true
      },

      /**
       * When `true`, calling `select` on an item that is already selected
       * will deselect the item.
       */
      toggle: Boolean,

      /**
       * When `true`, multiple items may be selected at once (in this case,
       * `selected` is an array of currently selected items).  When `false`,
       * only one item may be selected at a time.
       */
      multi: Boolean
    },
    
    _itemsChanged: function() {
      // Unbind previous selection
      if (Array.isArray(this.selected)) {
        for (var i=0; i<this.selected.length; i++) {
          this.unbindPaths('selected.' + i);
        }
      } else {
        this.unbindPaths('selected');
      }
      // Initialize selection
      if (this.multi) {
        this.selected = [];
      } else {
        this.selected = null;
      }
    },

    /**
     * Deselects the given item if it is already selected.
     */
    deselect: function(item) {
      if (this.multi) {
        var scol = Polymer.Collection.get(this.selected);
        // var skey = scol.getKey(item);
        // if (skey >= 0) {
        var sidx = this.selected.indexOf(item);
        if (sidx >= 0) {
          var skey = scol.getKey(item);
          this.selected.splice(sidx, 1);
          // scol.remove(item);
          this.unbindPaths('selected.' + skey);
          return true;
        }
      } else {
        this.selected = null;
        this.unbindPaths('selected');
      }
    },

    /**
     * Selects the given item.  When `toggle` is true, this will automatically
     * deselect the item if already selected.
     */
    select: function(item) {
      var icol = Polymer.Collection.get(this.items);
      var key = icol.getKey(item);
      if (this.multi) {
        // var sidx = this.selected.indexOf(item);
        // if (sidx < 0) {
        var scol = Polymer.Collection.get(this.selected);
        var skey = scol.getKey(item);
        if (skey >= 0) {
          this.deselect(item);
        } else if (this.toggle) {
          this.selected.push(item);
          // this.bindPaths('selected.' + sidx, 'items.' + skey);
          // skey = Polymer.Collection.get(this.selected).add(item);
          this.async(function() {
            skey = scol.getKey(item);
            this.bindPaths('selected.' + skey, 'items.' + key);
          });
        }
      } else {
        if (this.toggle && item == this.selected) {
          this.deselect();
        } else {
          this.bindPaths('selected', 'items.' + key);
          this.selected = item;
        }
      }
    }

  });

