# URL-Keyed Metrics API

This describes how to write client code to collect UKM data. Before you add new metrics, you should file a proposal.  See go/ukm for more information.

## Document your metrics in ukm.xml

Any events and metrics you collect need to be documented in [//tools/metrics/ukm/ukm.xml](https://cs.chromium.org/chromium/src/tools/metrics/ukm/ukm.xml)

Important information to include:

* Metric owners: This the email of someone who can answer questions about how this metric is recorded, what it means, and how it should be used. Can include multiple people.
* A description of the event about which you are recording details, including when the event will be recorded.
* For each metric in the event: a description of the data and what it means.
* The unit should be included in the description, along with possible output values.
* If an event will only happen once per Navigation, it can be marked singular="true".

### Example
```xml
<event name="Goat.Teleported">
  <owner>teleporter@chromium.org</owner>
  <summary>
    Recorded when a page teleports a goat.
  </summary>
  <metric name="Duration">
    <summary>
      How long it took to teleport, in ns.
    </summary>
  </metric>
  <metric name="Mass">
    <summary>
      The mass of the teleported goat, in kg, rounded to the nearest multiple of 10.
    </summary>
  </metric>
</event>
```

### Controlling the Aggregation of Metrics

Control of which metrics are included in the History table is done via the same
[`tools/metrics/ukm/ukm.xml`](https://cs.chromium.org/chromium/src/tools/metrics/ukm/ukm.xml)
file in the Chromium codebase. To have a metric aggregated, `<aggregation>` and
`<history>` tags need to be added.

```xml
<event name="Goat.Teleported">
  <metric name="Duration">
    ...
    <aggregation>
      <history>
        <index fields="profile.country"/>
        <statistics>
          <quantiles type="std-percentiles"/>
        </statistics>
      </history>
    </aggregation>
    ...
  </metric>
</event>
```

Supported statistic types are:

*   `<quantiles type="std-percentiles"/>`: Calculates the "standard percentiles"
    for the values which are 1, 5, 10, 25, 50, 75, 90, 95, and 99%ile.
*   `<enumeration/>`: Calculates the proportions of all values individually. The
    proportions indicate the relative frequency of each bucket and are
    calculated independently for each metric over each aggregation. (Details
    below.)

There can also be one or more `index` tags which define additional aggregation
keys. These are a comma-separated list of keys that is appended to the standard
set. These additional keys are optional but, if present, are always present
together. In other words, "fields=profile.county,profile.form_factory" will
cause all the standard aggregations plus each with *both* country *and*
form_factor but **not** with all the standard aggregations (see above) plus only
one of them. If individual and combined versions are desired, use multiple index
tags.

Currently supported additional index fields are:

*   `profile.country`
*   `profile.form_factor`
*   `profile.system_ram`

#### Aggregation by Metrics in the Same Event

In addition to the standard "profile" keys, aggregation can be done against
another metric in the same event.  This is accomplished with the same <index>
tag as above but including "metrics.OtherMetricName" in the fields list. There
is no event name included as the metric must always be in the same event.

The metric to act as a key must also have <aggregation>, <history>, and
<statistics> tags with a valid statistic (currently only <enumeration> is
supported). However, since generating statistics for this "key" metric isn't
likely to be useful on its own, add "export=False" to its <statistics> tag.

```xml
<event name="Memory.Experimental">
  <metric name="ProcessType">
    <aggregation>
      <history>
        <statistics export="False">
          <enumeration/>
        </statistics>
      </history>
    </aggregation>
  </metric>
  <metric name="PrivateMemoryFootprint">
    <aggregation>
      <history>
        <index fields="metrics.ProcessType"/>
        <statistics>
          <quantiles type="std-percentiles"/>
        </statistics>
      </history>
    </aggregation>
  </metric>
</event>
```

## Enumeration Proportions

Porportions are calculated against the number of "page loads" (meaning per
"source" which is usually but not always the same as a browser page load) that
emitted one or more values for the enumeration.  The proportions will sum to 1.0
for an enumeration that emits only one result per page-load if it emits anything
at all. An enumeration emitted more than once per source will result in
proportions that total greater than 1.0 but are still relative to the total
number of loads.

For example, `Security.SiteEngagement` emits one value (either 4 or 2) per source:

*   https://www.google.com/ : 4
*   https://www.facebook.com/ : 2
*   https://www.wikipedia.com/ : 4

A proportion calculated over all sources would sum to 1.0:

*   2 (0.3333)
*   4 (0.6667)

In contrast, `Blink.UseCounter.Feature` emits multiple values per source:

*   https://www.google.com/ : 1, 2, 4, 6
*   https://www.facebook.com/ : 2, 4, 5
*   https://www.wikipedia.com/ : 1, 2, 4

A proportion calculated over all sources would be:

*   1 (0.6667)
*   2 (1.0000)
*   3 (absent)
*   4 (1.0000)
*   5 (0.3333)
*   6 (0.3333)

## Get UkmRecorder instance

In order to record UKM events, your code needs a UkmRecorder object, defined by [//services/metrics/public/cpp/ukm_recorder.h](https://cs.chromium.org/chromium/src/services/metrics/public/cpp/ukm_recorder.h)

There are two main ways of getting a UkmRecorder instance.

1) Use `ukm::UkmRecorder::Get()`.  This currently only works from the Browser process.

2) Use a service connector and get a UkmRecorder.

```cpp
std::unique_ptr<ukm::UkmRecorder> ukm_recorder =
    ukm::UkmRecorder::Create(context()->connector());
ukm::builders::MyEvent(source_id)
    .SetMyMetric(metric_value)
    .Record(ukm_recorder.get());
```

## Get a ukm::SourceId

UKM identifies navigations by their source ID and you'll need to associate and ID with your event in order to tie it to a main frame URL.  Preferrably, get an existing ID for the navigation from another object.

The main method for doing this is by getting a navigation ID:

```cpp
ukm::SourceId source_id = GetSourceIdForWebContentsDocument(web_contents);
ukm::SourceId source_id = ukm::ConvertToSourceId(
    navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
```

Some events however occur in the background, and a navigation ID does not exist. In that case, you can use the `ukm::UkmBackgroundRecorderService` to check whether the event can be recorded. This is achieved by using the History Service to determine whether the event's origin is still present in the user profile.

```cpp
// Add the service as a dependency in your service's constructor.
DependsOn(ukm::UkmBackgroundRecorderFactory::GetInstance());

// Get an instance of the UKM service using a Profile.
auto* ukm_background_service = ukm::UkmBackgroundRecorderFactory::GetForProfile(profile);

// Check if recording a background event for |origin| is allowed.
ukm_background_service->GetBackgroundSourceIdIfAllowed(origin, base::BindOnce(&DidGetBackgroundSourceId));

// A callback will run with an optional source ID.
void DidGetBackgroundSourceId(base::Optional<ukm::SourceId> source_id) {
  if (!source_id) return;  // Can't record as it wasn't found in the history.

  // Use the newly generated source ID.
  ukm::builders::MyEvent(*source_id)
      .SetMyMetric(metric_value)
      .Record(ukm_recorder.get());
}
```

For the remaining cases you may need to temporarily create your own IDs and associate the URL with them. However we currently prefer that this method is not used, and if you need to setup the URL yourself, please email us first at ukm-team@google.com.
Example:

```cpp
ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
ukm_recorder->UpdateSourceURL(source_id, main_frame_url);
```

You will also need to add your class as a friend of UkmRecorder in order to use this private API.

## Create some events

Helper objects for recording your event are generated from the descriptions in ukm.xml.  You can use them like so:

```cpp
#include "services/metrics/public/cpp/ukm_builders.h"

void OnGoatTeleported() {
  ...
  ukm::builders::Goat_Teleported(source_id)
      .SetDuration(duration.InNanoseconds())
      .SetMass(RoundedToMultiple(mass_kg, 10))
      .Record(ukm_recorder);
}
```

If the event name in the XML contains a period (`.`), it is replaced with an underscore (`_`) in the method name.

## Check that it works

Build chromium and run it with '--force-enable-metrics-reporting'.  Trigger your event and check chrome://ukm to make sure the data was recorded correctly.

## Unit testing

You can pass your code a TestUkmRecorder (see [//components/ukm/test_ukm_recorder.h](https://cs.chromium.org/chromium/src/components/ukm/test_ukm_recorder.h)) and then use the methods it provides to test that your data records correctly.
