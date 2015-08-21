
    Polymer({
      is: 'paper-tooltip',

      hostAttributes: {
        role: 'tooltip',
        tabindex: -1
      },

      behaviors: [
        Polymer.NeonAnimationRunnerBehavior
      ],

      properties: {
        /**
         * The id of the element that the tooltip is anchored to. This element
         * must be a sibling of the tooltip.
         */
        for: {
          type: String,
          observer: '_forChanged'
        },

        /**
         * The spacing between the top of the tooltip and the element it is
         * anchored to.
         */
        marginTop: {
          type: Number,
          value: 14
        },

        animationConfig: {
          type: Object,
          value: function() {
            return {
              'entry': [{
                name: 'fade-in-animation',
                node: this,
                timing: {delay: 500}
              }],
              'exit': [{
                name: 'fade-out-animation',
                node: this
              }]
            }
          }
        },

        _showing: {
          type: Boolean,
          value: false
        }
      },

      listeners: {
        'neon-animation-finish': '_onAnimationFinish'
      },

      /**
       * Returns the target element that this tooltip is anchored to. It is
       * either the element given by the `for` attribute, or the immediate parent
       * of the tooltip.
       */
      get target () {
        var parentNode = Polymer.dom(this).parentNode;
        // If the parentNode is a document fragment, then we need to use the host.
        var ownerRoot = Polymer.dom(this).getOwnerRoot();

        var target;
        if (this.for) {
          target = Polymer.dom(ownerRoot).querySelector('#' + this.for);
        } else {
          target = parentNode.nodeType == Node.DOCUMENT_FRAGMENT_NODE ?
              ownerRoot.host : parentNode;
        }

        return target;
      },

      attached: function() {
        this._target = this.target;

        this.listen(this._target, 'mouseenter', 'show');
        this.listen(this._target, 'focus', 'show');
        this.listen(this._target, 'mouseleave', 'hide');
        this.listen(this._target, 'blur', 'hide');
      },

      detached: function() {
        if (this._target) {
          this.unlisten(this._target, 'mouseenter', 'show');
          this.unlisten(this._target, 'focus', 'show');
          this.unlisten(this._target, 'mouseleave', 'hide');
          this.unlisten(this._target, 'blur', 'hide');
        }
      },

      show: function() {
        if (this._showing)
          return;

        this.cancelAnimation();

        this.toggleClass('hidden', false, this.$.tooltip);
        this._setPosition();
        this._showing = true;

        this.playAnimation('entry');
      },

      hide: function() {
        if (!this._showing)
          return;

        this._showing = false;
        this.playAnimation('exit');
      },

      _forChanged: function() {
        this._target = this.target;
      },

      _setPosition: function() {
        if (!this._target)
          return;

        var parentRect = this.offsetParent.getBoundingClientRect();
        var targetRect = this._target.getBoundingClientRect();
        var thisRect = this.getBoundingClientRect();

        var centerOffset = (targetRect.width - thisRect.width) / 2;

        this.style.left = targetRect.left - parentRect.left + centerOffset + 'px';
        this.style.top = targetRect.top - parentRect.top + targetRect.height + this.marginTop + 'px';
      },

      _onAnimationFinish: function() {
        if (!this._showing) {
          this.toggleClass('hidden', true, this.$.tooltip);
        }
      },
    })
  