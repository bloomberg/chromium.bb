// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

log_util = (function() {
  'use strict';

  /**
   * Creates a new log dump.  |events| is a list of all events, |polledData| is
   * an object containing the results of each poll, |tabData| is an object
   * containing data for individual tabs, |date| is the time the dump was
   * created, as a formatted string, and |privacyStripping| is whether or not
   * private information should be removed from the generated dump.
   *
   * Returns the new log dump as an object.  Resulting object may have a null
   * |numericDate|.
   *
   * TODO(eroman): Use javadoc notation for these parameters.
   *
   * Log dumps are just JSON objects containing five values:
   *
   *   |userComments| User-provided notes describing what this dump file is
   *                  about.
   *   |constants| needed to interpret the data.  This also includes some
   *               browser state information.
   *   |events| from the NetLog.
   *   |polledData| from each PollableDataHelper available on the source OS.
   *   |tabData| containing any tab-specific state that's not present in
   *             |polledData|.
   *
   * |polledData| and |tabData| may be empty objects, or may be missing data for
   * tabs not present on the OS the log is from.
   */
  function createLogDump(
      userComments, constants, events, polledData, tabData, numericDate,
      privacyStripping) {
    if (privacyStripping)
      events = events.map(stripPrivacyInfo);

    var logDump = {
      'userComments': userComments,
      'constants': constants,
      'events': events,
      'polledData': polledData,
      'tabData': tabData
    };

    // Not technically client info, but it's used at the same point in the code.
    if (numericDate && constants.clientInfo) {
      constants.clientInfo.numericDate = numericDate;
    }

    return logDump;
  }

  /**
   * Creates a full log dump using |polledData| and the return value of each
   * tab's saveState function and passes it to |callback|.
   */
  function onUpdateAllCompleted(
      userComments, callback, privacyStripping, polledData) {
    var logDump = createLogDump(
        userComments, Constants,
        EventsTracker.getInstance().getAllCapturedEvents(), polledData,
        getTabData_(), timeutil.getCurrentTime(), privacyStripping);
    callback(JSON.stringify(logDump));
  }

  /**
   * Called to create a new log dump.  Must not be called once a dump has been
   * loaded.  Once a log dump has been created, |callback| is passed the dumped
   * text as a string.
   */
  function createLogDumpAsync(userComments, callback, privacyStripping) {
    g_browser.updateAllInfo(onUpdateAllCompleted.bind(
        null, userComments, callback, privacyStripping));
  }

  /**
   * Gather any tab-specific state information prior to creating a log dump.
   */
  function getTabData_() {
    var tabData = {};
    var tabSwitcher = MainView.getInstance().tabSwitcher();
    var tabIdToView = tabSwitcher.getAllTabViews();
    for (var tabId in tabIdToView) {
      var view = tabIdToView[tabId];
      if (view.saveState)
        tabData[tabId] = view.saveState();
    }
  }

  /**
   * Loads a full log dump.  Returns a string containing a log of the load.
   * |opt_fileName| should always be given when loading from a file, instead of
   * from a log dump generated in-memory.
   * The process goes like this:
   * 1)  Load constants.  If this fails, or the version number can't be handled,
   *     abort the load.  If this step succeeds, the load cannot be aborted.
   * 2)  Clear all events.  Any event observers are informed of the clear as
   *     normal.
   * 3)  Call onLoadLogStart(polledData, tabData) for each view with an
   *     onLoadLogStart function.  This allows tabs to clear any extra state
   *     that would affect the next step.  |polledData| contains the data polled
   *     for all helpers, but |tabData| contains only the data from that
   *     specific tab.
   * 4)  Add all events from the log file.
   * 5)  Call onLoadLogFinish(polledData, tabData) for each view with an
   *     onLoadLogFinish function.  The arguments are the same as in step 3.  If
   *     there is no onLoadLogFinish function, it throws an exception, or it
   *     returns false instead of true, the data dump is assumed to contain no
   *     valid data for the tab, so the tab is hidden.  Otherwise, the tab is
   *     shown.
   */
  function loadLogDump(logDump, opt_fileName) {
    // Perform minimal validity check, and abort if it fails.
    if (typeof(logDump) != 'object')
      return 'Load failed.  Top level JSON data is not an object.';

    // String listing text summary of load errors, if any.
    var errorString = '';

    if (!areValidConstants(logDump.constants))
      errorString += 'Invalid constants object.\n';
    if (typeof(logDump.events) != 'object')
      errorString += 'NetLog events missing.\n';
    if (typeof(logDump.constants.logFormatVersion) != 'number')
      errorString += 'Invalid version number.\n';

    if (errorString.length > 0)
      return 'Load failed:\n\n' + errorString;

    if (typeof(logDump.polledData) != 'object')
      logDump.polledData = {};
    if (typeof(logDump.tabData) != 'object')
      logDump.tabData = {};

    var kSupportedLogFormatVersion = 1;

    if (logDump.constants.logFormatVersion != kSupportedLogFormatVersion) {
      return 'Unable to load different log version.' +
          ' Found ' + logDump.constants.logFormatVersion + ', Expected ' +
          Constants.logFormatVersion;
    }

    g_browser.receivedConstants(logDump.constants);

    // Check for validity of each log entry, and then add the ones that pass.
    // Since the events are kept around, and we can't just hide a single view
    // on a bad event, we have more error checking for them than other data.
    var validEvents = [];
    var numDeprecatedPassiveEvents = 0;
    for (var eventIndex = 0; eventIndex < logDump.events.length; ++eventIndex) {
      var event = logDump.events[eventIndex];
      if (typeof event == 'object' && typeof event.source == 'object' &&
          typeof event.time == 'string' &&
          typeof EventTypeNames[event.type] == 'string' &&
          typeof EventSourceTypeNames[event.source.type] == 'string' &&
          getKeyWithValue(EventPhase, event.phase) != '?') {
        if (event.wasPassivelyCaptured) {
          // NOTE: Up until Chrome 18, log dumps included "passively captured"
          // events. These are no longer supported, so skip past them
          // to avoid confusing the rest of the code.
          numDeprecatedPassiveEvents++;
          continue;
        }
        validEvents.push(event);
      }
    }

    // Determine the export date for the loaded log.
    //
    // Dumps created from chrome://net-internals (Chrome 17 - Chrome 59) will
    // have this set in constants.clientInfo.numericDate.
    //
    // However more recent dumping mechanisms (chrome://net-export/ and
    // --log-net-log) write the constants object directly to a file when the
    // dump is *started* so lack this ability.
    //
    // In this case, we will synthesize this field by looking at the timestamp
    // of the last event logged. In practice this works fine since there tend
    // to be lots of events logged.
    //
    // TODO(eroman): Fix the log format / writers to avoid this problem. Dumps
    // *should* contain the time when capturing started, and when capturing
    // ended.
    if (typeof logDump.constants.clientInfo.numericDate != 'number') {
      if (validEvents.length > 0) {
        var lastEvent = validEvents[validEvents.length - 1];
        ClientInfo.numericDate =
            timeutil.convertTimeTicksToDate(lastEvent.time).getTime();
      } else {
        errorString += 'Can\'t guess export date as there are no events.\n';
        ClientInfo.numericDate = 0;
      }
    }

    // Prevent communication with the browser.  Once the constants have been
    // loaded, it's safer to continue trying to load the log, even in the case
    // of bad data.
    MainView.getInstance().onLoadLog(opt_fileName);

    // Delete all events.  This will also update all logObservers.
    EventsTracker.getInstance().deleteAllLogEntries();

    // Inform all the views that a log file is being loaded, and pass in
    // view-specific saved state, if any.
    var tabSwitcher = MainView.getInstance().tabSwitcher();
    var tabIdToView = tabSwitcher.getAllTabViews();
    for (var tabId in tabIdToView) {
      var view = tabIdToView[tabId];
      view.onLoadLogStart(logDump.polledData, logDump.tabData[tabId]);
    }
    EventsTracker.getInstance().addLogEntries(validEvents);

    var numInvalidEvents = logDump.events.length -
        (validEvents.length + numDeprecatedPassiveEvents);
    if (numInvalidEvents > 0) {
      errorString += 'Unable to load ' + numInvalidEvents +
          ' events, due to invalid data.\n\n';
    }

    if (numDeprecatedPassiveEvents > 0) {
      errorString += 'Discarded ' + numDeprecatedPassiveEvents +
          ' passively collected events. Use an older version of Chrome to' +
          ' load this dump if you want to see them.\n\n';
    }

    // Update all views with data from the file.  Show only those views which
    // successfully load the data.
    for (var tabId in tabIdToView) {
      var view = tabIdToView[tabId];
      var showView = false;
      // The try block eliminates the need for checking every single value
      // before trying to access it.
      try {
        if (view.onLoadLogFinish(
                logDump.polledData, logDump.tabData[tabId], logDump)) {
          showView = true;
        }
      } catch (error) {
        errorString +=
            'Caught error while calling onLoadLogFinish: ' + error + '\n\n';
      }
      tabSwitcher.showTabLink(tabId, showView);
    }

    return errorString + 'Log loaded.';
  }

  /**
   * Loads a log dump from the string |logFileContents|, which can be either a
   * full net-internals dump, or a NetLog dump only.  Returns a string
   * containing a log of the load.
   */
  function loadLogFile(logFileContents, fileName) {
    // Try and parse the log dump as a single JSON string.  If this succeeds,
    // it's most likely a full log dump.  Otherwise, it may be a dump created by
    // --log-net-log.
    var parsedDump = null;
    var errorString = '';
    try {
      parsedDump = JSON.parse(logFileContents);
    } catch (error) {
      try {
        // We may have a --log-net-log=blah log dump.  If so, remove the comma
        // after the final good entry, and add the necessary close brackets.
        var end = Math.max(
            logFileContents.lastIndexOf(',\n'),
            logFileContents.lastIndexOf(',\r'));
        if (end != -1) {
          parsedDump = JSON.parse(logFileContents.substring(0, end) + ']}');
          errorString += 'Log file truncated.  Events may be missing.\n';
        }
      } catch (error2) {
      }
    }

    if (!parsedDump)
      return 'Unable to parse log dump as JSON file.';
    return errorString + loadLogDump(parsedDump, fileName);
  }

  // Exports.
  return {createLogDumpAsync: createLogDumpAsync, loadLogFile: loadLogFile};
})();

