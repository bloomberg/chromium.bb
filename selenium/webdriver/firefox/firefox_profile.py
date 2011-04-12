# Copyright 2008-2011 WebDriver committers
# Copyright 2008-2011 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import tempfile
import os
import logging
import zipfile
import shutil
import re
import rdflib
import base64
from cStringIO import StringIO

WEBDRIVER_EXT = "webdriver.xpi"
EXTENSION_NAME = "fxdriver@googlecode.com"

class FirefoxProfile(object):

    ANONYMOUS_PROFILE_NAME   = "WEBDRIVER_ANONYMOUS_PROFILE"
    default_preferences = {
        "app.update.auto": "false",
        "app.update.enabled": "false",
        "browser.startup.page" : "0",
        "browser.download.manager.showWhenStarting": "false",
        "browser.EULA.override": "true",
        "browser.EULA.3.accepted": "true",
        "browser.link.open_external": "2",
        "browser.link.open_newwindow": "2",
        "browser.offline": "false",
        "browser.safebrowsing.enabled": "false",
        "browser.search.update": "false",
        "browser.sessionstore.resume_from_crash": "false",
        "browser.shell.checkDefaultBrowser": "false",
        "browser.tabs.warnOnClose": "false",
        "browser.tabs.warnOnOpen": "false",
        "browser.startup.page": "0",
        "startup.homepage_welcome_url": "\"about:blank\"",
        "devtools.errorconsole.enabled": "true",
        "dom.disable_open_during_load": "false",
        "dom.max_script_run_time": "30",
        "extensions.logging.enabled": "true",
        "extensions.update.enabled": "false",
        "extensions.update.notifyUser": "false",
        "network.manage-offline-status": "false",
        "network.http.max-connections-per-server": "10",
        "prompts.tab_modal.enabled": "false",
        "security.fileuri.origin_policy": "3",
        "security.fileuri.strict_origin_policy": "false",
        "security.warn_entering_secure": "false",
        "security.warn_submit_insecure": "false",
        "security.warn_entering_secure.show_once": "false",
        "security.warn_entering_weak": "false",
        "security.warn_entering_weak.show_once": "false",
        "security.warn_leaving_secure": "false",
        "security.warn_leaving_secure.show_once": "false",
        "security.warn_submit_insecure": "false",
        "security.warn_viewing_mixed": "false",
        "security.warn_viewing_mixed.show_once": "false",
        "signon.rememberSignons": "false",
        "toolkit.networkmanager.disable": "true",    
        "javascript.options.showInConsole": "true",
        "browser.dom.window.dump.enabled": "true",
        "webdriver_accept_untrusted_certs": "true",
        "dom.max_script_run_time": "30"
        }

    def __init__(self,profile_directory=None):
        """ Initialises a new instance of a Firefox Profile
            args:
                profile_directory: Directory of profile that you want to use. 
                                This defaults to None and will create a new
                                directory when object is created.
        """
        self.profile_dir = profile_directory
        if self.profile_dir is None:
            self.profile_dir = self._create_tempfolder()
        else:
            newprof = os.path.join(
                tempfile.gettempdir(), "webdriver-py-profilecopy")
            if os.path.exists(newprof):
                shutil.rmtree(newprof)
            shutil.copytree(self.profile_dir, newprof)
            self.profile_dir = newprof
            self._read_existing_userjs()
        self.extensionsDir = os.path.join(self.profile_dir, "extensions")
        self.userPrefs = os.path.join(self.profile_dir, "user.js")

    #Public Methods
    def set_preference(self, key, value): 
        """ sets the preference that we want in the profile."""
        self.default_preferences[key] = str(value)

    def add_extension(self, extension=WEBDRIVER_EXT):
        self._install_extension(extension)

    def update_preferences(self):
        self._write_user_prefs(self.default_preferences)
    
    #Properties

    @property
    def path(self):
        """ Gets the profile directory that is currently being used"""
        return self.profile_dir

    @property
    def port(self):
        """ Gets the port that WebDriver is working on"""
        return self._port

    @port.setter
    def port(self, port):
        """ Sets the port that WebDriver will be running on """
        self._port = port
        self.default_preferences["webdriver_firefox_port"] =  str(self._port)

    @property
    def accept_untrusted_certs(self):
        return bool(self.default_preferences["webdriver_accept_untrusted_certs"])

    @accept_untrusted_certs.setter
    def accept_untrusted_certs(self, value):
        self.default_preferences["webdriver_accept_untrusted_certs"] = str(value)

    @property
    def encoded(self):
        """
        A zipped, base64 encoded string of profile directory
        for use with remote WebDriver JSON wire protocol
        """
        fp = StringIO()
        zipped = zipfile.ZipFile(fp, 'w', zipfile.ZIP_DEFLATED)
        path_root = len(self.path) + 1 # account for trailing slash
        for base, dirs, files in os.walk(self.path):
            for fyle in files:
                filename = os.path.join(base, fyle)
                zipped.write(filename, filename[path_root:])
        zipped.close()
        return base64.encodestring(fp.getvalue())


    #Private Methods

    def _create_tempfolder(self):
        """ Creates a temp folder to store User.js and the extension """
        return tempfile.mkdtemp()

    def _write_user_prefs(self, user_prefs):
        """ writes the current user prefs dictionary to disk """
        f = open(self.userPrefs, "w") 
        for pref in user_prefs.keys():
            f.write('user_pref("%s", %s);\n' % (pref, user_prefs[pref]))

        f.close()

    def _read_existing_userjs(self):
        f = open(os.path.join(self.profile_dir, 'user.js'), "r")
        tmp_usr = f.readlines()
        f.close()
        for usr in tmp_usr:
            matches = re.search('user_pref\("(.*)",\s(.*)\)', usr)
            self.default_preferences[matches.group(1)] = matches.group(2)

    def _install_extension(self, extension):
        tempdir = tempfile.mkdtemp()
        ext_dir = "" 

        if extension == WEBDRIVER_EXT:
            extension = os.path.join(os.path.dirname(__file__), WEBDRIVER_EXT)
            ext_dir = os.path.join(self.extensionsDir, EXTENSION_NAME)
            
        
        xpi = zipfile.ZipFile(extension)

        #Get directories ready
        for file_in_xpi in xpi.namelist():
            name = file_in_xpi.replace("\\", os.path.sep).replace("/", os.path.sep)
            dest = os.path.join(tempdir, name)
            if (name.endswith(os.path.sep) and not os.path.exists(dest)):
                os.makedirs(dest)

        #Copy files
        for file_in_xpi in xpi.namelist():
            name = file_in_xpi.replace("\\", os.path.sep).replace("/", os.path.sep)
            dest = os.path.join(tempdir, name)
            if not (name.endswith(os.path.sep)):
                outfile = open(dest, 'wb')
                outfile.write(xpi.read(file_in_xpi))
                outfile.close()

        if ext_dir == "":
            installrdfpath = os.path.join(tempdir,"install.rdf")
            ext_dir = os.path.join(
                self.extensionsDir, self._read_id_from_install_rdf(installrdfpath))
        if os.path.exists(ext_dir):
            shutil.rmtree(ext_dir)
        shutil.copytree(tempdir, ext_dir)

    def _read_id_from_install_rdf(self, installrdfpath):
        from rdflib import Graph
        rdf = Graph()
        installrdf = rdf.parse(installrdfpath)
        id_ = ""
        for i in installrdf.all_nodes():
            if re.search(".*@.*\..*", i):
                return i.decode()
