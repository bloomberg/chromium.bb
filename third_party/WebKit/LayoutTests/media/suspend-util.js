function suspendMediaElement(video, expectedState, callback) {
  var pollSuspendState = function() {
    if (!window.internals.isMediaElementSuspended(video)) {
      window.requestAnimationFrame(pollSuspendState);
      return;
    }

    callback();
  };

  window.requestAnimationFrame(pollSuspendState);
  window.internals.forceStaleStateForMediaElement(video, expectedState);
}

function preloadMetadataSuspendTest(t, video, src, expectSuspend) {
  assert_true(!!window.internals, 'This test requires windows.internals.');
  video.onerror = t.unreached_func();

  var timeWatcher = t.step_func(function() {
    if (video.currentTime > 0) {
      assert_false(window.internals.isMediaElementSuspended(video));
      t.done();
    } else {
      window.requestAnimationFrame(timeWatcher);
    }
  });

  var eventListener = t.step_func(function() {
    assert_equals(expectSuspend,
                  window.internals.isMediaElementSuspended(video));
    if (!expectSuspend) {
      t.done();
      return;
    }

    window.requestAnimationFrame(timeWatcher);
    video.play();
  });

  video.addEventListener('loadedmetadata', eventListener, false);
  video.src = src;
}

function suspendTest(t, video, src, expectedState) {
  assert_true(!!window.internals, 'This test requires windows.internals.');
  video.onerror = t.unreached_func();

  var timeWatcher = t.step_func(function() {
    if (video.currentTime > 0) {
      assert_false(window.internals.isMediaElementSuspended(video));
      t.done();
    } else {
      window.requestAnimationFrame(timeWatcher);
    }
  });

  // We can't force a suspend state until loading has started.
  video.addEventListener('loadstart', t.step_func(function() {
    suspendMediaElement(video, expectedState, t.step_func(function() {
      assert_true(window.internals.isMediaElementSuspended(video));
      window.requestAnimationFrame(timeWatcher);
      video.play();
    }));
  }), false);

  video.src = src;
}
