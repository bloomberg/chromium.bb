#!/usr/bin/perl -wT
#
# write-temp-file.cgi - creates a file and its temporary subdirectory
#
# The file's desired basename is passed in the filename query parameter.
# The file's desired contents are passed in the data query parameter.
#
# The newly created temporary file will be inside a uniquely-named
# subdirectory to prevent crosstalk between parallel test runs.  Once
# the test is done with the temporary file its <file_path> should be
# passed to delete-temp-file.cgi for cleanup.
#
# Must be called using the HTTP POST method.
#
# On success, outputs the following three-line plaintext UTF-8
# response:
#
#   OK
#   <file_path>
#
# Where <file_path> is the full file_path of the just-written
# temporary file suitable for use with wide-character Unicode APIs
# after UTF-8 decoding and UTF-16 reencoding.
#
# Any other output is an error message and indicates failure.
#
# Errors are redirected to stdout and UTF-8 encoded to aid diagnostics
# in the calling test suite.
#
# NOTE: This CGI allows writing files with filenames including
# non-ASCII Unicode characters, but note that Blink renderer crashes
# are expected on non-Windows hosts using such filenames when a
# non-UTF-8 locale is used. Use a UTF-8 locale to prevent this,
# e.g. with:
#
# bash$ export LC_ALL=en_US.UTF-8 LC_CTYPE=en_US.UTF-8 LANG=en_US.UTF-8
#
# Any other locale should work too provided it uses UTF-8 character
# encoding.
#
# Test failures are expected when relying on this CGI on Windows when
# using any system "ANSI" codepage ("ACP" below) other than
# windows-1252. To fix this, upgrade your system's Win32.pm Perl
# module to the latest version. Any version of Win32.pm since
# Win32-0.45 (released in 2012) should have the needed method:
# https://rt.cpan.org/Public/Bug/Display.html?id=78820
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
use Encode qw(encode decode is_utf8);
use File::Basename qw(dirname);
use File::Spec::Functions qw(catfile tmpdir);
use File::Temp qw(tempdir tempfile);
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
  $system_tmpdir =
    Win32::GetFullPathName(Win32::GetANSIPathName($system_tmpdir));
}

my $req = CGI->new;
if (uc 'post' ne $req->request_method) {
  die 'Wrong method: ' . $req->request_method;
}
my $basename = decode utf8 => $req->url_param('filename');
$basename =~ /\A([^\0- ]*)\z/s
  or die "untaint failed: $!";
$basename = $1;
my $data = decode utf8 => $req->url_param('data');

# Create a random-named subdirectory of the system temporary directory
# to hold the newly-created test file. The X's will be replaced with
# random printable characters by tempdir.
my $temp_cgi_subdir_template = 'LayoutTests_cgiXXXXX';
if ($win32) {
  # On win32 this name needs to remain unique even after 8.3 shortname
  # conversion so we drop the LayoutTests_ prefix.
  $temp_cgi_subdir_template = 'cgiXXXXX';
}
my $temp_cgi_subdir =
  tempdir($temp_cgi_subdir_template, DIR => $system_tmpdir);
my $file_path = catfile($temp_cgi_subdir, $basename);
if (dirname($file_path) ne $temp_cgi_subdir) {
  die(encode utf8 => "$file_path not in $temp_cgi_subdir");
}
my $tempfile_contents_write_handle;
my $tempfile;

# $file_path_acp_utf8 is a UTF-8-encoded representation of the
# fully-qualified "ACP" file file_path. $file_path_acp is a
# logically-equivalent ACP-encoded bytestring suitable for use in a
# (narrow-character) system call. These are only distinct on Win32.
my $file_path_acp = $file_path;
my $file_path_acp_utf8 = $file_path;
if ($win32) {

  # Win32::GetACP is a recently-added method and not yet available in
  # some installations. Fall back to windows-1252 when GetACP is
  # missing as that matches the ACP used by our Windows bots.
  my $win32_ansi_codepage = 'windows-1252';
  eval '$win32_ansi_codepage = "cp" . Win32::GetACP();';

  Win32::CreateFile($file_path)
    or die(encode utf8 => "CreateFile $file_path: $^E");
  $file_path_acp = Win32::GetFullPathName(Win32::GetANSIPathName($file_path));
  $file_path = Win32::GetLongPathName($file_path_acp);
  $file_path_acp_utf8 = $file_path_acp;
  if (!is_utf8($file_path_acp_utf8)) {
    $file_path_acp_utf8 = decode($win32_ansi_codepage, $file_path_acp_utf8);
  }
  if (!is_utf8($file_path)) {
    $file_path = decode($win32_ansi_codepage, $file_path);
  }
  open($tempfile_contents_write_handle, '>>', $file_path_acp)
    or die(encode utf8 => "open >> $file_path_acp_utf8: $!");
  $tempfile = $file_path_acp_utf8;
} else {
  ($tempfile_contents_write_handle, $tempfile) =
    tempfile(DIR => $temp_cgi_subdir);
}
binmode $tempfile_contents_write_handle, ':encoding(utf-8)';
print $tempfile_contents_write_handle $data;
close $tempfile_contents_write_handle
  or die(encode utf8 => "close $tempfile: $!");
if (!$win32) {
  rename($tempfile, $file_path)
    or die(encode utf8 => "rename $tempfile, $file_path: $!");
}

# Fail early to aid debugging when the newly-created file does not
# actually work with system calls or does not contain the correct
# bytes.
local $/ = undef;
my $tempfile_verification_read_handle;
open($tempfile_verification_read_handle, '<', $file_path_acp)
  or die(encode utf8 => "open $file_path_acp_utf8: $!");
binmode $tempfile_verification_read_handle, ':encoding(utf-8)';
my $data2 = <$tempfile_verification_read_handle>;
close($tempfile_verification_read_handle)
  or die(encode utf8 => "close $file_path_acp_utf8: $!");
if ($data ne $data2) {
  die(encode utf8 => "Expected $data but got $data2");
}

print "OK\n$file_path";
