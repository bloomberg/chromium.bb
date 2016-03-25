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

# Get the source code from the Google Cloud Repository.
# It is expected that the contents of this repository have been generated using
# the tools/android/loading/gce/deploy.sh script.
# git requires $HOME and it's not set during the startup script.
export HOME=/root
git config --global credential.helper gcloud.sh
git clone --depth 1 https://source.developers.google.com/p/$PROJECTID \
    /opt/app/clovis

# Install app dependencies
virtualenv /opt/app/clovis/env
/opt/app/clovis/env/bin/pip install \
    -r /opt/app/clovis/tools/android/loading/gce/pip_requirements.txt

# Download Chrome from Google Cloud Storage and unzip it.
# It is expected that the contents of the bucket have been generated using the
# tools/android/loading/gce/deploy.sh script.
STORAGE_BUCKET=`python - <<EOF
import json
config_file = "/opt/app/clovis/tools/android/loading/gce/server_config.json"
with open(config_file) as config:
  obj=json.load(config);
  print obj["bucket_name"]
EOF`

mkdir /opt/app/clovis/out
gsutil cp gs://$STORAGE_BUCKET/chrome/* /opt/app/clovis/out/
unzip /opt/app/clovis/out/linux.zip -d /opt/app/clovis/out/

# Install the Chrome sandbox
cp /opt/app/clovis/out/chrome_sandbox /usr/local/sbin/chrome-devel-sandbox
chown root:root /usr/local/sbin/chrome-devel-sandbox
chmod 4755 /usr/local/sbin/chrome-devel-sandbox
export CHROME_DEVEL_SANDBOX=/usr/local/sbin/chrome-devel-sandbox

# Make sure the pythonapp user owns the application code
chown -R pythonapp:pythonapp /opt/app

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
directory=/opt/app/clovis/tools/android/loading/gce
command=/opt/app/clovis/env/bin/gunicorn --workers=1 --bind 0.0.0.0:8080 \
    'main:StartApp("/opt/app/clovis/out/chrome")'
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

