JSON schema for the input of the simulator.

```jsonc
{
  // List of zero or more sources to register.
  "sources": [
    {
      // Required time at which to register the source in milliseconds since
      // the UNIX epoch formatted as a base-10 string.
      "timestamp": "123",

      // Required origin to which the report will be sent if the source is
      // attributed.
      "reporting_origin": "https://reporting.example",

      // Required origin on which to register the source.
      "source_origin": "https://source.example",

      // Required source type, either "navigation" or "event", corresponding to
      // whether the source is registered on click or on view, respectively.
      "source_type": "navigation",

      // Required dictionary data to register a source.
      "Attribution-Reporting-Register-Source": {
        // Required uint64 formatted as a base-10 string.
        "source_event_id": "123456789",

        // Required site on which the source will be attributed.
        "destination": "https://destination.example",

        // Optional int64 in seconds formatted as a base-10 string.
        // Default to 30 days.
        "expiry": "86400000",

        // Optional int64 formatted as a base-10 string.
        // Default to 0.
        "priority": "-456",

        // Optional dictionary of filters and corresponding values. Defaults to
        // empty.
        "filter_data": {
          "a": ["b", "c"],
          "d": []
        },

        // Optional uint64 formatted as a base-10 string. Defaults to null.
        "debug_key": "987"
      },

      // Optional list of zero or more aggregatable keys.
      "Attribution-Reporting-Register-Aggregatable-Source": [
        {
          // Required string to identify the key.
          "id": "a",

          // Required uint128 formatted as a base-16 string.
          "key_piece": "0x1"
        }
      ]
    }
  ],

  // List of zero or more triggers to register.
  "triggers": [
    {
      // Required time at which to register the trigger in milliseconds since
      // the UNIX epoch formatted as a base-10 string.
      "timestamp": "123",

      // Required origin to which the report will be sent.
      "reporting_origin": "https://reporting.example",

      // Required origin on which the trigger is being registered.
      "destination_origin": "https://destination.example",

      // Optional list of zero or more event trigger data.
      "Attribution-Reporting-Register-Event-Trigger": [
        {
          // Optional uint64 formatted as a base-10 string.
          // Defaults to 0.
          "trigger_data": "3",

          // Optional int64 formatted as a base-10 string.
          // Defaults to 0.
          "priority": "-456",

          // Optional uint64 formatted as a base-10 string. Defaults to null.
          "deduplication_key": "654",

          // Optional dictionary of filters and corresponding values. Defaults
          // to empty.
          "filters": {
            "a": ["b", "c"],
            "d": []
          },

          // Optional dictionary of negated filters and corresponding values.
          // Defaults to empty.
          "not_filters": {
            "x": ["y"],
            "z": []
          }
        }
      ],

      // Optional list of zero or more aggregatable trigger data.
      "Attribution-Reporting-Register-Aggregatable-Trigger-Data": [
        {
          // Required uint128 formatted as a base-16 string.
          "key_piece": "0x10",

          // Required list of key identifiers.
          "source_keys": ["a"],

          // Optional dictionary of filters and corresponding values. Defaults
          // to empty.
          "filters": {
            "a": ["b", "c"],
            "d": []
          },

          // Optional dictionary of negated filters and corresponding values.
          // Defaults to empty.
          "not_filters": {
            "x": ["y"],
            "z": []
          }
        }
      ],

      // Optional dictionary of key identifiers and corresponding values.
      "Attribution-Reporting-Register-Aggregatable-Values": {
        "a": 123
      },

      // Optional uint64 formatted as a base-10 string. Defaults to null.
      "Attribution-Reporting-Trigger-Debug-Key": "789",

      // Optional dictionary of filters and corresponding values. Defaults to
      // empty.
      "Attribution-Reporting-Filters": {
        "a": ["b", "c"],
        "d": []
      }
    }
  ],

  // List of zero or more cookies to store, for use with debug keys.
  "cookies": [
    {
      // Required time at which to store the cookie in milliseconds since the
      // UNIX epoch formatted as a base-10 string.
      "timestamp": "123",

      // Required URL for which to store the cookie.
      "url": "https://r.test",

      // Required string specifying the cookie to store.
      "Set-Cookie": "ar_debug=1; SameSite=None; Secure; HttpOnly"
    }
  ],

  // List of zero or more times to clear Attribution Reporting data.
  "data_clears": [
    {
      // Required time at which to clear data in milliseconds since the UNIX
      // epoch formatted as a base-10 string.
      "timestamp": "123",

      // Optional time in milliseconds since the UNIX epoch formatted as a
      // base-10 string. Only Attribution Reporting data at or after this time 
      // will be deleted. If omitted, defaults to the beginning of time.
      "delete_begin": "123",

      // Optional time in milliseconds since the UNIX epoch formatted as a
      // base-10 string. Only Attribution Reporting data at or before this
      // time will be deleted. If omitted, defaults to the end of time.
      "delete_end": "456",

      // Optional list of origins whose data to delete. If omitted, data for all
      // origins will be deleted.
      "origins": [
        "https://a.test",
        "https://b.test"
      ]
    }
  ]
}
```

JSON schema for the output of the simulator.

```jsonc
{
  // List of event-level reports. Omitted if empty.
  "event_level_reports": [
    {
      // Time at which the report would have been sent in milliseconds since
      // the UNIX epoch formatted as a base-10 string.
      "report_time": "123",

      // URL to which the report would have been sent.
      "report_url": "https://reporting.example/.well-known/attribution-reporting/report-event-attribution",

      "test_info": {
        // Whether fake reports were generated.
        "randomized_trigger": false
      },

      // The report itself. See
      // https://github.com/WICG/conversion-measurement-api/blob/main/EVENT.md#attribution-reports
      // for details about its fields.
      "report": {}
    },
  ],

  // List of debug event-level reports. Omitted if empty.
  "debug_event_level_reports": [],

  // List of aggregatable reports. Omitted if empty.
  "aggregatable_reports": [
    {
      // Time at which the report would have been sent in milliseconds since
      // the UNIX epoch formatted as a base-10 string.
      "report_time": "123",

      // URL to which the report would have been sent.
      "report_url": "https://reporting.example/.well-known/attribution-reporting/report-aggregate-attribution",

      "test_info": {
        // Aggregatable histograms created from the source and trigger.
        "histograms": [
          {
            // uint128 formatted as a base-16 string.
            "key": "0x0",

            // uint32 value.
            "value": 123
          }
        ],

        // The report itself. See
        // https://github.com/WICG/conversion-measurement-api/blob/main/AGGREGATE.md#aggregatable-reports
        // for details about its fields.
        "report": {}
      }
    }
  ],

  // List of debug aggregatable reports. Omitted if empty.
  "debug_aggregatable_reports": [],

  // List of rejected sources. Omitted if empty.
  "rejected_sources": [
    {
      // Why the source was rejected.
      "reason": "excessiveReportingOrigins",

      // The original input JSON corresponding to the source.
      "source": {}
    }
  ],

  // List of rejected triggers. Omitted if empty.
  "rejected_triggers": [
    {
      // Why the trigger was rejected.
      "reason": "deduplicated",

      // The original input JSON corresponding to the trigger.
      "trigger": {}
    }
  ],

  // List of replaced event-level reports. Omitted if empty.
  // These reports were never sent, but were replaced by higher-priority ones.
  "replaced_event_level_reports": [
    {
      // Time at which the report was replaced in milliseconds since the UNIX
      // epoch formatted as a base-10 string.
      "replacement_time": "123",

      // The report id that the report was replaced by. Omitted if
      // `remove_report_ids` option is true.
      "replaced_by": "21abd97f-73e8-4b88-9389-a9fee6abda5e",

      // URL to which the report would have been sent.
      "report_url": "https://reporting.example/.well-known/attribution-reporting/report-event-attribution",

      "test_info": {
        // Whether fake reports were generated.
        "randomized_trigger": false
      },

      // The report itself. See
      // https://github.com/WICG/conversion-measurement-api/blob/main/EVENT.md#attribution-reports
      // for details about its fields.
      "report": {}
    }
  ]
}
```
