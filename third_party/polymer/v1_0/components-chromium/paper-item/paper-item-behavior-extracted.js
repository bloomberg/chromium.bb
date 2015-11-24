/** @polymerBehavior Polymer.PaperItemBehavior */
  Polymer.PaperItemBehaviorImpl = {
    hostAttributes: {
      role: 'option',
      tabindex: '0'
    }
  };

  /** @polymerBehavior */
  Polymer.PaperItemBehavior = [
    Polymer.IronControlState,
    Polymer.IronButtonState,
    Polymer.PaperItemBehaviorImpl
  ];