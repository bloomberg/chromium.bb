

  Polymer('paper-checkbox', {
    
    /**
     * Fired when the checked state changes.
     *
     * @event change
     */
    
    toggles: true,

    checkedChanged: function() {
      var cl = this.$.checkbox.classList;
      cl.toggle('checked', this.checked);
      cl.toggle('unchecked', !this.checked);
      cl.toggle('checkmark', !this.checked);
      cl.toggle('box', this.checked);
      this.setAttribute('aria-checked', this.checked ? 'true': 'false');
      this.fire('change');
    },

    checkboxAnimationEnd: function() {
      var cl = this.$.checkbox.classList;
      cl.toggle('checkmark', this.checked && !cl.contains('checkmark'));
      cl.toggle('box', !this.checked && !cl.contains('box'));
    }

  });
  
