/**
 * Generates config objects that are common between multiple properties for use
 * in property-suite.js
 */
let config_templates = {};

config_templates.borderConfig = {
  validKeywords: [
    'none',
    //TODO: Implement the keywords listed below.
    // 'hidden',
    // 'dotted',
    // 'dashed',
    // 'solid',
    // 'double',
    // 'groove',
    // 'ridge',
    // 'inset',
    // 'outset',
    // 'inherit'
  ],
  validObjects: [
  ],
  supportsMultiple: false,
  invalidObjects: [new CSSSimpleLength(4, 'px')]
};