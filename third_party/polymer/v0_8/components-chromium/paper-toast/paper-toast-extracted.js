
(function() {

  var PaperToast = Polymer({
    is: 'paper-toast',

    properties: {
      /**
       * The duration in milliseconds to show the toast.
       */
      duration: {
        type: Number,
        value: 3000
      },

      /**
       * The text to display in the toast.
       */
      text: {
        type: String,
        value: ""
      },

      /**
       * True if the toast is currently visible.
       */
      visible: {
        type: Boolean,
        readOnly: true,
        value: false,
        observer: '_visibleChanged'
      }
    },

    created: function() {
      Polymer.IronA11yAnnouncer.requestAvailability();
    },

    ready: function() {
      this.async(function() {
        this.hide();
      });
    },

    /**
     * Show the toast.
     * @method show
     */
    show: function() {
      if (PaperToast.currentToast) {
        PaperToast.currentToast.hide();
      }
      PaperToast.currentToast = this;
      this.removeAttribute('aria-hidden');
      this._setVisible(true);
      this.fire('iron-announce', {
        text: this.text
      });
      this.debounce('hide', this.hide, this.duration);
    },

    /**
     * Hide the toast
     */
    hide: function() {
      this.setAttribute('aria-hidden', 'true');
      this._setVisible(false);
    },

    /**
     * Toggle the opened state of the toast.
     * @method toggle
     */
    toggle: function() {
      if (!this.visible) {
        this.show();
      } else {
        this.hide();
      }
    },

    _visibleChanged: function(visible) {
      this.toggleClass('paper-toast-open', visible);
    }
  });

  PaperToast.currentToast = null;

})();
