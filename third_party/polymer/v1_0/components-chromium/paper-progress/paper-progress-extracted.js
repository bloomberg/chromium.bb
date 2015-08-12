Polymer({

    is: 'paper-progress',

    behaviors: [
      Polymer.IronRangeBehavior
    ],

    properties: {

      /**
       * The number that represents the current secondary progress.
       */
      secondaryProgress: {
        type: Number,
        value: 0,
        notify: true
      },

      /**
       * The secondary ratio
       */
      secondaryRatio: {
        type: Number,
        value: 0,
        readOnly: true,
        observer: '_secondaryRatioChanged'
      },

      /**
       * Use an indeterminate progress indicator.
       */
      indeterminate: {
        type: Boolean,
        value: false,
        notify: true,
        observer: '_toggleIndeterminate'
      }
    },

    observers: [
      '_ratioChanged(ratio)',
      '_secondaryProgressChanged(secondaryProgress, min, max)'
    ],

    _toggleIndeterminate: function() {
      // If we use attribute/class binding, the animation sometimes doesn't translate properly
      // on Safari 7.1. So instead, we toggle the class here in the update method.
      this.toggleClass('indeterminate', this.indeterminate, this.$.activeProgress);
      this.toggleClass('indeterminate', this.indeterminate, this.$.indeterminateSplitter);
    },

    _transformProgress: function(progress, ratio) {
      var transform = 'scaleX(' + (ratio / 100) + ')';
      progress.style.transform = progress.style.webkitTransform = transform;
    },

    _ratioChanged: function(ratio) {
      this._transformProgress(this.$.activeProgress, ratio);
    },

    _secondaryRatioChanged: function(secondaryRatio) {
      this._transformProgress(this.$.secondaryProgress, secondaryRatio);
    },

    _secondaryProgressChanged: function() {
      this.secondaryProgress = this._clampValue(this.secondaryProgress);
      this._setSecondaryRatio(this._calcRatio(this.secondaryProgress) * 100);
    }

  });