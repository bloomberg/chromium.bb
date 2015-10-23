function cellular_preload_test() {
  async_test(function(t) {
    internals.setNetworkStateNotifierTestOnly(true);
    internals.setNetworkConnectionInfo('cellular', 2.0);

    var video = document.querySelector('video');
    assert_equals(video.preload, 'none')
    video.src = 'resources/test-positive-start-time.webm';

    video.onsuspend = t.step_func_done();
    video.onprogress = t.unreached_func();
    t.add_cleanup(function() {
      internals.setNetworkStateNotifierTestOnly(false);
    });
  });
}
