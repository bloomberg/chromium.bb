'use strict';

function assert_relatedapplication_equals(actual, expected, description) {
  assert_equals(actual.platform, expected.platform, description);
  assert_equals(actual.url, expected.url, description);
  assert_equals(actual.id, expected.id, description);
}

function assert_array_relatedapplication_equals(
    actual, expected, description) {
  assert_equals(actual.length, expected.length, description);

  for (let i = 0; i < actual.length; i++)
    assert_relatedapplication_equals(actual[i], expected[i], description);
}
