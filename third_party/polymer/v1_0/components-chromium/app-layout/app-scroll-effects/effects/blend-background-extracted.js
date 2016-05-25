/**
   * While scrolling down, fade in the rear background layer and fade out the front background
   * layer (opacity interpolated based on scroll position).
   */
  Polymer.AppLayout.registerEffect('blend-background', {
    /** @this Polymer.AppLayout.ElementWithBackground */
    setUp: function setUp() {
      this.$.backgroundFrontLayer.style.willChange = 'opacity';
      this.$.backgroundFrontLayer.style.webkitTransform = 'translateZ(0)';
      this.$.backgroundRearLayer.style.willChange = 'opacity';
      this.$.backgroundRearLayer.style.webkitTransform = 'translateZ(0)';
      this.$.backgroundRearLayer.style.opacity = 0;
    },
    /** @this Polymer.AppLayout.ElementWithBackground */
    run: function run(p, y) {
      this.$.backgroundFrontLayer.style.opacity = 1 - p;
      this.$.backgroundRearLayer.style.opacity = p;
    }
  });