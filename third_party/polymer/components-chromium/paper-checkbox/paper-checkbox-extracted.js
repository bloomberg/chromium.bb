

  Polymer('paper-checkbox', {
    
    /**
     * Fired when the checked state changes due to user interaction.
     *
     * @event change
     */
     
    /**
     * Fired when the checked state changes.
     *
     * @event core-change
     */
    
    toggles: true,

    checkedChanged: function() {
      this.$.checkbox.classList.toggle('checked', this.checked);
      this.setAttribute('aria-checked', this.checked ? 'true': 'false');
      this.$.checkmark.classList.toggle('hidden', !this.checked);
      this.fire('core-change');
    },

    checkboxAnimationEnd: function() {
      var cl = this.$.checkmark.classList;
      cl.toggle('hidden', !this.checked && cl.contains('hidden'));
    }

  });
  
