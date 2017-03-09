/**
 * Generates config objects that are common between multiple properties for use
 * in property-suite.js
 */

let config_templates = {};
config_templates.lengthConfig_ = {
  validObjects: [
      new CSSSimpleLength(1, 'px'),
      new CSSSimpleLength(3, 'em'),
      new CSSSimpleLength(4, 'ex'),
      new CSSSimpleLength(5, 'ch'),
      new CSSSimpleLength(6, 'rem'),
      new CSSSimpleLength(7, 'vw'),
      new CSSSimpleLength(8, 'vh'),
      new CSSSimpleLength(9, 'vmin'),
      new CSSSimpleLength(10, 'vmax'),
      new CSSSimpleLength(11, 'cm'),
      new CSSSimpleLength(12, 'mm'),
      new CSSSimpleLength(13, 'in'),
      new CSSSimpleLength(14, 'pc'),
      new CSSSimpleLength(15, 'pt'),
      // Fully populated calc (no percent)
      new CSSCalcLength({
        px: 1200,
        em: 4.5,
        ex: -6.7,
        ch: 8.9,
        rem: -10,
        vw: 1.1,
        vh: -1.2,
        vmin: 1.3,
        vmax: -1.4,
        cm: 1.56,
        mm: -1.7,
        in: 1.8,
        pc: -1.9,
        pt: 2.1}),
      // Contains only px
      new CSSCalcLength({px: 10}),
      // Contains neither pixels or percent
      new CSSCalcLength({vmin: 5, in: 10}),
      ],
      validStringMappings: {
        // Contains multiplication
        'calc(3 * (5px - 3em))': new CSSCalcLength({px: 15, em: -9}),
        // Contains division
        'calc((5vmin + 1mm) / 2)': new CSSCalcLength({vmin: 2.5, mm: 0.5}),
      },
      supportsMultiple: false,
      invalidObjects: [new CSSNumberValue(1)]
};

config_templates.lengthPercentConfig_ = Object.assign(
    {}, config_templates.lengthConfig_);
config_templates.lengthPercentConfig_.validObjects.concat([
    new CSSSimpleLength(2, 'percent'),
    // Fully populated calc
    new CSSCalcLength({
      px: 1200,
      percent: -2.3,
      em: 4.5,
      ex: -6.7,
      ch: 8.9,
      rem: -10,
      vw: 1.1,
      vh: -1.2,
      vmin: 1.3,
      vmax: -1.4,
      cm: 1.56,
      mm: -1.7,
      in: 1.8,
      pc: -1.9,
      pt: 2.1}),
    // Contains only percent
    new CSSCalcLength({percent: 10}),
    // Contains px and percent
    new CSSCalcLength({px: 6, percent: 10})
  ]);

config_templates.borderConfig_ = {
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

config_templates.lengthConfig = function() {
  return Object.assign({}, config_templates.lengthConfig_);
};
config_templates.lengthPercentConfig = function() {
  return Object.assign({}, config_templates.lengthPercentConfig_);
};
config_templates.borderConfig = function() {
  return Object.assign({}, config_templates.borderConfig_);
};
