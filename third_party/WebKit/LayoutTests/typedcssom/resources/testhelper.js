// Compares two CSSStyleValues to check if they're the same type
// and have the same attributes.
function assert_style_value_equals(a, b) {
  if (a == null || b == null) {
    assert_equals(a, b);
    return;
  }

  assert_equals(a.constructor.name, b.constructor.name);
  const className = a.constructor.name;
  switch (className) {
    case 'CSSKeywordValue':
      assert_equals(a.value, b.value);
      break;
    case 'CSSUnitValue':
      assert_approx_equals(a.value, b.value, 1e-6);
      assert_equals(a.unit, b.unit);
      break;
    case 'CSSMathSum':
    case 'CSSMathProduct':
    case 'CSSMathMin':
    case 'CSSMathMax':
      assert_style_value_array_equals(a.values, b.values);
      break;
    case 'CSSMathInvert':
    case 'CSSMathNegate':
      assert_style_value_equals(a.value, b.value);
      break;
    case 'CSSUnparsedValue':
      assert_style_value_array_equals(a, b);
      break;
    case 'CSSVariableReferenceValue':
      assert_equals(a.variable, b.variable);
      assert_style_value_equals(a.fallback, b.fallback);
      break;
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
