/**
   * Upon scrolling past a threshold, fade in the rear background layer and fade out the front
   * background layer (opacity CSS transitioned over time).
   */
  Polymer.AppLayout.registerEffect('fade-background', {
    /** @this Polymer.AppLayout.ElementWithBackground */
    setUp: function setUp(config) {
      var duration = config.duration || '0.5s';
      this.$.backgroundFrontLayer.style.willChange = 'opacity';
      this.$.backgroundFrontLayer.style.webkitTransform = 'translateZ(0)';
      this.$.backgroundFrontLayer.style.transitionProperty = 'opacity';
      this.$.backgroundFrontLayer.style.transitionDuration = duration;
      this.$.backgroundRearLayer.style.willChange = 'opacity';
      this.$.backgroundRearLayer.style.webkitTransform = 'translateZ(0)';
      this.$.backgroundRearLayer.style.transitionProperty = 'opacity';
      this.$.backgroundRearLayer.style.transitionDuration = duration;
    },
    /** @this Polymer.AppLayout.ElementWithBackground */
    run: function run(p, y) {
      if (p >= 1) {
        this.$.backgroundFrontLayer.style.opacity = 0;
        this.$.backgroundRearLayer.style.opacity = 1;
      } else {
        this.$.backgroundFrontLayer.style.opacity = 1;
        this.$.backgroundRearLayer.style.opacity = 0;
      }
    }
  });