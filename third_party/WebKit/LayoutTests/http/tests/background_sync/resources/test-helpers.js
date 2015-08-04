// Returns a promise which resolves when all registrations have been cleared
// from the given background sync manager.
function clear_registered_syncs(sync_manager) {
  return sync_manager.getRegistrations().then(function(registrations) {
    return Promise.all(
      registrations.map(registration => registration.unregister()));
  });
}

// Clears all background sync registrations from all managers.
function clear_all_syncs(serviceworker_registration) {
  return Promise.all([
    clear_registered_syncs(serviceworker_registration.sync),
    clear_registered_syncs(serviceworker_registration.periodicSync)
  ]);
}
