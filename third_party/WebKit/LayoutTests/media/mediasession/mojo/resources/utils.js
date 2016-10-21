var MOJOM_ACTION_PLAY = 0;
var MOJOM_ACTION_PAUSE = 1;
var MOJOM_ACTION_PLAY_PAUSE = 2;
var MOJOM_ACTION_PREVIOUS_TRACK = 3;
var MOJOM_ACTION_NEXT_TRACK = 4;
var MOJOM_ACTION_SEEK_FORWARD = 5;
var MOJOM_ACTION_SEEK_BACKWARD = 6;

function assert_image_equals(expected, observed) {
  assert_equals(expected.src, observed.src);
  assert_equals(expected.type, observed.type);
  assert_equals(expected.sizes, observed.sizes);
}

function assert_metadata_equals(expected, observed) {
  assert_equals(expected.title, observed.title);
  assert_equals(expected.artist, observed.artist);
  assert_equals(expected.album, observed.album);
  assert_equals(expected.artwork.length, observed.artwork.length);
  for (var i = 0; i < expected.artwork.length; i++) {
    assert_image_equals(expected.artwork[i], observed.artwork[i]);
  }
}
