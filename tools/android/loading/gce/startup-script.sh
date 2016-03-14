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
apt-get install -yq git supervisor python-pip python-dev libffi-dev libssl-dev

# Create a pythonapp user. The application will run as this user.
useradd -m -d /home/pythonapp pythonapp

# pip from apt is out of date, so make it update itself and install virtualenv.
pip install --upgrade pip virtualenv

# Get the source code from the Google Cloud Repository
# git requires $HOME and it's not set during the startup script.
export HOME=/root
git config --global credential.helper gcloud.sh
git clone https://source.developers.google.com/p/$PROJECTID /opt/app/clovis

# Install app dependencies
virtualenv /opt/app/clovis/env
/opt/app/clovis/env/bin/pip install -r /opt/app/clovis/pip_requirements.txt

# Make sure the pythonapp user owns the application code
chown -R pythonapp:pythonapp /opt/app

# Configure supervisor to start gunicorn inside of our virtualenv and run the
# applicaiton.
cat >/etc/supervisor/conf.d/python-app.conf << EOF
[program:pythonapp]
directory=/opt/app/clovis
command=/opt/app/clovis/env/bin/gunicorn --workers=1 main:app \
  --bind 0.0.0.0:8080
autostart=true
autorestart=true
user=pythonapp
# Environment variables ensure that the application runs inside of the
# configured virtualenv.
environment=VIRTUAL_ENV="/opt/app/env/clovis",PATH="/opt/app/clovis/env/bin",\
    HOME="/home/pythonapp",USER="pythonapp"
stdout_logfile=syslog
stderr_logfile=syslog
EOF

supervisorctl reread
supervisorctl update

