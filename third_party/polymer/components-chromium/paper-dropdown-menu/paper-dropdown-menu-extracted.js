

(function() {

  var p = {

    publish: {

      /**
       * A label for the control. The label is displayed if no item is selected.
       *
       * @attribute label
       * @type string
       * @default 'Select an item'
       */
      label: 'Select an item',

      /**
       * The icon to display when the drop-down is opened.
       *
       * @attribute openedIcon
       * @type string
       * @default 'arrow-drop-up'
       */
      openedIcon: 'arrow-drop-up',

      /**
       * The icon to display when the drop-down is closed.
       *
       * @attribute closedIcon
       * @type string
       * @default 'arrow-drop-down'
       */
      closedIcon: 'arrow-drop-down'

    },

    selectedItemLabel: '',

    overlayListeners: {
      'core-overlay-open': 'openAction',
      'core-activate': 'activateAction',
      'core-select': 'selectAction'
    },

    activateAction: function(e) {
      this.opened = false;
    },

    selectAction: function(e) {
      var detail = e.detail;
      if (detail.isSelected) {
        this.selectedItemLabel = detail.item.label || detail.item.textContent;
      } else {
        this.selectedItemLabel = '';
      }
    }

  };

  Polymer.mixin2(p, Polymer.CoreFocusable);
  Polymer('paper-dropdown-menu',p);

})();

