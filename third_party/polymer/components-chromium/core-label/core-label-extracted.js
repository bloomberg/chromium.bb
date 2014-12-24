
    (function() {

      var ID = 0;
      function generate(node) {
        if (!node.id) {
          node.id = 'core-label-' + ID++;
        }
        return node.id;
      }

      Polymer('core-label', {
        /**
         * A query selector string for a "target" element not nested in the `<core-label>`
         *
         * @attribute for
         * @type string
         * @default ''
         */
        publish: {
          'for': {reflect: true, value: ''}
        },
        eventDelegates: {
          'tap': 'tapHandler'
        },
        created: function() {
          generate(this);
          this._forElement = null;
        },
        ready: function() {
          if (!this.for) {
            this._forElement = this.querySelector('[for]');
            this._tie();
          }
        },
        tapHandler: function(ev) {
          if (!this._forElement) {
            return;
          }
          if (ev.target === this._forElement) {
            return;
          }
          this._forElement.focus();
          this._forElement.click();
          this.fire('tap', null, this._forElement);
        },
        _tie: function() {
          if (this._forElement) {
            this._forElement.setAttribute('aria-labelledby', this.id);
          }
        },
        _findScope: function() {
          var n = this.parentNode;
          while(n && n.parentNode) {
            n = n.parentNode;
          }
          return n;
        },
        forChanged: function(oldFor, newFor) {
          if (this._forElement) {
            this._forElement.removeAttribute('aria-labelledby');
          }
          var scope = this._findScope();
          if (!scope) {
            return;
          }
          this._forElement = scope.querySelector(newFor);
          if (this._forElement) {
            this._tie();
          }
        }
      });
    })();
  