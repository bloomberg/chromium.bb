function enableRemotePlaybackBackendForTest(t) {
  var remotePlaybackBackendEnabledOldValue =
      internals.runtimeFlags.remotePlaybackBackendEnabled;
  internals.runtimeFlags.remotePlaybackBackendEnabled = true;

  t.add_cleanup(() => {
    internals.runtimeFlags.remotePlaybackBackendEnabled =
        remotePlaybackBackendEnabledOldValue;
  });
}
