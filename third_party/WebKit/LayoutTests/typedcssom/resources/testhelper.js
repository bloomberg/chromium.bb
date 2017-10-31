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
