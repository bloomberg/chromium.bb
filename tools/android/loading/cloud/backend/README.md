# Clovis in the Cloud: Developer Guide

This document describes how to collect Chromium traces using Google Compute
Engine.

[TOC]

## Initial setup

Install the [gcloud command line tool][1].

## Deploy the code

```shell
# Build Chrome (do not use the component build).
BUILD_DIR=out/Release
ninja -C $BUILD_DIR -j1000 -l60 chrome chrome_sandbox

# Deploy to GCE
# CLOUD_STORAGE_PATH is the path in Google Cloud Storage under which the
# Clovis deployment will be uploaded.

./tools/android/loading/cloud/backend/deploy.sh $BUILD_DIR $CLOUD_STORAGE_PATH
```

## Start the app in the cloud

Create an instance using latest ubuntu LTS:

```shell
gcloud compute instances create clovis-tracer-1 \
 --machine-type n1-standard-1 \
 --image ubuntu-14-04 \
 --zone europe-west1-c \
 --scopes cloud-platform,https://www.googleapis.com/auth/cloud-taskqueue \
 --metadata cloud-storage-path=$CLOUD_STORAGE_PATH,taskqueue_tag=some_tag \
 --metadata-from-file \
     startup-script=$CHROMIUM_SRC/tools/android/loading/cloud/backend/startup-script.sh
```

**Note:** To start an instance without automatically starting the app on it,
add a `auto-start=false` metadata. This can be useful when doing iterative
development on the instance, to be able to restart the app manually.

This should output the IP address of the instance.
Otherwise the IP address can be retrieved by doing:

```shell
gcloud compute instances list
```

**Note:** It can take a few minutes for the instance to start. You can follow
the progress of the startup script on the gcloud console web interface (menu
"Compute Engine" > "VM instances" then click on your instance and scroll down to
see the "Serial console output") or from the command line using:

```shell
gcloud compute instances get-serial-port-output clovis-tracer-1
```

## Use the app

Create tasks from the associated AppEngine application, see [documentation][3].
Make sure the `taskqueue_tag` of the AppEngine request matches the one of the
ComputeEngine instances.

## Stop the app in the cloud

```shell
gcloud compute instances delete clovis-tracer-1
```

## Connect to the instance with SSH

```shell
gcloud compute ssh clovis-tracer-1
```

## Use the app locally

From a new directory, set up a local environment:

```shell
virtualenv env
source env/bin/activate
pip install -r \
    $CHROMIUM_SRC/tools/android/loading/cloud/backend/pip_requirements.txt
```

The first time, you may need to get more access tokens:

```shell
gcloud beta auth application-default login --scopes \
    https://www.googleapis.com/auth/cloud-taskqueue \
    https://www.googleapis.com/auth/cloud-platform
```

Create a JSON file describing the deployment configuration:

```shell
# CONFIG_FILE is the output json file.
# PROJECT_NAME is the Google Cloud project.
# CLOUD_STORAGE_PATH is the path in Google Storage where generated traces will
# be stored.
# CHROME_PATH is the path to the Chrome executable on the host.
# CHROMIUM_SRC is the Chromium src directory.
cat >$CONFIG_FILE << EOF
{
  "project_name" : "$PROJECT_NAME",
  "cloud_storage_path" : "$CLOUD_STORAGE_PATH",
  "chrome_path" : "$CHROME_PATH",
  "src_path" : "$CHROMIUM_SRC",
  "taskqueue_tag" : "some_tag"
}
EOF
```

Launch the app, passing the path to the deployment configuration file:

```shell
python $CHROMIUM_SRC/tools/android/loading/cloud/backend/worker.py \
    --config $CONFIG_FILE
```

You can now [use the app][2].

Tear down the local environment:

```shell
deactivate
```

[1]: https://cloud.google.com/sdk
[2]: #Use-the-app
[3]: ../frontend/README.md
