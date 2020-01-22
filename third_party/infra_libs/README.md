# bqh.py

`infra_libs/bqh` provides some helper methods for working with BigQuery. It
is recommended you use this library over using the client API directly as it
includes common logic for handling protobufs, formatting errors, safe guards,
and handling edge cases.

[TOC]

# Usage

Create a client:

```
from google.cloud import bigquery
from google.oauth2 import service_account

service_account_file = ...
bigquery_creds = service_account.Credentials.from_service_account_file(
    service_account_file)
bigquery_client = bigquery.client.Client(
    project='example-project', credentials=bigquery_creds)
```

Send rows:

```
from infra_libs import bqh

# ExampleRow is a protobuf Message
rows = [ExampleRow(example_field='1'), ExampleRow(example_field='2')]
try:
  bqh.send_rows(bigquery_client, 'example-dataset', 'example-table', rows)
except bqh.BigQueryInsertError:
  # handle error
except bqh.UnsupportedTypeError:
  # handle error
```

# Limits

Please see [BigQuery
docs](https://cloud.google.com/bigquery/quotas#streaminginserts) for the most
updated limits for streaming inserts. It is expected that the client is
responsible for ensuring their usage will not exceed these limits through
infra_libs/biquery usage. A note on maximum rows per request: send_rows()
batches rows per request, ensuring that no more than 10,000 rows are sent per
request, and allowing for custom batch size. BigQuery recommends using 500 as a
practical limit (so we use this as a default), and experimenting with your
specific schema and data sizes to determine the batch size with the ideal
balance of throughput and latency for your use case.

# Authentication

Authentication for the Cloud projects happens
[during client creation](https://googlecloudplatform.github.io/google-cloud-python/latest/bigquery/usage.html#authentication-configuration).
What form this takes depends on the application.

# vPython

infra_libs/bqh is available via vPython as a CIPD package. To update the
available version, build and upload a new wheel with
[dockerbuild](../../infra/tools/dockerbuild/README#subcommand_wheel_build).

google-cloud-bigquery is required to create a BigQuery client. Unfortunately,
google-cloud-bigquery has quite a few dependencies. Here is the Vpython spec you
need to use infra_libs.bigquery and google-cloud-bigquery:

```
wheel: <
  name: "infra/python/wheels/requests-py2_py3"
  version: "version:2.13.0"
>
wheel: <
  name: "infra/python/wheels/google_api_python_client-py2_py3"
  version: "version:1.6.2"
>
wheel: <
  name: "infra/python/wheels/six-py2_py3"
  version: "version:1.10.0"
>
wheel: <
  name: "infra/python/wheels/uritemplate-py2_py3"
  version: "version:3.0.0"
>
wheel: <
  name: "infra/python/wheels/httplib2-py2_py3"
  version: "version:0.10.3"
>
wheel: <
  name: "infra/python/wheels/rsa-py2_py3"
  version: "version:3.4.2"
>
wheel: <
  name: "infra/python/wheels/pyasn1_modules-py2_py3"
  version: "version:0.0.8"
>
wheel: <
  name: "infra/python/wheels/pyasn1-py2_py3"
  version: "version:0.2.3"
>
wheel: <
  name: "infra/python/wheels/oauth2client/linux-arm64_cp27_cp27mu"
  version: "version:3.0.0"
>
wheel: <
  name: "infra/python/wheels/protobuf-py2_py3"
  version: "version:3.2.0"
>
wheel: <
  name: "infra/python/wheels/infra_libs-py2"
  version: "version:1.3.0"
>
```

# Recommended Monitoring

You can use ts_mon to track upload latency and errors.

```
from infra_libs import ts_mon

upload_durations = ts_mon.CumulativeDistributionMetric(
    'example/service/upload/durations',
    'Time taken to upload an event to bigquery.',
    [ts_mon.StringField('status')],
    bucketer=ts_mon.GeometricBucketer(10**0.04),
    units=ts_mon.MetricsDataUnits.SECONDS)

upload_errors = ts_mon.CounterMetric(
    'example/service/upload/errors',
    'Errors encountered upon uploading an event to bigquery.',
    [ts_mon.StringField('error type')])

with ts_mon.ScopedMeasureTime(upload_durations):
  try:
    bqh.send_rows(...)
  except UnsupportedTypeError:
    upload_errors.Add(...)
```
