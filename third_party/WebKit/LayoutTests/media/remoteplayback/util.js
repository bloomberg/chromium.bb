function disableRemotePlaybackBackendForTest(t) {
  var remotePlaybackBackendEnabledOldValue =
      internals.runtimeFlags.remotePlaybackBackendEnabled;
  internals.runtimeFlags.remotePlaybackBackendEnabled = false;

  t.add_cleanup(() => {
    internals.runtimeFlags.remotePlaybackBackendEnabled =
        remotePlaybackBackendEnabledOldValue;
  });
}