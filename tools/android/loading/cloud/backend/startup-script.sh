# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script executed at instance startup. It installs the required dependencies,
# downloads the source code, and starts a web server.

set -v

get_instance_metadata() {
  curl -fs http://metadata/computeMetadata/v1/instance/attributes/$1 \
      -H "Metadata-Flavor: Google"
}

# Talk to the metadata server to get the project id and the instance id
PROJECTID=$(curl -s \
    "http://metadata.google.internal/computeMetadata/v1/project/project-id" \
    -H "Metadata-Flavor: Google")

INSTANCE_ID=$(curl -s \
    "http://metadata.google.internal/computeMetadata/v1/instance/hostname" \
    -H "Metadata-Flavor: Google")

# Install dependencies from apt
apt-get update
# Basic dependencies
apt-get install -yq git supervisor python-pip python-dev unzip
# Web server dependencies
apt-get install -yq libffi-dev libssl-dev
# Chrome dependencies
apt-get install -yq libpangocairo-1.0-0 libXcomposite1 libXcursor1 libXdamage1 \
    libXi6 libXtst6 libnss3 libcups2 libgconf2-4 libXss1 libXrandr2 \
    libatk1.0-0 libasound2 libgtk2.0-0
# Trace collection dependencies
apt-get install -yq xvfb

# Create a pythonapp user. The application will run as this user.
useradd -m -d /home/pythonapp pythonapp

# pip from apt is out of date, so make it update itself and install virtualenv.
pip install --upgrade pip virtualenv

# Download the Clovis deployment from Google Cloud Storage and unzip it.
# It is expected that the contents of the deployment have been generated using
# the tools/android/loading/cloud/backend/deploy.sh script.
CLOUD_STORAGE_PATH=`get_instance_metadata cloud-storage-path`
DEPLOYMENT_PATH=$CLOUD_STORAGE_PATH/deployment

mkdir -p /opt/app/clovis
gsutil cp gs://$DEPLOYMENT_PATH/source/source.tgz /opt/app/clovis/source.tgz
tar xvf /opt/app/clovis/source.tgz -C /opt/app/clovis
rm /opt/app/clovis/source.tgz

# Install app dependencies
virtualenv /opt/app/clovis/env
/opt/app/clovis/env/bin/pip install -r \
   /opt/app/clovis/src/tools/android/loading/cloud/backend/pip_requirements.txt

mkdir /opt/app/clovis/binaries
gsutil cp gs://$DEPLOYMENT_PATH/binaries/* /opt/app/clovis/binaries/
unzip /opt/app/clovis/binaries/linux.zip -d /opt/app/clovis/binaries/

# Install the Chrome sandbox
cp /opt/app/clovis/binaries/chrome_sandbox /usr/local/sbin/chrome-devel-sandbox
chown root:root /usr/local/sbin/chrome-devel-sandbox
chmod 4755 /usr/local/sbin/chrome-devel-sandbox

# Make sure the pythonapp user owns the application code
chown -R pythonapp:pythonapp /opt/app

# Create the configuration file for this deployment.
DEPLOYMENT_CONFIG_PATH=/opt/app/clovis/deployment_config.json
TASKQUEUE_TAG=`get_instance_metadata taskqueue_tag`
cat >$DEPLOYMENT_CONFIG_PATH << EOF
{
  "project_name" : "$PROJECTID",
  "cloud_storage_path" : "$CLOUD_STORAGE_PATH",
  "chrome_path" : "/opt/app/clovis/binaries/chrome",
  "src_path" : "/opt/app/clovis/src",
  "taskqueue_tag" : "$TASKQUEUE_TAG",
  "trace_database_filename" : "trace_database_${INSTANCE_ID}.json"
}
EOF

# Check if auto-start is enabled
AUTO_START=`get_instance_metadata auto-start`

# Exit early if auto start is not enabled.
if [ "$AUTO_START" == "false" ]; then
  exit 1
fi

# Configure supervisor to start the worker inside of our virtualenv.
cat >/etc/supervisor/conf.d/python-app.conf << EOF
[program:pythonapp]
directory=/opt/app/clovis/src/tools/android/loading/cloud/backend
command=python worker.py --config $DEPLOYMENT_CONFIG_PATH
autostart=true
autorestart=true
user=pythonapp
# Environment variables ensure that the application runs inside of the
# configured virtualenv.
environment=VIRTUAL_ENV="/opt/app/clovis/env", \
    PATH="/opt/app/clovis/env/bin:/usr/bin", \
    HOME="/home/pythonapp",USER="pythonapp", \
    CHROME_DEVEL_SANDBOX="/usr/local/sbin/chrome-devel-sandbox"
stdout_logfile=syslog
stderr_logfile=syslog
EOF

supervisorctl reread
supervisorctl update

