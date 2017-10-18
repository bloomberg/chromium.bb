const style_value_attributes = {
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
