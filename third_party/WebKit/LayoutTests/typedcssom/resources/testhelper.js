const style_value_attributes = {
  'CSSKeywordValue': ['value'],
  'CSSUnitValue': ['unit', 'value'],
};

// Compares two CSSStyleValues to check if they're the same type
// and have the same attributes.
function assert_style_value_equals(a, b) {
  assert_equals(a.constructor.name, b.constructor.name);
  for (const attr of style_value_attributes[a.constructor.name]) {
    assert_equals(a[attr], b[attr], attr);
  }
}

// Compares two arrays of CSSStyleValues to check if every element is equal
function assert_style_value_array_equals(a, b) {
  assert_equals(a.length, b.length);
  for (let i = 0; i < a.length; i++) {
    assert_style_value_equals(a[i], b[i]);
  }
}

// Creates a new div element with specified inline style.
function newDivWithStyle(style) {
  let target = document.createElement('div');
  target.style = style;
  return target;
}

const gValidUnits = [
  'number', 'percent', 'em', 'ex', 'ch',
  'ic', 'rem', 'lh', 'rlh', 'vw',
  'vh', 'vi', 'vb', 'vmin', 'vmax',
  'cm', 'mm', 'q', 'in', 'pt',
  'pc', 'px', 'deg', 'grad', 'rad',
  'turn', 's', 'ms', 'Hz', 'kHz',
  'dpi', 'dpcm', 'dppx', 'fr',
];
