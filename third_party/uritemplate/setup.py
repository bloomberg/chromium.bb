#!/usr/bin/env python

from distutils.core import setup
import uritemplate

base_url = "http://github.com/uri-templates/uritemplate-py/"

setup(
  name = 'uritemplate',
  version = uritemplate.__version__,
  description = 'URI Templates',
  author = 'Joe Gregorio',
  author_email = 'joe@bitworking.org',
  url = base_url,
  download_url = \
    '%starball/uritemplate-py-%s' % (base_url, uritemplate.__version__),
  packages = ['uritemplate'],
  provides = ['uritemplate'],
  long_description=open("README.rst").read(),
  install_requires = ['simplejson >= 2.5.0'],
  classifiers = [
    'Development Status :: 4 - Beta',
    'Intended Audience :: Developers',
    'License :: OSI Approved :: Apache Software License',
    'Programming Language :: Python',
    'Programming Language :: Python :: 2',
    'Programming Language :: Python :: 2.5',
    'Programming Language :: Python :: 2.6',
    'Programming Language :: Python :: 2.7',
    'Programming Language :: Python :: 3',
    'Programming Language :: Python :: 3.3',
    'Operating System :: POSIX',
    'Topic :: Internet :: WWW/HTTP',
    'Topic :: Software Development :: Libraries :: Python Modules',
  ]
)

