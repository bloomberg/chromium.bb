
  
    Polymer('paper-progress', {
      
      /**
       * The number that represents the current secondary progress.
       *
       * @attribute secondaryProgress
       * @type number
       * @default 0
       */
      secondaryProgress: 0,
      
      /**
       * Use an indeterminate progress indicator.
       *
       * @attribute indeterminate
       * @type boolean
       * @default false
       */
      indeterminate: false,

      step: 0,
      
      observe: {
        'value secondaryProgress min max indeterminate': 'update'
      },
      
      update: function() {
        this.super();
        this.secondaryProgress = this.clampValue(this.secondaryProgress);
        this.secondaryRatio = this.calcRatio(this.secondaryProgress) * 100;

        // If we use attribute/class binding, the animation sometimes doesn't translate properly
        // on Safari 7.1. So instead, we toggle the class here in the update method.
        this.$.activeProgress.classList.toggle('indeterminate', this.indeterminate);
      },

      transformProgress: function(progress, ratio) {
        var transform = 'scaleX(' + (ratio / 100) + ')';
        progress.style.transform = progress.style.webkitTransform = transform;
      },

      ratioChanged: function() {
        this.transformProgress(this.$.activeProgress, this.ratio);
      },

      secondaryRatioChanged: function() {
        this.transformProgress(this.$.secondaryProgress, this.secondaryRatio);
      }
      
    });
    
  