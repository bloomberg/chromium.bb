#===============================================================================
#
# lib/Win32/UTCFileTime.pm
#
# DESCRIPTION
#   Module providing functions to get/set UTC file times with stat/utime on
#   Win32.
#
# COPYRIGHT
#   Copyright (C) 2003-2008, 2012 Steve Hay.  All rights reserved.
#
# LICENCE
#   You may distribute under the terms of either the GNU General Public License
#   or the Artistic License, as specified in the LICENCE file.
#
#===============================================================================

package Win32::UTCFileTime;

use 5.006000;

use strict;
use warnings;

use Carp qw(croak);
use Exporter qw();
use XSLoader qw();

sub stat(;$);
sub lstat(;$);
sub alt_stat(;$);
sub utime(@);

#===============================================================================
# MODULE INITIALIZATION
#===============================================================================

our(@ISA, @EXPORT, @EXPORT_OK, $VERSION);

BEGIN {
    @ISA = qw(Exporter);
    
    @EXPORT = qw(
        stat
        lstat
        utime
    );
    
    @EXPORT_OK = qw(
        $ErrStr
        alt_stat
    );
    
    $VERSION = '1.55';

    XSLoader::load(__PACKAGE__, $VERSION);
}

# Last error message.
our $ErrStr = '';

# Control whether to try alt_stat() if CORE::stat() or CORE::lstat() fails.
# (Boolean.)
our $Try_Alt_Stat = 0;

#===============================================================================
# PUBLIC API
#===============================================================================

# Autoload the SEM_* flags from the constant() XS fuction.

sub AUTOLOAD {
    our $AUTOLOAD;

    # Get the name of the constant to generate a subroutine for.
    (my $constant = $AUTOLOAD) =~ s/^.*:://;

    # Avoid deep recursion on AUTOLOAD() if constant() is not defined.
    croak('Unexpected error in AUTOLOAD(): constant() is not defined')
        if $constant eq 'constant';

    my($error, $value) = constant($constant);

    # Handle any error from looking up the constant.
    croak($error) if $error;

    # Generate an in-line subroutine returning the required value.
    {
        no strict 'refs';
        *$AUTOLOAD = sub { return $value };
    }

    # Switch to the subroutine that we have just generated.
    goto &$AUTOLOAD;
}

# Specialized import() method to handle the ':globally' pseudo-symbol.
# This method is based on the import() method in the standard library module
# File::Glob (version 1.01).

sub import {
    my $i = 1;
    while ($i < @_) {
        # If the ':globally' pseudo-symbol is found in the list of symbols to
        # export then remove it and export stat(), lstat() and utime() to the
        # special CORE::GLOBAL package.
        if ($_[$i] =~ /^:globally$/io) {
            splice @_, $i, 1;
            {
                no warnings 'once';
                *CORE::GLOBAL::stat  = \&Win32::UTCFileTime::stat;
                *CORE::GLOBAL::lstat = \&Win32::UTCFileTime::lstat;
                *CORE::GLOBAL::utime = \&Win32::UTCFileTime::utime;
            }
            next;
        }
        $i++;
    }

    # Switch to Exporter's import() method to handle any remaining symbols to
    # export.
    goto &Exporter::import;
}

sub stat(;$) {
    my $file = @_ ? shift : $_;

    $ErrStr = '';

    # Make sure we do not display a message box asking the user to insert a
    # floppy disk or CD-ROM.
    my $old_umode = _set_error_mode(SEM_FAILCRITICALERRORS());

    if (wantarray) {
        my @stats = CORE::stat $file;

        unless (@stats) {
            _set_error_mode($old_umode);
            if ($Try_Alt_Stat) {
                goto &alt_stat;
            }
            else {
                $ErrStr = "Can't stat file '$file': $!";
                return;
            }
        }

        unless (@stats[8 .. 10] = _get_utc_file_times($file)) {
            _set_error_mode($old_umode);
            return;
        }

        _set_error_mode($old_umode);
        return @stats;
    }
    else {
        my $ret = CORE::stat $file;

        unless ($ret) {
            _set_error_mode($old_umode);
            if ($Try_Alt_Stat) {
                goto &alt_stat;
            }
            else {
                $ErrStr = "Can't stat file '$file': $!";
                return $ret;
            }
        }

        _set_error_mode($old_umode);
        return $ret;
    }
}

sub lstat(;$) {
    my $link = @_ ? shift : $_;

    $ErrStr = '';

    # Make sure we do not display a message box asking the user to insert a
    # floppy disk or CD-ROM.
    my $old_umode = _set_error_mode(SEM_FAILCRITICALERRORS());

    if (wantarray) {
        my @lstats = CORE::lstat $link;

        unless (@lstats) {
            _set_error_mode($old_umode);
            if ($Try_Alt_Stat) {
                goto &alt_stat;
            }
            else {
                $ErrStr = "Can't stat link '$link': $!";
                return;
            }
        }

        unless (@lstats[8 .. 10] = _get_utc_file_times($link)) {
            _set_error_mode($old_umode);
            return;
        }

        _set_error_mode($old_umode);
        return @lstats;
    }
    else {
        my $ret = CORE::lstat $link;

        unless ($ret) {
            _set_error_mode($old_umode);
            if ($Try_Alt_Stat) {
                goto &alt_stat;
            }
            else {
                $ErrStr = "Can't stat link '$link': $!";
                return $ret;
            }
        }

        _set_error_mode($old_umode);
        return $ret;
    }
}

sub alt_stat(;$) {
    my $file = @_ ? shift : $_;

    $ErrStr = '';

    # Make sure we do not display a message box asking the user to insert a
    # floppy disk or CD-ROM.
    my $old_umode = _set_error_mode(SEM_FAILCRITICALERRORS());

    if (wantarray) {
        my @stats = _alt_stat($file);

        _set_error_mode($old_umode);
        return @stats;
    }
    else {
        my $ret = _alt_stat($file);

        _set_error_mode($old_umode);
        return $ret;
    }
}

sub utime(@) {
    my($atime, $mtime, @files) = @_;

    $ErrStr = '';

    my $time = time;

    $atime = $time unless defined $atime;
    $mtime = $time unless defined $mtime;

    my $count = 0;
    foreach my $file (@files) {
        _set_utc_file_times($file, $atime, $mtime) and $count++;
    }

    return $count;
}

1;

__END__

#===============================================================================
# DOCUMENTATION
#===============================================================================

=head1 NAME

Win32::UTCFileTime - Get/set UTC file times with stat/utime on Win32

=head1 SYNOPSIS

    # Override built-in stat()/lstat()/utime() within current package only:
    use Win32::UTCFileTime qw(:DEFAULT $ErrStr);
    @stats = stat $file or die "stat() failed: $ErrStr\n";
    $now = time;
    utime $now, $now, $file;

    # Or, override built-in stat()/lstat()/utime() within all packages:
    use Win32::UTCFileTime qw(:globally);
    ...

    # Use an alternative implementation of stat() instead:
    use Win32::UTCFileTime qw(alt_stat $ErrStr);
    @stats = alt_stat($file) or die "alt_stat() failed: $ErrStr\n";

=head1 DESCRIPTION

This module provides replacements for Perl's built-in C<stat()> and C<utime()>
functions that respectively get and set "correct" UTC file times instead of the
erroneous values read and written by Microsoft's implementation of C<stat(2)>
and C<utime(2)>, which Perl's built-in functions inherit on Win32 when built
with the Microsoft C library.

For completeness, a replacement for Perl's built-in C<lstat()> function is also
provided, although in practice that is unimplemented on Win32 and just calls
C<stat()> anyway.  (Note, however, that it calls the I<original> C<stat()>, not
the override provided by this module, so you must use the C<lstat()> override
provided by this module if you want "correct" UTC file times from C<lstat()>.)

The problem with Microsoft's C<stat(2)> and C<utime(2)>, and hence Perl's
built-in C<stat()>, C<lstat()> and C<utime()> when built with the Microsoft C
library, is basically this: file times reported by C<stat(2)> or stored by
C<utime(2)> may change by an hour as we move into or out of daylight saving time
(DST) if the computer is set to "Automatically adjust clock for daylight saving
changes" (which is the default setting) and the file is stored on an NTFS volume
(which is the preferred filesystem used by Windows NT/2000/XP/2003).

It seems particularly ironic that the problem should afflict the NTFS filesystem
because the C<time_t> values used by both C<stat(2)> and C<utime(2)> express
UTC-based times, and NTFS stores file times in UTC.  However, Microsoft's
implementation of both of these functions use a variety of Win32 API calls that
mangle the numbers in ways that do not quite turn out right when a DST season
change is involved.  On FAT volumes, the filesystem used by Windows 95/98/ME,
file times are stored in local time and are put through even more contortions by
these functions, but actually emerge correctly, so file times are stable across
DST seasons on FAT volumes.  The NTFS/FAT difference is taken into account by
this module's replacement C<stat()>, C<lstat()> and C<utime()> functions so that
corrections are not erroneously applied when they should not be.

The problems that arise when mangling time values between UTC and local time are
because the mapping from UTC to local time is not one-to-one.  It is
straightforward to convert UTC to local time, but there is an ambiguity when
converting back from local time to UTC involving DST.  The Win32 API provides
two documented functions (C<FileTimeToLocalFileTime()> and
C<LocalFileTimeToFileTime()>) for these conversions that resolve the ambiguity
by, arguably "wrongly", using an algorithm involving the current system time
rather than the file time being converted to decide whether to apply a DST
correction; the advantage of this scheme is that these functions are exact
inverses.  Another, undocumented, function is also used internally by C<stat(2)>
for the tricky local time to UTC conversion, which, "correctly", uses the file
time being converted to decide whether to apply a DST correction.  The standard
C library provides C<localtime(3)> for UTC to local time conversion, albeit from
C<time_t> format to C<struct tm> format, (and also C<gmtime(3)> for the same
structure-conversion without converting to local time), and C<mktime(3)> for
local time to UTC conversion, applying a DST correction or not as instructed by
one of the fields in its C<struct tm> argument.

See L<"BACKGROUND REFERENCE"> for more details.

The replacement C<stat()> and C<lstat()> functions provided by this module
behave identically to Perl's built-in functions of the same name, except that:

=over 4

=item *

the last access time, last modification time and creation time return values are
all "correct" UTC-based values, stable across DST seasons;

=item *

the argument (or C<$_> if no argument is given) must be a file (path or name),
not a filehandle (and not the special filehandle consisting of an underscore
character either).

=back

In fact, both of these replacement functions work by calling Perl's
corresponding built-in function first and then overwriting the file time fields
in the lists thus obtained with the corrected values.  In this way, all of the
extra things done by Perl's built-in functions besides simply calling the
underlying C C<stat(2)> function are inherited by this module.

In obtaining these file time fields, these replacement functions actually
incorporate one slight improvement over the built-in functions (as of Perl
5.8.0): They work better on directories specified with trailing slashes or
backslashes under Windows NT platforms.

(As described in the L<"BACKGROUND REFERENCE"> section, the Microsoft C library
C<stat(2)> function, and hence Perl's built-in C<stat()> function, calls the
Win32 API function C<FindFirstFile()>.  That function is documented to fail on
directories that are specified with a trailing slash or backslash, so the
built-in function will not succeed.  It fact, it falls back on another Win32 API
function, C<GetFileAttributes()>, to set up the C<st_mode> field and then
returns success in such cases, but leaving the other fields set to zero.

The replacement functions call a different Win32 API function, C<CreateFile()>,
which can open directories specified with a trailing slash or backslash, but
only under Windows NT platforms.  The file time fields will thus be set
correctly by these replacement functions on Windows NT platforms.  (Under
Windows 95 platforms, they are set to zero and the functions succeed, as per the
Perl built-ins.)  Note, however, that the other fields, left over from the
original call to Perl's built-in C<stat()> or C<lstat()> function, will still be
zero.  For a complete alternative C<stat()> function that only uses
C<CreateFile()>, and will thus set all fields correctly even for directories
specified with a trailing slash or backslash, albeit only under Windows NT
platforms, use the C<alt_stat()> function.)

The replacement C<utime()> function provided by this module behaves identically
to Perl's built-in function of the same name, except that:

=over 4

=item *

the last access time and last modification time arguments are both "correctly"
interpreted as UTC-based values, stable across DST seasons;

=item *

no warnings about "Use of uninitialized value in utime" are produced when either
file time argument is specified as C<undef>.

=back

In particular, the one extra thing done by Perl's built-in function besides
simply calling the underlying C C<utime(2)> function (namely, providing a fix so
that it works on directories as well as files) is also incorporated into this
module's replacement function.

All three replacement functions are exported to the caller's package by default.
A special C<:globally> export pseudo-symbol is also provided that will export
all three functions to the CORE::GLOBAL package, which effectively overrides the
Perl built-in functions in I<all> packages, not just the caller's.  The
C<alt_stat()> function is only exported when explicitly requested.

=head2 Functions

=over 4

=item C<stat([$file])>

Gets the status information for the file $file.  If $file is omitted then C<$_>
is used instead.

In list context, returns the same 13-element list as Perl's built-in C<stat()>
function on success, or returns an empty list and sets $ErrStr on failure.

For convenience, here are the members of that 13-element list and their meanings
on Win32:

     0  dev      drive number of the disk containing the file (same as rdev)
     1  ino      not meaningful on Win32; always returned as 0
     2  mode     file mode (type and permissions)
     3  nlink    number of (hard) links to the file; always 1 on non-NTFS drives
     4  uid      numeric user ID of file's owner; always 0 on Win32
     5  gid      numeric group ID of file's owner; always 0 on Win32
     6  rdev     drive number of the disk containing the file (same as dev)
     7  size     total size of file, in bytes
     8  atime    last access time in seconds since the epoch
     9  mtime    last modification time in seconds since the epoch
    10  ctime    creation time in seconds since the epoch
    11  blksize  not implemented on Win32; returned as ''
    12  blocks   not implemented on Win32; returned as ''

where the epoch was at 00:00:00 Jan 01 1970 UTC and the drive number of the disk
is 0 for F<A:>, 1 for F<B:>, 2 for F<C:> and so on.

Because the mode contains both the file type (the C<S_IFDIR> bit is set if
$file specifies a directory; the C<S_IFREG> bit is set if the $file specifies a
regular file) and its permissions (the user read/write bits are set according to
the file's permission mode; the user execute bits are set according to the
filename extension), you should mask off the file type portion and C<printf()>
using a C<"%04o"> if you want to see the real permissions:

    $mode = (stat($filename))[2];
    printf "Permissions are %04o\n", $mode & 07777;

You can also import symbolic mode constants (C<S_IF*>) and functions
(C<S_IS*()>) from the Fcntl module to assist in examining the mode.  See
L<perlfunc/stat> for more details.

Note that you cannot use this module in conjunction with the File::stat module
(which provides a convenient, by-name, access mechanism to the fields of the
13-element list) because both modules operate by overriding Perl's built-in
C<stat()> function.  Only the second override to be applied would have effect.

In scalar context, returns a Boolean value indicating success or failure (and
sets $ErrStr on failure).

=item C<lstat([$link])>

Gets the status information for the symbolic link $link.  If $file is omitted
then C<$_> is used instead.  This is the same as C<stat()> on Win32, which
does not implement symbolic links.

=item C<alt_stat([$file])>

Gets the status information for the file $file.  If $file is omitted then C<$_>
is used instead.

Behaves almost identically to C<stat()> above, but uses this module's own
implementation of the standard C library C<stat(2)> function that can succeed in
some cases where Microsoft's implementation fails.

As mentioned in the main L<"DESCRIPTION"> above, Microsoft's C<stat(2)>, and
hence Perl's built-in C<stat()> and the replacement C<stat()> function above,
calls the Win32 API function C<FindFirstFile()>.  That function is used to
search a directory for a file, and thus requires the process to have "List
Folder Contents" permission on the directory containing the $file in question.
If that permission is denied then C<stat()> will fail.  It also has the
disadvantage mentioned above that it will fail on directories specified with a
trailing slash or backslash.

C<alt_stat()> avoids both of these problems by using a different Win32 API
function, C<CreateFile()>, instead.  That function opens a file directly and
hence does not require the process to have "List Folder Contents" permission on
the parent directory.  It can also open directories specified with trailing
slash or backslash, but only under Windows NT platforms.  B<In fact, under
Windows 95 platforms, it cannot open directories at all and will only set the
C<st_mode> field correctly;> the other fields will be set to zero, like the Perl
built-in C<stat()> and C<lstat()> functions do for directories specified with a
trailing slash or backslash.

The main disadvantage with using this function is that the entire C<struct stat>
has to be built by hand by it, rather than simply inheriting most of it from the
Microsoft C<stat(2)> call and then overwriting the file time fields.  Thus, some
of the fields, notably the C<st_mode> field, which is somewhat ambiguous on
Win32, may have different values to those that would have been set by the other
C<stat()> functions.

=item C<utime($atime, $mtime, @files)>

Sets the last access time and last modification time to the values specified by
$atime and $mtime respectively for each of the files in @files.  The process
must have write access to each of the files concerned in order to change these
file times.

Returns the number of files successfully changed and sets $ErrStr if one or more
files could not be changed.

The times should both be specified as the number of seconds since the epoch,
where the epoch was at 00:00:00 Jan 01 1970 UTC.  If the undefined value is used
for either file time argument then the current time will be used for that value.

Note that the 11th element of the 13-element list returned by C<stat()> is the
creation time on Win32, not the inode change time as it is on many other
operating systems.  Therefore, neither Perl's built-in C<utime()> function nor
this replacement function set that value to the current time as would happen on
other operating systems.

=back

=head2 Variables

=over 4

=item $ErrStr

Last error message.

If any function fails then a description of the last error will be set in this
variable for use in reporting the cause of the failure, much like the use of the
Perl Special Variables C<$!> and C<$^E> after failed system calls and Win32 API
calls.  Note that C<$!> and/or C<$^E> may also be set on failure, but this is
not always the case so it is better to check $ErrStr instead.  Any relevant
messages from C<$!> or C<$^E> will form part of the message in $ErrStr anyway.
See L<"Error Values"> for a listing of the possible values of $ErrStr.

If a function succeeds then this variable will be set to the null string.

=item $Try_Alt_Stat

Control whether to try C<alt_stat()> if C<CORE::stat()> or C<CORE::lstat()>
fails.

Boolean value.

As documented in the L<"DESCRIPTION"> section above, the replacement C<stat()>
and C<lstat()> functions each call their built-in counterparts first and then
overwrite the file time fields in the lists thus obtained with the corrected
values.  Setting this variable to a true value will cause the replacement
functions to switch to C<alt_stat()> (via a C<goto &NAME> call) if the
C<CORE::stat()> or C<CORE::lstat()> call fails.

The default value is 0, i.e. the C<alt_stat()> function is not tried.

=back

=head1 DIAGNOSTICS

=head2 Warnings and Error Messages

This module may produce the following diagnostic messages.  They are classified
as follows (a la L<perldiag>):

    (W) A warning (optional).
    (F) A fatal error (trappable).
    (I) An internal error that you should never see (trappable).

=over 4

=item Can't close file descriptor '%d' for file '%s': %s

(W) The specified file descriptor for the specified file could not be closed
after failing to obtain the associated operating-system file handle from it.
The system error message corresponding to the standard C library C<errno>
variable is also given.

=item Can't close file descriptor '%d' for file '%s' after updating: %s

(W) The specified file descriptor for the specified file could not be closed
after updating the file times using it.  The system error message corresponding
to the standard C library C<errno> variable is also given.

=item Can't close file object handle '%lu' for file '%s' after reading: %s

(W) The specified file object handle for the specified file could not be closed
after reading file information from it.  The system error message corresponding
to the Win32 API last error code is also given.

=item Can't close file object handle '%lu' for file '%s' after updating: %s

(W) The specified file object handle for the specified file could not be closed
after updating the file times using it.  The system error message corresponding
to the Win32 API last error code is also given.

=item Can't convert base SYSTEMTIME to FILETIME: %s

(I) The C<SYSTEMTIME> representation of the epoch of C<time_t> values (namely,
00:00:00 Jan 01 1970 UTC) could not be converted to its C<FILETIME>
representation.  The system error message corresponding to the Win32 API last
error code is also given.

=item Can't determine operating system platform: %s.  Assuming the platform is
Windows NT

(W) The operating system platform (i.e. Win32s, Windows (95/98/ME), Windows NT
or Windows CE) could not be determined.  This information is used by the
C<alt_stat()> function to decide whether a F<".cmd"> file extension represents
an "executable file" when setting up the C<st_mode> field of the C<struct stat>.
A Windows NT platform is assumed in this case.  The system error message
corresponding to the Win32 API last error code is also given.

=item %s is not a valid Win32::UTCFileTime macro

(F) You attempted to lookup the value of the specified constant in the
Win32::UTCFileTime module, but that constant is unknown to this module.

=item Overflow: Too many links (%lu) to file '%s'

(W) The number of hard links to the specified file is greater than the largest
C<short int>, and therefore cannot be assigned to the C<st_nlink> field of the
C<struct stat> set-up by C<alt_stat()>.  The largest C<short int> itself is used
instead in this case.

=item Unexpected error in AUTOLOAD(): constant() is not defined

(I) There was an unexpected error looking up the value of the specified
constant: the constant-lookup function itself is apparently not defined.

=item Unexpected return type %d while processing Win32::UTCFileTime macro %s

(I) There was an unexpected error looking up the value of the specified
constant: the C component of the constant-lookup function returned an unknown
type.

=item Your vendor has not defined Win32::UTCFileTime macro %s

(I) You attempted to lookup the value of the specified constant in the
Win32::UTCFileTime module, but that constant is apparently not defined.

=back

=head2 Error Values

Each function sets $ErrStr to a value indicating the cause of the error when
they fail.  The possible values are as follows:

=over 4

=item Can't get file information for file '%s': %s

File information could not be read from an open file handle on the specified
file.  The system error message corresponding to the Win32 API last error code
is also given.

=item Can't open file '%s' for reading: %s

The specified file could not be opened for reading the file information.  The
system error message corresponding to the Win32 API last error code is also
given.

=item Can't open file '%s' for updating: %s

The specified file could not be opened for updating the file times.  The system
error message corresponding to the Win32 API last error code is also given.

=item Can't set file times for file '%s': %s

The file times could not be updated on an open file handle on the specified
file.  The system error message corresponding to the Win32 API last error code
is also given.

=item Can't stat %s '%s': %s

The C<CORE::stat()> or C<CORE::lstat()> function failed for the specified file
or link.  (The replacement C<stat()> and C<lstat()> functions call their CORE
counterparts before getting the "correct" UTC file times.)  The system error
message corresponding to the standard C library C<errno> variable is also given.

=item Wildcard in filename '%s'

The specified filename passed to the C<alt_stat()> function contains a DOS-style
wildcard character, namely "?" or "*".  The function is not able to handle
filenames of this format.

=back

In some cases, the functions may also leave the Perl Special Variables C<$!>
and/or C<$^E> set to values indicating the cause of the error when they fail;
one or the other of these will be incorporated into the $ErrStr message in such
cases, as indicated above.  The possible values of each are as follows (C<$!>
shown first, C<$^E> underneath):

=over 4

=item EACCES (Permission denied)

=item ERROR_ACCESS_DENIED (Access is denied)

[C<utime()> only.]  One or more of the @files is read-only.  (The process must
have write access to each file to be able to update its file times.)

=item EMFILE (Too many open files)

=item ERROR_TOO_MANY_OPEN_FILES (The system cannot open the file)

The maximum number of file descriptors has been reached.  (Each file must be
opened in order to read file information from it or to update its file times.)

=item ENOENT (No such file or directory)

=item ERROR_FILE_NOT_FOUND (The system cannot find the file specified)

The filename or path in $file was not found.

=back

Note that since each function uses Win32 API functions rather than standard C
library functions, they will probably only set C<$^E> (which represents the
Win32 API last error code, as returned by C<GetLastError()>), not C<$!> (which
represents the standard C library C<errno> variable).

Other values may also be produced by various functions that are used within this
module whose possible error codes are not documented.

See L<C<$!>|perlvar/$!>, L<C<%!>|perlvar/%!>, L<C<$^E>|perlvar/$^E> and
L<Error Indicators|perlvar/"Error Indicators"> in L<perlvar>,
C<Win32::GetLastError()> and C<Win32::FormatMessage()> in L<Win32>, and L<Errno>
and L<Win32::WinError> for details on how to check these values.

=head1 BACKGROUND REFERENCE

A number of Microsoft Knowledge Base articles refer to the odd characteristics
of the Win32 API functions and the Microsoft C library functions involved, in
particular see:

=over 4

=item *

128126: FileTimeToLocalFileTime() Adjusts for Daylight Saving Time

=item *

129574: Time Stamp Changes with Daylight Savings

=item *

158588: Obtaining Universal Coordinated Time (UTC) from NTFS Files

=item *

190315: Some CRT File Functions Adjust For Daylight Savings Time

=back

As these articles themselves emphasize, the behaviour in question is by design,
not a bug.  As an aside, another Microsoft Knowledge Base article (214661: FIX:
Daylight Savings Time Bug in C Run-Time Library) refers to a different problem
involving the Microsoft C library that was confirmed as a bug and was fixed in
Visual Studio 6.0 Service Pack 3, so it is worth ensuring that your edition of
Visual Studio is upgraded to at least that Service Pack level when you build
Perl and this module.  (At the time of writing, Service Pack 6 is the latest
available for Visual Studio 6.0.)

An excellent overview of the problem with Microsoft's C<stat(2)> was written by
Jonathan M Gilligan and posted on the Code Project website
(F<http://www.codeproject.com/>).  He has kindly granted permission to use his
article here to describe the problem more fully.  A slightly edited version of
it now follows; the original article can be found at the URL
F<http://www.codeproject.com/datetime/dstbugs.asp>.

(The article was accompanied by a C library, adapted from code written for CVSNT
(F<http://www.cvsnt.org/>) by Jonathan and Tony M Hoyle, which implemented the
solution outlined at the end of his article.  The solution provided by this
module is partly based on that library and the original CVSNT code itself
(version 2.0.4), which both authors kindly granted permission to use under the
terms of the Perl Artistic License as well as the GNU GPL.)

=head2 Introduction

Not many Windows developers seem aware of it, but Microsoft deliberately
designed Windows NT to report incorrect file creation, modification, and access
times.  This decision is documented in the Knowledge Base in articles Q128126
and Q158588.  For most purposes, this behaviour is innocuous, but as Microsoft
writes in Q158588,

    After the automatic correction for Daylight Savings Time, monitoring
    programs comparing current time/date stamps to reference data that were not
    written using Win32 API calls which directly obtain/adjust to Universal
    Coordinated Time (UTC) will erroneously report time/date changes on files.
    Programs affected by this issue may include version-control software,
    database-synchronization software, software-distribution packages, backup
    software...

This behaviour is responsible for a flood of questions to the various support
lists for CVS, following the first Sunday in April and the last Sunday in
October, with scores of people complaining that CVS now reports erroneously that
their files have been modified.  This is commonly known as the "red file bug"
because the WinCVS shell uses red icons to indicate modified files.

Over the past two years, several people have made concerted efforts to fix this
bug and determine the correct file modification times for files both on NTFS and
FAT volumes.  It has proved surprisingly difficult to solve this problem
correctly.  I believe that I have finally gotten everything right and would like
to share my solution with anyone else who cares about this issue.

=head2 An example of the problem

Run the following batch file on a computer where F<C:> is an NTFS volume and
F<A:> is a FAT-formatted floppy disk.  You will need write access to F<C:\> and
F<A:\>.  This script will change your system time and date, so be prepared to
manually restore them afterwards.

    REM Test_DST_Bug.bat
    REM File Modification Time Test
    Date /T
    Time /T
    Date 10/27/2001
    Time 10:00 AM
    Echo Foo > A:\Foo.txt
    Time 10:30 AM
    Echo Foo > C:\Bar.txt
    dir A:\Foo.txt C:\Bar.txt
    Date 10/28/2001
    dir A:\Foo.txt C:\Bar.txt
    REM Prompt the user to reset the date and time.
    date
    time

The result looks something like this (abridged to save space)

    C:\>Date 10/27/2001
    C:\>dir A:\Foo.txt C:\Bar.txt

      Directory of A:\
    10/27/01  10:00a                     6 Foo.txt
      Directory of C:\
    10/27/01  10:30a                     6 Bar.txt

    C:\>Date 10/28/2001
    C:\>dir A:\Foo.txt C:\Bar.txt

      Directory of A:\
    10/27/01  10:00a                     6 Foo.txt
      Directory of C:\
    10/27/01  09:30a                     6 Bar.txt

On 27 October, Windows correctly reports that F<Bar.txt> was modified half an
hour after F<Foo.txt>, but the next day, Windows has changed its mind and
decided that actually, F<Bar.txt> was modified half an hour I<before>
F<Foo.txt>.  A naive programmer might think this was a bug, but as Microsoft
emphasized, I<this is how they want Windows to behave.>

=head2 Why Windows has this problem

The origin of this file time problem lies in the early days of MS-DOS and
PC-DOS.  Unix and other operating systems designed for continuous use and
network communications have long tended to store times in GMT (later UTC) format
so computers in different time zones can accurately determine the order of
different events.  However, when Microsoft adapted DOS for the IBM PC, the
personal computer was not envisioned in the context of wide-area networks, where
it would be important to compare the modification times of files on the PC with
those on another computer in another time zone.

In the interest of efficiently using the very limited resources of the computer,
Microsoft wisely decided not to waste bits or processor cycles worrying about
time zones.  To put this decision in context, recall that the first two
generations of PCs did not have battery-backed real-time clocks, so you would
generally put C<DATE> and C<TIME> commands into your F<AUTOEXEC.BAT> file to
prompt you to enter the date and time manually when the computer booted.

=head2 Digression on systems of measuring time

By the time of WinNT, wide-area networks and had become sufficiently common that
Microsoft realized that the OS should measure time in some universal format that
would allow different computers to compare the order (and separation) of events
irrespective of their particular time zones.  Although the details vary
(different time structures measure time relative to different events), the net
effect is that all times used internally in Win32 measure time with respect to
UTC (what used to be called GMT).

Having once worked down the hall from the master atomic clock array for the
United States at the National Institute of Standards and Technology in Boulder,
I feel obligated to add a few words about time and systems for reporting time.
Long ago, we used to refer time to GMT, or Greenwich Mean Time, which was kept
by the Royal Observatory in Greenwich, England and was ultimately referred to
the position of the sun as measured by the observatory.  When atomic clocks
became the standard for timekeeping, a new standard, called UTC emerged.  UTC is
a bastard acronym.  In English, it stands for "Coordinated Universal Time,"
while in French it stands for "le temps universel coordonne."  Rather than using
either CUT or TUC, the nonsense compromise acronym UTC was adopted.

To understand UTC, we must first understand the more abstract International
Atomic Time (TAI, le temps atomique international), which measures the number of
seconds that have elapsed since approximately 1 Jan 1958, as measured by caesium
atomic clocks.  The second is defined to be the amount of time required for 9
192 631 770 cycles of the caesium hyperfine frequency.  However, neither the day
nor the year are exact multiples of this number, so we take TAI and correct it
so that it corresponds to the actual motion of the earth by adding corrections
such as "leap seconds."  TAI measures raw atomic time.  UTC measures time
coordinated to the motion of the earth (i.e., so we don't end up having midnight
while the sun is shining or January in midsummer).  Details of what UTC really
means, together with a more detailed history of timekeeping, can be found at
F<http://ecco.bsee.swin.edu.au/chronos/GMT-explained.html>.

=head2 UTC, time zones, and Windows file times

So, what does this all have to do with file modification times on Windows
computers? Windows is stuck with some serious problems integrating FAT and NTFS
files compatibly.  FAT records file modification times with respect to the local
time zone, while NTFS records file modification (as well as creation and access
times, which FAT does not record) in UTC.  The first question you may want to
ask is, "How should Windows report these file times?"  Clearly, it would be
stupid for C<dir> and Windows Explorer to report FAT file times in the local
time zone and NTFS file times in UTC.  If inconsistent formats were used, users
would have great difficulty determining which of two files was more recent.  We
must thus choose to translate one of the two file time formats when we report to
the user.  Most users are likely to want to know the file modification time in
their local time zone.  This keeps things consistent with what people learned to
expect under DOS and Win16.  It also is more useful to most users, who may want
to know how long ago they modified a file without looking up the offset of their
local time zone from UTC.

It is straightforward to translate UTC to local time.  You look up the offset,
in minutes, between the local time zone and UTC, determine whether daylight
saving is in effect and add either the standard or the daylight offset to the
UTC time.  However, daylight time throws a subtle wrench in the works if we try
to go backwards...

=head2 The problem with daylight time

If you want to translate a time in your local time zone into UTC, it seems a
straightforward matter of determining whether daylight time is in effect locally
and then subtracting either the standard or the daylight offset from the local
time to arrive at UTC.  A subtle problem emerges due to the fact that the
mapping from UTC to local time is not one-to-one.  Specifically, when we leave
daylight saving time and set our clocks back, there are two distinct hour-long
intervals of UTC time that map onto the same hour-long interval of local time.
Consider the concrete case of 01:30 on the last Sunday in October.  Let's
suppose the local time zone is US Central Time (-6 hours offset from UTC when
daylight time is not in effect, -5 hours when it is).  At 06:00 UTC on Sunday 28
October 2001, the time in the US Central zone will be 01:00 and daylight time
will be in effect.  At 06:30 UTC, it will be 01:30 local.  At 07:00 UTC, it will
be 01:00 local and daylight time will not be in effect.  At 07:30 UTC, it will
be 01:30 local.  Thus, for all times 01:00 E<lt>= t E<lt> 02:00 local, there
will be two distinct UTC times that correspond to the given local time.  This
degenerate mapping means that we can't be sure which UTC time corresponds to
01:30 local time.  If a FAT file is marked as having been modified at 01:30 on
Oct 28 2001, we can't determine the UTC time.

When translating local file times to UTC and vice-versa, Microsoft made a
strange decision.  We would like to have the following code produce C<out_time>
equal to C<in_time>

    FILETIME in_time, local_time, out_time;

    // Assign in_time, then do this...

    FileTimeToLocalFileTime(&in_time,    &local_time);
    LocalFileTimeToFileTime(&local_time, &out_time  );

The problem is that if the local time zone is US Central (UTC-6 hours for
standard time, UTC-5 hours for daylight time) then C<in_time> = 06:30 Oct 28
2001 and C<in_time> = 07:30 Oct 28 2001 both map onto the same local time, 01:30
Oct 28 2001 and we don't know which branch to choose when we execute
C<LocalFileTimeToFileTime()>.  Microsoft picked an incorrect, but unambiguously
invertible algorithm: move all times up an hour when daylight time is in effect
on the local computer, irrespective of the DST state of the time being
converted.  Thus, if DST is in effect on my local computer,
C<FileTimeToLocalFileTime()> converts 06:30 Oct 28 2001 UTC to 01:30 CDT and
07:30 Oct 28 2001 UTC to 02:30 CDT.  If I call the same function with the same
arguments, but when DST is not in effect on my local computer,
C<FileTimeToLocalFileTime()> will convert 06:30 UTC to 00:30 CDT and 07:30 UTC
to 01:30 CDT.

It may seem strange that this would affect the C library call C<stat(2)>, which
allegedly returns the UTC modification time of a file.  If you examine the
source code for Microsoft's C library, you find that it gets the modification
time thus:

    // Pseudo-code listing

    WIN32_FIND_DATA find_buf;
    HANDLE hFile;
    FILETIME local_ft;
    SYSTEMTIME local_st;
    time_t mod_time;

    // FindFirstFile() returns times in UTC.
    // For NTFS files, it just returns the modification time stored on the disk.
    // For FAT files, it converts the modification time from local (which is
    // stored on the disk) to UTC using LocalFileTimeToFileTime().
    hFile = FindFirstFile(file_name, &find_buf);

    // Convert UTC time to local time.
    FileTimeToLocalFileTime(&find_buf.ftLastWriteTime, &local_ft);
    FileTimeToSystemTime(&local_ft, &local_st);

    // Now use a private, undocumented function to convert local time to UTC
    // time according to the DST settings appropriate to the time being
    // converted!
    mod_time = __loctotime_t(local_st);

For a FAT file, the conversions work like this:

=over 4

=item *

Raw file modification time (stored as local time) is converted to UTC by
C<LocalFileTimeToFileTime()>.

=item *

UTC is converted back to local time by C<FileTimeToLocalFileTime()>.  Note that
this exactly reverses the effect of the previous step, so we are left with the
correct local modification time.

=item *

Local time is converted to "correct" UTC by private function.

=back

For an NTFS file, the conversions work like this:

=over 4

=item *

Raw file modification time is already stored in UTC, so we don't need to convert
it. 

=item *

UTC is converted to local time by C<FileTimeToLocalFileTime()>.  This applies a
DST correction according to the DST setting of the I<current system time>,
irrespective of the DST setting at the file modification time. 

=item *

Local time is converted to "correct" UTC by private function.  Note that this
I<does not> necessarily reverse the effect of the previous step because in this
step we use the DST setting of the I<file modification time>, not the current
system time. 

=back

This explains the problem I showed at the top of this article: The time reported
by C<dir> for a file on an NTFS volume changes by an hour as we move into or out
of daylight saving time, despite the fact that the file hasn't been touched.
FAT modification times are stable across DST seasons.

=head2 Categorizing the problem

There are 3 possible ways I can think of where this inconsistency in reporting
file times may cause problems:

=over 4

=item *

You may be comparing a file on an NTFS volume with a C<time_t> value stored in a
file (or memory).  This is frequently seen in CVS and leads to the infamous
"red file bug" on the first Sunday in April and the last Sunday in October.

=item *

You may be comparing a file on a FAT volume with a C<time_t> value.

=item *

You may be comparing a file on a FAT volume with a file on an NTFS volume.

=back

=head2 Solutions

=over 4

=item *

For the first case, it's simple.  Get the file times using the Win32 API call
C<GetFileTime()> instead of using the C library C<stat(2)>, and convert the
C<FILETIME> to C<time_t> by subtracting the origin (00:00:00 Jan 01 1970 UTC)
and dividing by 10,000,000 to convert "clunks" (100-nanosecond intervals) to
seconds. 

=item *

For the second case, C<stat(2)> will work and return a C<time_t> that you can
compare to the stored one.  If you must use C<GetFileTime()> do not use
C<LocalFileTimeToFileTime()>.  This function will apply the daylight status of
the current system time, not the daylight status of the file time in the
argument.  Fortunately, the C library C<mktime(3)> function will correctly
convert the time I<if you correctly set the C<tm_isdst> field of the
C<struct tm>>.

There is a bit of a chicken-and-egg problem here.  Windows does not supply a
good API call to let you determine whether DST was in effect at a given time.
Fortunately for residents of the US and other countries that use the same logic
(Daylight time starts at 2:00 AM on the first Sunday in April and ends at 2:00
AM on the last Sunday in October), you can set C<tm_isdst> to a negative number
and C<mktime(3)> will automatically determine whether daylight time applies or
not.  If the file was modified in the window 1:00-2:00 AM on the last Sunday in
October, it's ambiguous how C<mktime(3)> computes the modification time.

People in time zones that do not follow the usual US daylight rule must
brute-force the daylight time problem by retrieving the applicable
C<TIME_ZONE_INFORMATION> structure with C<GetTimeZoneInformation()> and manually
calculating whether daylight time applies.

=item *

For the third case, the best bet is to follow the instructions for the second
case above and compare the resultant UTC C<time_t> with the time for the NTFS
file determined in the first case above.

=back

The library implements this solution with checking for the filesystem the file
is stored under. 

=head2 Further subtleties

That is the end of Jonathan M Gilligan's article.  It should be noted that
although the last section, L<"Solutions">, refers to the Win32 API function
C<GetFileTime()>, both his library and the CVSNT code that it was adapted from
use a different Win32 API function instead, namely, C<FindFirstFile()>.  As seen
in the pseudo-code listing above, that is the function used in Microsoft's
implementation of C<stat(2)> itself, and evidently has one advantage over
C<GetFileTime()> that is documented in the Microsoft Knowledge Base article
128126 cited above: C<GetFileTime()> gets I<cached> UTC times from FAT, whereas
C<FindFirstFile()> always reads the time from the file.  This means that the
value returned by C<GetFileTime()> may be incorrect under FAT after a DST season
change.

That should have been the end of the discussion about the problems with
Microsoft's implementation of C<stat(2)> and how to overcome them, and, indeed,
versions of this module before version 1.45 did simply implement one of the
solutions above (together with another enhancement to Jonathan's library, taken
from the CVSNT code, to apply the correct daylight saving time rule, rather than
assuming the United States' rule).

However, it came to my attention those older versions of this module did not
correctly handle file times on FAT due to a further subtlety in the Win32 API
function C<FindFirstFile()>.  Recall from above that on FAT, C<FindFirstFile()>
has to convert the local time stored on the disk to UTC returned by the
function, and that this conversion was believed to be performed incorrectly when
the file time concerned is in the opposite DST season to the current system
time.

This is easily seen by performing a test similar to that at start of Jonathan's
article above (L<"An example of the problem">).  Create a file called F<file>,
change the system time to the opposite DST season and then run the program
below to print out the file's last modification time:

    // test1.c
    #include <windows.h>
    #include <stdio.h>
    void main(void) {
        HANDLE hndl;
        WIN32_FIND_DATA wfd;
        SYSTEMTIME st;
        if ((hndl = FindFirstFile("file", &wfd)) != INVALID_HANDLE_VALUE &&
            FileTimeToSystemTime(&wfd.ftLastWriteTime, &st))
        {
            printf("Time: %02d/%02d/%04d %02d:%02d:%02d\n",
                   st.wDay, st.wMonth, st.wYear,
                   st.wHour, st.wMinute, st.wSecond);
            CloseHandle(hndl);
        }
    }

If this is done on an NTFS drive then the file time displayed will be the
correct UTC time at which the file was created (last modified).  If it is done
on a FAT drive then the file time displayed will be wrong by one hour.

That is fine, but notice that in the above sequence of events, the file time is
only ever set to the current system time.  The subtlety referred to above is
only seen when the file time was itself set from the opposite DST season.  This
is only possible programmatically; for example, the program below uses the Win32
API function C<SetFileTime()> to set the file time to 12:00 Jun 01 2005 UTC:

    // test2.c
    #include <windows.h>
    #include <stdio.h>
    void main(void) {
        SYSTEMTIME st = { 2005, 6, 0, 1, 12, 0, 0, 0 };
        FILETIME ft;
        HANDLE hndl;
        if (SystemTimeToFileTime(&st, &ft) &&
            (hndl = CreateFile("file", GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                0, NULL)) != INVALID_HANDLE_VALUE &&
            SetFileTime(hndl, NULL, &ft, &ft))
        {
            printf("File time has been set to 01/06/2005 12:00:00\n");
            CloseHandle(hndl);
        }
    }

Using a program such as the one above (and varying the file time that it sets as
necessary) a few simple tests show that the problem with C<FindFirstFile()> on a
FAT drive actually seems to be this: the file time retrieved is wrong by one
hour if it was B<set> in the opposite DST season to the current system time.

This, of course, includes the case where (as in <"An example of the problem">) a
file is created in one DST season and is then read from the opposite DST season
(for example, create a file in summer (which effectively sets a summer file time
from summer) and then read it from winter), but it also includes hitherto unseen
cases in which C<FindFirstFile()> will even return the wrong time for a file
time in the same DST season as the current system time if the file time was
B<set> from the opposite DST season to the current system time (for example, set
a summer file time from winter and then read it from summer).  Also note that,
in other thus-far unseen cases, C<FindFirstFile()> can return the B<correct>
time for a file time in the opposite DST season to the current system time if
the file time was B<set> from the same DST season as the current system time
(for example, set a winter file time from summer and then read it from summer).

Thus, we appear to be concerned with when the file time was B<set> relative to
the current system time, rather than what the file time B<is> relative to the
current system time.  This misunderstanding accounts for some of the test
failures that versions of this module before version 1.45 exhibited on a FAT
drive.

Of course, when a given file time was set is not something that can be known at
a later date because this information is not stored anywhere, which seems to
make a solution based on C<FindFirstFile()> impossible.  This leads us to look
to other Win32 API functions for a solution instead.

C<GetFileInformationByHandle()> was already being used in older versions of this
module in two places: in the replacement C<stat()> function as a fallback in the
cases (such as a directory specified with a trailing backslash) where
C<FindFirstFile()> fails, and in the alternative C<alt_stat()> function.  It was
assumed C<GetFileInformationByHandle()> performed in the same manner as
C<FindFirstFile()> with respect to how it retrieved and/or converted file times
on FAT and NTFS drives, and hence the same "corrections" were applied.  However,
further tests such as above with the program below shows that this is not true.

    // test3.c
    #include <windows.h>
    #include <stdio.h>
    void main(void) {
        HANDLE hndl;
        BY_HANDLE_FILE_INFORMATION bhfi;
        SYSTEMTIME st;
        if ((hndl = CreateFile("file", GENERIC_READ, FILE_SHARE_READ, NULL,
                OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL)) !=
                INVALID_HANDLE_VALUE &&
            GetFileInformationByHandle(hndl, &bhfi) &&
            FileTimeToSystemTime(&bhfi.ftLastWriteTime, &st))
    {
            printf("Time: %02d/%02d/%04d %02d:%02d:%02d\n",
                st.wDay, st.wMonth, st.wYear,
                st.wHour, st.wMinute, st.wSecond);
            CloseHandle(hndl);
        }
    }

Comparing the output of F<test1.c> and F<test3.c> for a test file with its file
time in each permutation of being B<set from> and being B<in> different DST
seasons in turn, on both FAT and NTFS drives, it appears that whilst F<test1.c>
exhibits the strange behaviour described above on FAT and only returns correct
UTC time reliably on NTFS, F<test3.c> always returns correct UTC time on both
FAT and NTFS.

Thus, the "corrections" that older versions of this module applied to the file
times retrieved by B<GetFileInformationByHandle()> should not have been applied,
which accounts for more of the test failures that those versions exhibited on
FAT drives.

=head2 The Final Solution

The final solution is thus alarmingly simple, and much simpler than that
implemented in the library accompanying Jonathan M Gilligan's article and in the
related CVSNT code: simply use C<GetFileInformationByHandle()> to retrieve the
last modification time.  It is always correct UTC time on both FAT and NTFS.

Another look at the source for Microsoft's C library shows that everything
written above regarding the last modification time of a file is also true of the
last access time and creation time, and simple modifications to the test
programs above also show that C<GetFileInformationByHandle()> always returns
them correctly too, so this module corrects those values as well in the same
way.

(Incidentally, the source code of Microsoft's implementation of C<stat(2)> can
be found in F<C:\Program Files\Microsoft Visual Studio\VC98\CRT\SRC\STAT.C> if
you installed Microsoft Visual C++ 6.0 in its default location and selected the
"VC++ Runtime Libraries -E<gt> CRT Source Code" option when installing.)

=head2 More problems: utime()

We have been looking at C<stat(2)> in the context of I<getting> various times
associated with files, so it is natural to wonder whether the complementary
function in this regard, namely C<utime(2)>, which I<sets> file times, is
afflicted in a similar way.

The answer, unfortunately, is yes.  A look at the source code of Microsoft's
implementation of C<utime(2)> shows that it puts the supplied last access time
and last modification time through similar, though slightly simpler, contortions
to those in C<stat(2)> before storing them in the filesystem.  Briefly, it goes
something like the following:

    // Pseudo-code listing

    time_t mod_time;
    struct tm *local_tmb;
    SYSTEMTIME local_st;
    FILETIME local_ft;
    FILETIME utc_ft;

    // Convert UTC time to local time.
    local_tmb = localtime(mod_time)

    // Convert local time to UTC time.
    // (Construct SYSTEMTIME from struct tm first.)
    SystemTimeToFileTime(&local_st, &local_ft);
    LocalFileTimeToFileTime(&local_ft, utc_ft)

    // Set UTC time.
    SetFileTime(hFile, utc_ft)

In versions of this module before version 1.45 it was believed that
C<SetFileTime()> called C<FileTimeToLocalFileTime()> on FAT drives, thus
cancelling out the effect of the C<LocalFileTimeToFileTime()> call and yielding
the correct result but in fact this is not the case, which accounts for the
remaining test failures that those versions exhibited on FAT drives.

Performing some tests using the following program to set various file times from
the same or opposite DST season as the file time being set and then reading
back the file times that were actually using F<test3.c> above (which is believed
always to return the correct file time), we can observe that F<test4.c> always
sets the file time wrongly (by one hour) if the file time being set is in the
opposite DST season to the current system time, and it always sets the file time
correctly if the file time being set is in the same DST season to the current
system time.  This is true on both FAT and NTFS drives.

    // test4.c
    #include <windows.h>
    #include <stdio.h>
    #include <time.h>
    #include <sys/utime.h>
    static BOOL _FileTimeToUnixTime(const FILETIME *ft, time_t *ut);
    void main(void) {
        SYSTEMTIME st = { 2005, 6, 0, 1, 12, 0, 0, 0 };
        FILETIME ft;
        time_t ut;
        struct utimbuf utb;
        if (SystemTimeToFileTime(&st, &ft) &&
            _FileTimeToUnixTime(&ft, &ut) &&
            (utb.actime = utb.modtime = ut) &&
            utime("file", &utb) != -1)
        {
            printf("File time has been set to 01/06/2005 12:00:00\n");
        }
    }
    static BOOL _FileTimeToUnixTime(const FILETIME *ft, time_t *ut) {
        SYSTEMTIME basest = { 1970, 1, 0, 1, 0, 0, 0, 0 };
        FILETIME baseft;
        ULARGE_INTEGER it;
        ULONGLONG clunks_per_second = 10000000L;
        if (!SystemTimeToFileTime(&basest, &baseft))
            return FALSE;
        it.QuadPart  = ((ULARGE_INTEGER *)ft)->QuadPart;
        it.QuadPart -= ((ULARGE_INTEGER *)&baseft)->QuadPart;
        it.QuadPart /= clunks_per_second;
        *ut = it.LowPart;
        return TRUE;
    }

The reason for this behaviour is simply that the C<localtime(3)> call in
Microsoft's implementation of C<utime(2)> shown above converts the time
correctly, while the later C<LocalFileTimeToFileTime()> call does not.  The
final <SetFileTime()> call then behaves correctly on both FAT and NTFS, just as
C<GetFileInformationByHandle()> was found to do further above.

We therefore have a situation slightly simpler than that for C<stat(2)>: under
both FAT and NTFS, two conversions are applied that are not the exact inverses
of each other, leaving the UTC file times supplied potentially wrong by one
hour.

Thus, the time set by C<utime(2)> for a file on either a FAT or an NTFS volume
is incorrect by an hour when the time being set is in a different DST season to
the current system time.

The solution to this problem implemented by this module is as simple as that to
the C<stat(2)> problem: just use the appropriate Win32 API function instead (in
this case, C<SetFileTime()>).

=head1 EXPORTS

The following symbols are, or can be, exported by this module:

=over 4

=item Default Exports

C<stat>,
C<lstat>,
C<utime>.

=item Optional Exports

C<alt_stat>,
C<$ErrStr>,
C<:globally>.

=item Export Tags

I<None>.

=back

=head1 KNOWN BUGS

I<None>.

=head1 CAVEATS

=over 4

=item *

Any of the Win32 API functions involved here, or indeed Microsoft's
implementation of C<stat(2)> and C<utime(2)> themselves, could, of course,
change in future Windows operating systems.  Such changes could render
inappropriate the corrections being applied by this module.  (In fact, the
Microsoft Knowledge Base article Q158588 cited above specifically mentions that
the behaviour of C<GetFileTime()> under FAT may be changed to match the
behaviour under NTFS in a future version of Windows NT.  That particular change,
however, would have no effect on this module because, as mentioned above,
C<GetFileTime()> is not used by it.)

Likewise, if corrections such as those applied by this module were ever
incorporated into the Perl core (so that Perl's built-in C<stat()>, C<lstat()>
and C<utime()> functions get/set correct UTC values themselves, even when built
on the faulty Microsoft C library functions) then again, the corrections applied
by this module would not be appropriate.

In either case, this module would either need updating appropriately, or may
even become redundant.

=back

=head1 FEEDBACK

Patches, bug reports, suggestions or any other feedback is welcome.

Bugs can be reported on the CPAN Request Tracker at
F<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Win32-UTCFileTime>.

Open bugs on the CPAN Request Tracker can be viewed at
F<http://rt.cpan.org/NoAuth/Bugs.html?Dist=Win32-UTCFileTime>.

Please test this distribution.  See CPAN Testers at F<http://testers.cpan.org/>
for details of how to get involved.

Previous test results on CPAN Testers can be viewed at
F<http://testers.cpan.org/search?request=dist&dist=Win32-UTCFileTime>.

Please rate this distribution on CPAN Ratings at
F<http://cpanratings.perl.org/rate/?distribution=Win32-UTCFileTime>.

=head1 SEE ALSO

L<perlfunc/stat>,
L<perlfunc/lstat>,
L<perlfunc/utime>;

L<File::stat>,
L<Win32::FileTime>.

=head1 ACKNOWLEDGEMENTS

Many thanks to Jonathan M Gilligan E<lt>jonathan.gilligan@vanderbilt.eduE<gt>
and Tony M Hoyle E<lt>tmh@nodomain.orgE<gt> who wrote much of the C code that
this module is based on and granted permission to use it under the terms of the
Perl Artistic License as well as the GNU GPL.  Extra thanks to Jonathan for also
granting permission to use his article describing the problem and his solution
to it in the L<"BACKGROUND REFERENCE"> section of this manpage.

Credit is also due to Slaven Rezic for finding Jonathan's work on the Code
Project website (F<http://www.codeproject.com/>) in response to my bug report
([perl #18513] on the Perl Bugs website, F<http://bugs.perl.org/>).

The custom C<import()> method is based on that in the standard library module
File::Glob (version 1.01), written by Nathan Torkington and others.

The C<alt_stat()> function is based on code taken from the C<wnt_stat()>
function in CVSNT (version 2.0.4) and the C<win32_stat()> and C<pp_stat()>
functions in Perl (version 5.8.0).

The C<_StrWinError()> function used by the XS code is based on the
C<win32_str_os_error()> function in Perl (version 5.8.5).

=head1 AVAILABILITY

The latest version of this module is available from CPAN (see
L<perlmodlib/"CPAN"> for details) at

F<http://www.cpan.org/authors/id/S/SH/SHAY/> or

F<http://www.cpan.org/modules/by-module/Win32/>.

=head1 INSTALLATION

See the F<INSTALL> file.

=head1 AUTHOR

Steve Hay E<lt>shay@cpan.orgE<gt>

=head1 COPYRIGHT

Copyright (C) 2003-2008, 2012 Steve Hay.  All rights reserved.

Portions Copyright (C) 2001 Jonathan M Gilligan.  Used with permission.

Portions Copyright (C) 2001 Tony M Hoyle.  Used with permission.

=head1 LICENCE

This module is free software; you can redistribute it and/or modify it under the
same terms as Perl itself, i.e. under the terms of either the GNU General Public
License or the Artistic License, as specified in the F<LICENCE> file.

=head1 VERSION

Version 1.55

=head1 DATE

20 Mar 2012

=head1 HISTORY

See the F<Changes> file.

=cut

#===============================================================================
