# Performance Dashboard

The Chrome Performance Dashboard is an App Engine web application for displaying
and monitoring performance test results.

-   [Getting set up to contribute](/dashboard/docs/getting-set-up.md)
-   [Data format for new graph data](/dashboard/docs/data-format.md)
-   [Rolling back from a broken deployment](/dashboard/docs/rollback.md)
-   [Project glossary](/dashboard/docs/glossary.md)
-   [Pages and endpoints](/dashboard/docs/pages-and-endpoints.md)

## Code Structure

All dashboard code lives in the `dashboard/` subdirectory, with the endpoints
for individual HTTP API handlers in that directory. There are a number of
subprojects which are also hosted in that directory:

-   `api`: Handlers for API endpoints.
-   `common`: A module collecting various utilities and common types used across
    multiple subprojects in the performance dashboard.
-   `docs`: A collection of documents for users of the dashboard, the API, and
    other subprojects.
-   `elements`: User interface elements used in the performance dashboard web
    user interface. This is deprecated in favor of `spa`.
-   `models`: A collection of types which represent entities in the data store,
    with associated business logic for operations. These models should be
    thought of as models in the Model-View-Controller conceptual framework.
-   `pinpoint`: The performance regression bisection implementation. See more in
    the [pinpoint documentation](/dashboard/dashboard/pinpoint/README.md).
-   `services`: A collection of wrappers which represent external services which
    the dashboard subprojects interact with.
-   `static`: Directory containing all the static assets used in the user
    interface. This is deprecated in favor of `spa`.
-   `templates`: HTML files representing the templates for views served through
    the App Engine user interface. This is deprecated in favor of `spa`.
-   `sheriff_config`: A standalone service for managing sheriff configurations
    hosted in git repositories, accessed through luci-config.

## Dependencies

The dashboard requires Python modules from Google Cloud SDK to run.
An easy way to install it is through cipd, using the following command.
(You can replace `~/google-cloud-sdk` with another location if you prefer.)

```
echo infra/gae_sdk/python/all latest | cipd ensure -root ~/google-cloud-sdk -ensure-file -
```

Then run the following command to set `PYTHONPATH`. It is recommended to add
this to your `.bashrc` or equivalent.

```
export PYTHONPATH=~/google-cloud-sdk
```

If you already have a non-empty `PYTHONPATH`, you can add the Cloud SDK location
to it. However, dashboard does not require any additional Python libraries.
It is recommended that your `PYTHONPATH` only contains the cloud SDK while
testing the dashboard.

(Note: The official source for Google Cloud SDK is https://cloud.google.com/sdk,
and you can install Python modules with
`gcloud components install app-engine-python`.
However, this method of installation has not been verified with the dashboard.)

## Unit Tests

First following the steps given in Dependencies section above.
Then, run dashboard unit tests with:

```
dashboard/bin/run_py_tests
```

## Contact

Bugs can be reported on the Chromium issue tracker using the `Speed>Dashboard`
component:

-   [File a new Dashboard issue](https://bugs.chromium.org/p/chromium/issues/entry?description=Describe+the+problem:&components=Speed%3EDashboard&summary=[chromeperf]+)
-   [List open Dashboard issues](https://bugs.chromium.org/p/chromium/issues/list?q=component%3ASpeed%3EDashboard)

Note that some existing issues can be found in the
[Github issue tracker](https://github.com/catapult-project/catapult/issues), but
this is no longer the preferred location for filing new issues.

For questions and feedback, send an email to
chrome-perf-dashboard-team@google.com.
