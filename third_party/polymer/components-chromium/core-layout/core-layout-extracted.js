
  
  (function() {
  
    Polymer('core-layout', {

      isContainer: false,
      /**
       * Controls if the element lays out vertically or not.
       *
       * @attribute vertical
       * @type boolean
       * @default false
       */
      vertical: false,
      /**
       * Controls how the items are aligned in the main-axis direction. For 
       * example for a horizontal layout, this controls how each item is aligned
       * horizontally.
       *
       * @attribute justify
       * @type string start|center|end|between
       * @default ''
       */
      justify: '',
      /**
       * Controls how the items are aligned in cross-axis direction. For 
       * example for a horizontal layout, this controls how each item is aligned
       * vertically.
       *
       * @attribute align
       * @type string start|center|end
       * @default ''
       */
      align: '',
      /**
       * Controls whether or not the items layout in reverse order.
       *
       * @attribute reverse
       * @type boolean
       * @default false
       */
      reverse: false,
      layoutPrefix: 'core-',
  
      // NOTE: include template so that styles are loaded, but remove
      // so that we can decide dynamically what part to include
      registerCallback: function(polymerElement) {
        var template = polymerElement.querySelector('template');
        this.styles = template.content.querySelectorAll('style').array();
        this.styles.forEach(function(s) {
          s.removeAttribute('no-shim');
        })
      },
  
      fetchTemplate: function() {
        return null;
      },
  
      attached: function() {
        this.installScopeStyle(this.styles[0]);
        if (this.children.length) {
          this.isContainer = true;
        }
        var container = this.isContainer ? this : this.parentNode;  
        // detect if laying out a shadowRoot host.
        var forHost = container instanceof ShadowRoot;
        if (forHost) {
          this.installScopeStyle(this.styles[1], 'host');
          container = container.host || document.body;
        }
        this.layoutContainer = container;
      },

      detached: function() {
        this.layoutContainer = null;
      },

      layoutContainerChanged: function(old) {
        this.style.display = this.layoutContainer === this ? null : 'none';
        this.verticalChanged();
        this.alignChanged();
        this.justifyChanged();
      },

      setLayoutClass: function(prefix, old, newValue) {
        if (this.layoutContainer) {
          prefix = this.layoutPrefix + prefix;
          if (old) {
            this.layoutContainer.classList.remove(prefix + old);
          }
          if (newValue) {
            this.layoutContainer.classList.add(prefix + newValue);
          }
        }
      },

      verticalChanged: function(old) {
        old = old ? 'v' : 'h';
        var vertical = this.vertical ? 'v' : 'h';
        this.setLayoutClass('', old, vertical);
      },

      alignChanged: function(old) {
        this.setLayoutClass('align-', old, this.align);
      },

      justifyChanged: function(old) {
        this.setLayoutClass('justify-', old, this.justify);
      },

      reverseChanged: function(old) {
        old = old ? 'reverse' : '';
        var newValue = this.reverse ? 'reverse' : '';
        this.setLayoutClass('', old, newValue);
      }

    });

  })();
  