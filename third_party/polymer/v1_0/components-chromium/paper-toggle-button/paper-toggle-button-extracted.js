
    Polymer({
      is: 'paper-toggle-button',

      behaviors: [
        Polymer.PaperInkyFocusBehavior,
        Polymer.IronCheckedElementBehavior
      ],

      hostAttributes: {
        role: 'button',
        'aria-pressed': 'false',
        tabindex: 0
      },

      properties: {
        /**
         * Fired when the checked state changes due to user interaction.
         *
         * @event change
         */
        /**
         * Fired when the checked state changes.
         *
         * @event iron-change
         */
      },

      listeners: {
        track: '_ontrack'
      },

      ready: function() {
        this._isReady = true;
      },

      // button-behavior hook
      _buttonStateChanged: function() {
        if (this.disabled) {
          return;
        }
        if (this._isReady) {
          this.checked = this.active;
        }
      },

      _ontrack: function(event) {
        var track = event.detail;
        if (track.state === 'start') {
          this._trackStart(track);
        } else if (track.state === 'track') {
          this._trackMove(track);
        } else if (track.state === 'end') {
          this._trackEnd(track);
        }
      },

      _trackStart: function(track) {
        this._width = this.$.toggleBar.offsetWidth / 2;
        /*
         * keep an track-only check state to keep the dragging behavior smooth
         * while toggling activations
         */
        this._trackChecked = this.checked;
        this.$.toggleButton.classList.add('dragging');
      },

      _trackMove: function(track) {
        var dx = track.dx;
        this._x = Math.min(this._width,
            Math.max(0, this._trackChecked ? this._width + dx : dx));
        this.translate3d(this._x + 'px', 0, 0, this.$.toggleButton);
        this._userActivate(this._x > (this._width / 2));
      },

      _trackEnd: function(track) {
        this.$.toggleButton.classList.remove('dragging');
        this.transform('', this.$.toggleButton);
      }

    });
  