/**
   * Vertically translate the background based on a factor of the scroll position.
   */
  Polymer.AppLayout.registerEffect('parallax-background', {
    /**
     * @param {{scalar: string}} config
     * @this Polymer.AppLayout.ElementWithBackground
     */
    setUp: function setUp(config) {
      var scalar = parseFloat(config.scalar);

      this._deltaBg = this.$.backgroundFrontLayer.offsetHeight - this.$.background.offsetHeight;
      if (this._deltaBg === 0) {
        if (isNaN(scalar)) {
          scalar = 0.8;
        }
        this._deltaBg = this._dHeight * scalar;
      } else {
        if (isNaN(scalar)) {
          scalar = 1;
        }
        this._deltaBg = this._deltaBg * scalar;
      }
    },
    /** @this Polymer.AppLayout.ElementWithBackground */
    tearDown: function tearDown() {
      delete this._deltaBg;
    },
    /** @this Polymer.AppLayout.ElementWithBackground */
    run: function run(p, y) {
      this.transform('translate3d(0px, ' + (this._deltaBg * Math.min(1, p)) + 'px, 0px)', this.$.backgroundFrontLayer);
      if (this.$.backgroundRearLayer) {
        this.transform('translate3d(0px, ' + (this._deltaBg * Math.min(1, p)) + 'px, 0px)', this.$.backgroundRearLayer);
      }
    }
  });