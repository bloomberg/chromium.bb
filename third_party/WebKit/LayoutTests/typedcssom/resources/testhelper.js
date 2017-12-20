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
    case 'CSSTransformValue':
      assert_style_value_array_equals(a, b);
      break;
    case 'CSSRotation':
      assert_style_value_equals(a.angle, b.angle);
      // fallthrough
    case 'CSSTranslation':
    case 'CSSScale':
      assert_style_value_equals(a.x, b.x);
      assert_style_value_equals(a.y, b.y);
      assert_style_value_equals(a.z, b.z);
      assert_style_value_equals(a.is2D, b.is2D);
      break;
    case 'CSSSkew':
      assert_style_value_equals(a.ax, b.ax);
      assert_style_value_equals(a.ay, b.ay);
      break;
    case 'CSSPerspective':
      assert_style_value_equals(a.length, b.length);
      break;
    case 'CSSMatrixComponent':
      assert_matrix_approx_equals(a.matrix, b.matrix, 1e-6);
      break;
    default:
      assert_equals(a, b);
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
