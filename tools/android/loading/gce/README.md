# Clovis in the Cloud: Developer Guide

This document describes how to collect Chromium traces using Google Compute
Engine.

[TOC]

## Initial setup

Install the [gcloud command line tool][1].

Checkout the source:

```shell
mkdir clovis
cd clovis
gcloud init
```

When offered, accept to clone the Google Cloud repo.

## Update or Change the code

Make changes to the code, or simply copy the latest version from Chromium into
your local Google Cloud repository. Then commit and push:

```shell
git commit
git push -u origin master
```

If there are instances already running, they need to be restarted for this to
take effect.

## Start the app in the cloud

Create an instance using latest ubuntu LTS:

```shell
gcloud compute instances create clovis-tracer-1 \
 --machine-type n1-standard-1 \
 --image ubuntu-14-04 \
 --zone europe-west1-c \
 --tags clovis-http-server \
 --scopes cloud-platform \
 --metadata-from-file startup-script=default/startup-script.sh
```

This should output the IP address of the instance.
Otherwise the IP address can be retrieved by doing:

```shell
gcloud compute instances list
```

Interact with the app on the port 8080 in your browser at
`http://<instance-ip>:8080`.

TODO: allow starting the instance in the cloud without Supervisor. This enables
iterative development on the instance using SSH, manually starting and stopping
the app. This can be done using [instance metadata][2].

## Stop the app in the cloud

```shell
gcloud compute instances delete clovis-tracer-1
```

## Connect to the instance with SSH

```shell
gcloud compute ssh clovis-tracer-1
```

## Use the app locally

Setup the local environment:

```shell
virtualenv env
source env/bin/activate
pip install -r pip_requirements.txt
```

Launch the app:

```shell
gunicorn --workers=2 main:app --bind 127.0.0.1:8000
```

In your browser, go to `http://localhost:8000` and use the app.

Tear down the local environment:

```shell
deactivate
```

## Project-wide settings

This is already setup, no need to do this again.
Kept here for reference.

### Server configuration file

`main.py` expects to find a `server_config.json` file, which is a dictionary
with the keys:

*   `project_name`: the name of the Google Compute project,
*   `bucket_name`: the name of the Google Storage bucket used to store the
    results.

### Firewall rule

Firewall rule to allow access to the instance HTTP server from the outside:

```shell
gcloud compute firewall-rules create default-allow-http-8080 \
    --allow tcp:8080 \
    --source-ranges 0.0.0.0/0 \
    --target-tags clovis-http-server \
    --description "Allow port 8080 access to http-server"
```

The firewall rule can be disabled with:

```shell
gcloud compute firewall-rules delete default-allow-http-8080
```

[1]: https://cloud.google.com/sdk
[2]: https://cloud.google.com/compute/docs/startupscript#custom
