

  Polymer('core-tooltip', {

    /**
     * A simple string label for the tooltip to display. To display a rich
     * that includes HTML, use the `tip` attribute on the content.
     *
     * @attribute label
     * @type string
     * @default null
     */
    label: null,

    /**
     * If true, the tooltip an arrow pointing towards the content.
     *
     * @attribute noarrow
     * @type boolean
     * @default false
     */
    noarrow: false,

    /**
     * If true, the tooltip displays by default.
     *
     * @attribute show
     * @type boolean
     * @default false
     */
    show: false,

    /**
     * Positions the tooltip to the top, right, bottom, left of its content.
     *
     * @attribute position
     * @type string
     * @default 'bottom'
     */
    position: 'bottom',

    attached: function() {
      this.setPosition();
    },

    labelChanged: function(oldVal, newVal) {
      // Run if we're not after attached().
      if (oldVal) {
        this.setPosition();
      }
    },

    setPosition: function() {
      var controlWidth = this.clientWidth;
      var controlHeight = this.clientHeight;

      var styles = getComputedStyle(this.$.tooltip);
      var toolTipWidth = parseFloat(styles.width);
      var toolTipHeight = parseFloat(styles.height);

      switch (this.position) {
        case 'top':
        case 'bottom':
          this.$.tooltip.style.left = (controlWidth - toolTipWidth) / 2 + 'px';
          break;
        case 'left':
        case 'right':
          this.$.tooltip.style.top = (controlHeight - toolTipHeight) / 2 + 'px';
          break;
      }
    }
  });

