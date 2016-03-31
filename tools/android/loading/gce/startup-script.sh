# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script executed at instance startup. It installs the required dependencies,
# downloads the source code, and starts a web server.

set -v

# Talk to the metadata server to get the project id
PROJECTID=$(curl -s \
    "http://metadata.google.internal/computeMetadata/v1/project/project-id" \
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
# the tools/android/loading/gce/deploy.sh script.
CLOUD_STORAGE_PATH=$(curl -s \
    "http://metadata/computeMetadata/v1/instance/attributes/cloud-storage-path" \
    -H "Metadata-Flavor: Google")
DEPLOYMENT_PATH=$CLOUD_STORAGE_PATH/deployment

mkdir -p /opt/app/clovis
gsutil cp gs://$DEPLOYMENT_PATH/source/source.tgz /opt/app/clovis/source.tgz
tar xvf /opt/app/clovis/source.tgz -C /opt/app/clovis
rm /opt/app/clovis/source.tgz

# Install app dependencies
virtualenv /opt/app/clovis/env
/opt/app/clovis/env/bin/pip install \
    -r /opt/app/clovis/src/tools/android/loading/gce/pip_requirements.txt

mkdir /opt/app/clovis/binaries
gsutil cp gs://$DEPLOYMENT_PATH/binaries/* /opt/app/clovis/binaries/
unzip /opt/app/clovis/binaries/linux.zip -d /opt/app/clovis/binaries/

# Install the Chrome sandbox
cp /opt/app/clovis/binaries/chrome_sandbox /usr/local/sbin/chrome-devel-sandbox
chown root:root /usr/local/sbin/chrome-devel-sandbox
chmod 4755 /usr/local/sbin/chrome-devel-sandbox
export CHROME_DEVEL_SANDBOX=/usr/local/sbin/chrome-devel-sandbox

# Make sure the pythonapp user owns the application code
chown -R pythonapp:pythonapp /opt/app

# Create the configuration file for this deployment.
DEPLOYMENT_CONFIG_PATH=/opt/app/clovis/deployment_config.json
cat >$DEPLOYMENT_CONFIG_PATH << EOF
{
  "project_name" : "$PROJECTID",
  "cloud_storage_path" : "$CLOUD_STORAGE_PATH",
  "chrome_path" : "/opt/app/clovis/binaries/chrome"
}
EOF

# Check if auto-start is enabled
AUTO_START=$(curl -s \
    "http://metadata/computeMetadata/v1/instance/attributes/auto-start" \
    -H "Metadata-Flavor: Google")

# TODO(droger): Figure out how to correctly restore check for auto-startup
# as well as auto-startup code.
exit 1

# Exit early if auto start is not enabled.
if [ -z "$AUTO_START" ]; then
  exit 1
fi

# Configure supervisor to start gunicorn inside of our virtualenv and run the
# applicaiton.
cat >/etc/supervisor/conf.d/python-app.conf << EOF
[program:pythonapp]
directory=/opt/app/clovis/src/tools/android/loading/gce
command=/opt/app/clovis/env/bin/gunicorn --workers=1 --bind 0.0.0.0:8080 \
    'main:StartApp('\"$DEPLOYMENT_CONFIG_PATH\"')'
autostart=true
autorestart=true
user=pythonapp
# Environment variables ensure that the application runs inside of the
# configured virtualenv.
environment=VIRTUAL_ENV="/opt/app/clovis/env",PATH="/opt/app/clovis/env/bin",\
    HOME="/home/pythonapp",USER="pythonapp"
stdout_logfile=syslog
stderr_logfile=syslog
EOF

supervisorctl reread
supervisorctl update

