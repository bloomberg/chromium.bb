#!/bin/bash
# Simple shell script to get list of available hosts from the Chromoting
# Directory API using the saved AuthToken.
authToken=`sed -n 2p ~/.chromotingDirectoryAuthToken`
url="https://www.googleapis.com/chromoting/v1/@me/hosts"
curl --header "Content-Type: application/json" --header "Authorization: GoogleLogin Auth=$authToken" -L "$url"
echo
