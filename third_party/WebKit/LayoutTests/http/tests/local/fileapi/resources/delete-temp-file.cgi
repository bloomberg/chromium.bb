#!/usr/bin/perl -wT
#
# delete-temp-file.cgi - deletes a file and its temporary subdirectory
#
# The file's fully-qualified file path is passed in the filename query
# parameter.
#
# The file must have been previously created using
# write-temp-file.cgi, which creates files with test-chosen basenames
# inside random-named subdirectories of a system temporary directory,
# e.g. /tmp/LayoutTests_cgiXXXXX/name-chosen-by-test.txt or
# %LOCALAPPDATA%\Temp\cgiXXXXX\name-chosen-by-test.txt where XXXXX are
# random characters varying per write-temp-file.cgi run.
#
# Must be called using the HTTP POST method.
#
# Any output other than the string "OK" is an error message and
# indicates failure.
#
# Errors are redirected to stdout and UTF-8 encoded to aid diagnostics
# in the calling test suite.
#
# NOTE: "ACP" in this CGI refers to the Windows concept of "ANSI"
# Codepage, a (not actually ANSI-standardized) single- or double-byte
# non-Unicode ASCII-compatible character encoding used for some
# narrow-character Win32 APIs. It should not be confused with UTF-8
# (used for similar purposes on Linux, OS X, and other modern
# POSIX-like systems), nor with the usually-distinct OEM codepage
# used for other narrow-character Win32 APIs (for instance, console
# I/O and some filesystem data storage.)

use strict;
use warnings FATAL => 'all';
use CGI qw(-oldstyle_urls);
use Encode qw(encode decode);
use File::Basename qw(basename dirname);
use File::Spec::Functions qw(tmpdir);
use utf8;

my $win32 = eval 'use Win32; 1' ? 1 : 0;

open STDERR, '>&STDOUT';  # let the test see die() output
binmode STDOUT, ':encoding(utf-8)';
autoflush STDOUT 1;
autoflush STDERR 1;
print "content-type: text/plain; charset=utf-8\n\n";

# Finding a suitable system temporary directory should be as simple as
# $system_tmpdir = tmpdir() but Win32 Perl CGIs got unusable
# values. Tracked in https://crbug.com/786152

# Find the logical equivalent of /tmp - however extra contortions are
# needed on Win32 under Apache with an unpopulated environment to
# avoid choosing the root directory of the active drive by default.
my $local_appdata_temp = tmpdir();
if ($win32) {
  my $local_appdata = Win32::GetFolderPath(Win32::CSIDL_LOCAL_APPDATA());
  if (($local_appdata ne '') && -d "$local_appdata\\Temp") {
    $local_appdata_temp = "$local_appdata\\Temp";
  }
}

# This fallback order works well on fairly sane "vanilla" Win32, OS X
# and Linux Apache configurations with mod_perl.
my $system_tmpdir =
  $ENV{TMPDIR} || $ENV{TEMP} || $ENV{TMP} || $local_appdata_temp;
$system_tmpdir =~ /\A([^\0- ]*)\z/s
  or die "untaint failed: $!";
$system_tmpdir = $1;
if ($win32) {
  $system_tmpdir = Win32::GetANSIPathName($system_tmpdir);
  # Drive+directory path equality checks are performed in the 8.3
  # "shortname" space (and case-insensitively) to decrease the
  # likelihood of a false negative.
  #
  # ACP APIs return 8.3 names for some of the inputs currently used in
  # tests.
  $system_tmpdir = Win32::GetShortPathName($system_tmpdir);
}

my $req = CGI->new;
if (uc 'post' ne $req->request_method) {
  die 'Wrong method: ' . $req->request_method;
}

# $file_path is a UTF-8-encoded representation of the "ACP"
# file path. $file_path_acp is a logically-equivalent ACP-encoded
# bytestring suitable for use in a (narrow-character) system
# call. These are only distinct on Win32.
my $file_path = decode utf8 => $req->url_param('filename');
my $file_path_acp = $file_path;
if ($win32) {
  $file_path_acp = Win32::GetFullPathName(Win32::GetANSIPathName($file_path));
}
$file_path_acp =~ /\A([^\0- ]*)\z/s
  or die "untaint failed: $!";
$file_path_acp = $1;

# This should be a random-named subdirectory of the system temporary
# directory holding the test file to be deleted.
my $temp_cgi_subdir = dirname($file_path_acp);
if ($win32) {
  # Drive+directory path equality checks are performed in the 8.3
  # "shortname" space (and case-insensitively) to decrease the
  # likelihood of a false negative.
  #
  # ACP APIs return 8.3 names for some of the inputs currently used in
  # tests.
  $temp_cgi_subdir = Win32::GetShortPathName($temp_cgi_subdir);
}

# If the passed-in file path is actually one created by
# write-temp-file.cgi this will match $system_tmpdir computed from the
# environment.
my $file_path_implied_system_tmpdir = dirname($temp_cgi_subdir);

# This is the random-suffixed (LayoutTests_)?cgiXXXXX component.
my $temp_cgi_subdir_basename = basename($temp_cgi_subdir);

# The selected file must actually exist.
if (!-f $file_path_acp) {
  die(encode utf8 => "Can't reset $file_path: missing file");
}

# Security check: ensure the supplied fully-qualified file path is in a
# subdirectory of the system temporary directory.
if (uc($file_path_implied_system_tmpdir) ne uc($system_tmpdir)) {
  die(encode utf8 => ("Can't reset $file_path: " .
                      "$file_path_implied_system_tmpdir is not " .
                      "$system_tmpdir"));
}

# The system temporary directory must already exist.
if (!-d $system_tmpdir) {
  die(encode utf8 => "Can't reset $file_path: missing $system_tmpdir");
}

# Ensure the random-named subdirectory matches write-temp-file.cgi
# naming conventions.
if (!($temp_cgi_subdir_basename =~ /\A(LayoutTests_)?cgi\w+\z/i)) {
  die(encode utf8 => ("Can't reset $file_path: " .
                      "$temp_cgi_subdir_basename is not cgi*"));
}

# The random-named subdirectory must also already exist.
if (!-d $temp_cgi_subdir) {
  die(encode utf8 => ("Can't reset $file_path: " .
                      "missing $temp_cgi_subdir"));
}

# Delete the selected file.
unlink($file_path_acp)
  or die(encode utf8 => "unlink $file_path: $!");

# Delete its containing (LayoutTests_)?cgiXXXXX directory.
rmdir($temp_cgi_subdir)
  or die(encode utf8 => "rmdir $temp_cgi_subdir: $!");
print 'OK';
