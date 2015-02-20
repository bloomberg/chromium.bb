

  Polymer('paper-dropdown',{

    publish: {
      transition: 'paper-dropdown-transition'
    },

    ready: function() {
      this.super();
      this.sizingTarget = this.$.scroller;
    }

  });

