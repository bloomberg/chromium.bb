/**
 * Generates config objects that are common between multiple properties for use
 * in property-suite.js
 */

let config_templates = {};
config_templates.lengthConfig_ = {
  validObjects: [
      new CSSUnitValue(1, 'px'),
      new CSSUnitValue(3, 'em'),
      new CSSUnitValue(4, 'ex'),
      new CSSUnitValue(5, 'ch'),
      new CSSUnitValue(6, 'rem'),
      new CSSUnitValue(7, 'vw'),
      new CSSUnitValue(8, 'vh'),
      new CSSUnitValue(9, 'vmin'),
      new CSSUnitValue(10, 'vmax'),
      new CSSUnitValue(11, 'cm'),
      new CSSUnitValue(12, 'mm'),
      new CSSUnitValue(13, 'in'),
      new CSSUnitValue(14, 'pc'),
      new CSSUnitValue(15, 'pt'),
      // // Fully populated calc (no percent)
      // new CSSCalcValue({
      //   px: 1200,
      //   em: 4.5,
      //   ex: -6.7,
      //   ch: 8.9,
      //   rem: -10,
      //   vw: 1.1,
      //   vh: -1.2,
      //   vmin: 1.3,
      //   vmax: -1.4,
      //   cm: 1.56,
      //   mm: -1.7,
      //   in: 1.8,
      //   pc: -1.9,
      //   pt: 2.1}),
      // // Contains only px
      // new CSSCalcValue({px: 10}),
      // // Contains neither pixels or percent
      // new CSSCalcValue({vmin: 5, in: 10}),
      ],
      validStringMappings: {
      //   // Contains multiplication
      //   'calc(3 * (5px - 3em))': new CSSCalcValue({px: 15, em: -9}),
      //   // Contains division
      //   'calc((5vmin + 1mm) / 2)': new CSSCalcValue({vmin: 2.5, mm: 0.5}),
      },
      supportsMultiple: false,
      invalidObjects: [
          new CSSUnitValue(1, 'number'),
          new CSSUnitValue(2, 'percent'),
      ]
};

config_templates.lengthPercentConfig_ = Object.assign(
    {}, config_templates.lengthConfig_);
config_templates.lengthPercentConfig_.validObjects.concat([
    new CSSUnitValue(2, 'percent'),
    // Fully populated calc
    // new CSSCalcValue({
    //   px: 1200,
    //   percent: -2.3,
    //   em: 4.5,
    //   ex: -6.7,
    //   ch: 8.9,
    //   rem: -10,
    //   vw: 1.1,
    //   vh: -1.2,
    //   vmin: 1.3,
    //   vmax: -1.4,
    //   cm: 1.56,
    //   mm: -1.7,
    //   in: 1.8,
    //   pc: -1.9,
    //   pt: 2.1}),
    // // Contains only percent
    // new CSSCalcValue({percent: 10}),
    // // Contains px and percent
    // new CSSCalcValue({px: 6, percent: 10})
  ]);
config_templates.lengthPercentConfig_.invalidObjects = [
    new CSSUnitValue(1, 'number'),
];

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
  invalidObjects: [new CSSUnitValue(4, 'px')]
};

// TODO(crbug.com/774887): Flesh this out once we have a table of
// what properties normalize to what.
config_templates.positionConfig_ = {
  validKeywords: [
  ],
  validObjects: [
  ],
  supportsMultiple: false,
  invalidObjects: []
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
config_templates.positionConfig = function() {
  return Object.assign({}, config_templates.positionConfig_);
};
