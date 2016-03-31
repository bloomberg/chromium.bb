# Clovis in the Cloud: Developer Guide

This document describes how to collect Chromium traces using Google Compute
Engine.

[TOC]

## Initial setup

Install the [gcloud command line tool][1].

## Deploy the code

```shell
# Build Chrome
BUILD_DIR=out/Release
ninja -C $BUILD_DIR -j1000 -l60 chrome chrome_sandbox

# Deploy to GCE
# CLOUD_STORAGE_PATH is the path in Google Cloud Storage under which the
# Clovis deployment will be uploaded.

./tools/android/loading/gce/deploy.sh $BUILD_DIR $CLOUD_STORAGE_PATH
```

## Start the app in the cloud

Create an instance using latest ubuntu LTS:

```shell
gcloud compute instances create clovis-tracer-1 \
 --machine-type n1-standard-1 \
 --image ubuntu-14-04 \
 --zone europe-west1-c \
 --tags clovis-http-server \
 --scopes cloud-platform \
 --metadata cloud-storage-path=$CLOUD_STORAGE_PATH
 --metadata auto-start=true \
 --metadata-from-file startup-script=tools/android/loading/gce/startup-script.sh
```

**Note:** To start an instance without automatically starting the app on it,
remove the `--metadata auto-start=true` argument. This can be useful when doing
iterative development on the instance, to be able to restart the app manually.

This should output the IP address of the instance.
Otherwise the IP address can be retrieved by doing:

```shell
gcloud compute instances list
```

## Use the app

Check that `http://<instance-ip>:8080/test` prints `hello` when opened in a
browser.

To send a list of URLs to process:

```shell
curl -X POST -d @urls.json http://<instance-ip>:8080/set_tasks
```

where `urls.json` is a file containing URLs as a JSON array.

## Stop the app in the cloud

```shell
gcloud compute instances delete clovis-tracer-1
```

## Connect to the instance with SSH

```shell
gcloud compute ssh clovis-tracer-1
```

## Use the app locally

Set up the local environment:

```shell
virtualenv env
source env/bin/activate
pip install -r pip_requirements.txt
```

Create a JSON file describing the deployment configuration:

```shell
# CONFIG_FILE is the output json file.
# PROJECT_NAME is the Google Cloud project.
# CLOUD_STORAGE_PATH is the path in Google Storage where generated traces will
# be stored.
# CHROME_PATH is the path to the Chrome executable on the host.
cat >$CONFIG_FILE << EOF
{
  "project_name" : "$PROJECT_NAME",
  "cloud_storage_path" : "$CLOUD_STORAGE_PATH",
  "chrome_path" : "$CHROME_PATH"
}
EOF
```

Launch the app, passing the path to the deployment configuration file:

```shell
gunicorn --workers=1 --bind 127.0.0.1:8080 'main:StartApp('\"$CONFIG_FILE\"')'
```

You can now [use the app][2], which is located at http://localhost:8080.

Tear down the local environment:

```shell
deactivate
```

## Project-wide settings

This is already setup, no need to do this again.
Kept here for reference.

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
[2]: #Use-the-app
