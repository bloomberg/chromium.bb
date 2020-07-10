# Build API Metrics

[TOC]

## Overview

The overarching intent of the Build API Metrics subsystem is to enable
exploration, and eventually monitoring and alerting for build-related
performance issues.

The Build API Metrics subsystem is defined in terms of Metric Events.
See `infra/proto/src/chromiumos/metrics.proto`.

## How it works

During an invocation of the Build API, we set up an environment variable
`BUILD_API_METRICS_LOG` which specifies a log file into which metric events can
be appended. Afterwards, the Build API can call into the `utils/metrics.py` code
to read the metric events out (see `chromite.api.deserialize_metrics_log`.) This
data is transformed into MetricEvents (as per `metrics.proto`). These are fed
back through the Build API's Response message which is returned to the `recipes`
code. There it is uploaded into BigTable for later exploration using various
front-ends, such as Dremel.
