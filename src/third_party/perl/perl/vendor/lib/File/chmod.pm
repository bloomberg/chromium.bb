package File::chmod;

use Carp;
use strict;
use vars qw(
  $VERSION @ISA @EXPORT @EXPORT_OK $DEBUG
  $UMASK $MASK $VAL $W $MODE
);

require Exporter;

@ISA = qw( Exporter );
@EXPORT = qw( chmod getchmod );
@EXPORT_OK = qw( symchmod lschmod getsymchmod getlschmod getmod );

$VERSION = '0.32';
$DEBUG = 1;
$UMASK = 1;
$MASK = umask;

my ($SYM,$LS) = (1,2);
my %ERROR = (
  EDETMOD => "use of determine_mode is deprecated",
  ENEXLOC => "cannot set group execute on locked file",
  ENLOCEX => "cannot set file locking on group executable file",
  ENSGLOC => "cannot set-gid on locked file",
  ENLOCSG => "cannot set file locking on set-gid file",
  ENEXUID => "execute bit must be on for set-uid",
  ENEXGID => "execute bit must be on for set-gid",
  ENULSID => "set-id has no effect for 'others'",
  ENULSBG => "sticky bit has no effect for 'group'",
  ENULSBO => "sticky bit has no effect for 'others'",
);


sub getmod {
  my @return = map { (stat)[2] & 07777 } @_;
  return wantarray ? @return : $return[0];
}


sub chmod {
  my $mode = shift;
  my $how = mode($mode);

  return symchmod($mode,@_) if $how == $SYM;
  return lschmod($mode,@_) if $how == $LS;
  return CORE::chmod($mode,@_);
}


sub getchmod {
  my $mode = shift;
  my $how = mode($mode);

  return getsymchmod($mode,@_) if $how == $SYM;
  return getlschmod($mode,@_) if $how == $LS;
  return wantarray ? (($mode) x @_) : $mode;
}


sub symchmod {
  my $mode = shift;
  my @return = getsymchmod($mode,@_);
  my $ret = 0;
  for (@_){ $ret++ if CORE::chmod(shift(@return),$_) }
  return $ret;
}


sub getsymchmod {
  my $mode = shift;
  my @return;

  croak "symchmod received non-symbolic mode: $mode" if mode($mode) != $SYM;

  for (@_){
    local $VAL = getmod($_);

    for my $this (split /,/, $mode){
      local $W = 0;
      my $or;

      for (split //, $this){
        if (not defined $or and /[augo]/){
          /a/ and $W |= 7, next;
          /u/ and $W |= 1, next;
          /g/ and $W |= 2, next;
          /o/ and $W |= 4, next;
        }

        if (/[-+=]/){
          $W ||= 7;
          $or = (/[=+]/ ? 1 : 0);
          clear() if /=/;
          next;
        }

        croak "Bad mode $this" if not defined $or;
        croak "Unknown mode: $mode" if !/[ugorwxslt]/;

        /u/ and $or ? u_or() : u_not();
        /g/ and $or ? g_or() : g_not();
        /o/ and $or ? o_or() : o_not();
        /r/ and $or ? r_or() : r_not();
        /w/ and $or ? w_or() : w_not();
        /x/ and $or ? x_or() : x_not();
        /s/ and $or ? s_or() : s_not();
        /l/ and $or ? l_or() : l_not();
        /t/ and $or ? t_or() : t_not();
      }
    }
    $VAL &= ~$MASK if $UMASK;
    push @return, $VAL;
  }
  return wantarray ? @return : $return[0];
}


sub lschmod {
  my $mode = shift;
  return CORE::chmod(getlschmod($mode,@_),@_);
}


sub getlschmod {
  my $mode = shift;
  my $VAL = 0;

  croak "lschmod received non-ls mode: $mode" if mode($mode) != $LS;

  my ($u,$g,$o) = ($mode =~ /^.(...)(...)(...)$/);

  for ($u){
    $VAL |= 0400 if /r/;
    $VAL |= 0200 if /w/;
    $VAL |= 0100 if /[xs]/;
    $VAL |= 04000 if /[sS]/;
  }

  for ($g){
    $VAL |= 0040 if /r/;
    $VAL |= 0020 if /w/;
    $VAL |= 0010 if /[xs]/;
    $VAL |= 02000 if /[sS]/;
  }

  for ($o){
    $VAL |= 0004 if /r/;
    $VAL |= 0002 if /w/;
    $VAL |= 0001 if /[xt]/;
    $VAL |= 01000 if /[Tt]/;
  }

  return wantarray ? (($VAL) x @_) : $VAL;
}


sub mode {
  my $mode = shift;
  return 0 if $mode !~ /\D/;
  return $SYM if $mode =~ /[augo=+,]/;
  return $LS if $mode =~ /^.([r-][w-][xSs-]){2}[r-][w-][xTt-]$/;
  return $SYM;
}


sub determine_mode {
  warn $ERROR{EDECMOD};
  mode(@_);
}


sub clear {
  $W & 1 and $VAL &= 02077;
  $W & 2 and $VAL &= 05707;
  $W & 4 and $VAL &= 07770;
}
  

sub u_or {
  my $val = $VAL;
  $W & 2 and ($VAL |= (($val & 0700)>>3 | ($val & 04000)>>1));
  $W & 4 and ($VAL |= (($val & 0700)>>6));
}


sub u_not {
  my $val = $VAL;
  $W & 1 and $VAL &= ~(($val & 0700) | ($val & 05000));
  $W & 2 and $VAL &= ~(($val & 0700)>>3 | ($val & 04000)>>1);
  $W & 4 and $VAL &= ~(($val & 0700)>>6);
}


sub g_or {
  my $val = $VAL;
  $W & 1 and $VAL |= (($val & 070)<<3 | ($val & 02000)<<1);
  $W & 4 and $VAL |= ($val & 070)>>3;
}


sub g_not {
  my $val = $VAL;
  $W & 1 and $VAL &= ~(($val & 070)<<3 | ($val & 02000)<<1);
  $W & 2 and $VAL &= ~(($val & 070) | ($val & 02000));
  $W & 4 and $VAL &= ~(($val & 070)>>3);
}


sub o_or {
  my $val = $VAL;
  $W & 1 and $VAL |= (($val & 07)<<6);
  $W & 2 and $VAL |= (($val & 07)<<3);
}


sub o_not {
  my $val = $VAL;
  $W & 1 and $VAL &= ~(($val & 07)<<6);
  $W & 2 and $VAL &= ~(($val & 07)<<3);
  $W & 4 and $VAL &= ~($val & 07);
}


sub r_or {
  $W & 1 and $VAL |= 0400;
  $W & 2 and $VAL |= 0040;
  $W & 4 and $VAL |= 0004;
}


sub r_not {
  $W & 1 and $VAL &= ~0400;
  $W & 2 and $VAL &= ~0040;
  $W & 4 and $VAL &= ~0004;
}


sub w_or {
  $W & 1 and $VAL |= 0200;
  $W & 2 and $VAL |= 0020;
  $W & 4 and $VAL |= 0002;
}


sub w_not {
  $W & 1 and $VAL &= ~0200;
  $W & 2 and $VAL &= ~0020;
  $W & 4 and $VAL &= ~0002;
}


sub x_or {
  if ($VAL & 02000){ $DEBUG and warn($ERROR{ENEXLOC}), return }
  $W & 1 and $VAL |= 0100;
  $W & 2 and $VAL |= 0010;
  $W & 4 and $VAL |= 0001;
}


sub x_not {
  $W & 1 and $VAL &= ~0100;
  $W & 2 and $VAL &= ~0010;
  $W & 4 and $VAL &= ~0001;
}


sub s_or {
  if ($VAL & 02000){ $DEBUG and warn($ERROR{ENSGLOC}), return }
  if (not $VAL & 00100){ $DEBUG and warn($ERROR{ENEXUID}), return }
  if (not $VAL & 00010){ $DEBUG and warn($ERROR{ENEXGID}), return }
  $W & 1 and $VAL |= 04000;
  $W & 2 and $VAL |= 02000;
  $W & 4 and $DEBUG and warn $ERROR{ENULSID};
}


sub s_not {
  $W & 1 and $VAL &= ~04000;
  $W & 2 and $VAL &= ~02000;
  $W & 4 and $DEBUG and warn $ERROR{ENULSID};
}


sub l_or {
  if ($VAL & 02010){ $DEBUG and warn($ERROR{ENLOCSG}), return }
  if ($VAL & 00010){ $DEBUG and warn($ERROR{ENLOCEX}), return }
  $VAL |= 02000;
}


sub l_not {
  $VAL &= ~02000 if not $VAL & 00010;
}


sub t_or {
  $W & 1 and $VAL |= 01000;
  $W & 2 and $DEBUG and warn $ERROR{ENULSBG};
  $W & 4 and $DEBUG and warn $ERROR{ENULSBO};
}


sub t_not {
  $W & 1 and $VAL &= ~01000;
  $W & 2 and $DEBUG and warn $ERROR{ENULSBG};
  $W & 4 and $DEBUG and warn $ERROR{ENULSBO};
}


1;

__END__

=head1 NAME

File::chmod - Implements symbolic and ls chmod modes

=head1 VERSION

This is File::chmod v0.32.

=head1 SYNOPSIS

  use File::chmod;

  # chmod takes all three types
  # these all do the same thing
  chmod(0666,@files);
  chmod("=rw",@files);
  chmod("-rw-rw-rw-",@files);

  # or

  use File::chmod qw( symchmod lschmod );

  chmod(0666,@files);		# this is the normal chmod
  symchmod("=rw",@files);	# takes symbolic modes only
  lschmod("-rw-rw-rw-",@files);	# takes "ls" modes only

  # more functions, read on to understand

=head1 DESCRIPTION

File::chmod is a utility that allows you to bypass system calls or bit
processing of a file's permissions.  It overloads the chmod() function
with its own that gets an octal mode, a symbolic mode (see below), or
an "ls" mode (see below).  If you wish not to overload chmod(), you can
export symchmod() and lschmod(), which take, respectively, a symbolic
mode and an "ls" mode.

Symbolic modes are thoroughly described in your chmod(1) man page, but
here are a few examples.

  # NEW: if $UMASK is true, symchmod() applies a bit-mask found in $MASK

  chmod("+x","file1","file2");	# overloaded chmod(), that is...
  # turns on the execute bit for all users on those two files

  chmod("o=,g-w","file1","file2");
  # removes 'other' permissions, and the write bit for 'group'

  chmod("=u","file1","file2");
  # sets all bits to those in 'user'

"ls" modes are the type produced on the left-hand side of an C<ls -l> on a
directory.  Examples are:

  chmod("-rwxr-xr-x","file1","file2");
  # the 0755 setting; user has read-write-execute, group and others
  # have read-execute priveleges

  chmod("-rwsrws---","file1","file2");
  # sets read-write-execute for user and group, none for others
  # also sets set-uid and set-gid bits

The regular chmod() and lschmod() are absolute; that is, they are not
appending to or subtracting from the current file mode.  They set it,
regardless of what it had been before.  symchmod() is useful for allowing
the modifying of a file's permissions without having to run a system call
or determining the file's permissions, and then combining that with whatever
bits are appropriate.  It also operates separately on each file.

An added feature to version 0.30 is the $UMASK variable, explained below; if
symchmod() is called and this variable is true, then the function uses the
(also new) $MASK variable (which defaults to umask()) as a mask against the
new mode.  This is documented below more clearly.

=head2 Functions

Exported by default:

=over 4

=item chmod(MODE,FILES)

Takes an octal, symbolic, or "ls" mode, and then chmods each file
appropriately.

=item getchmod(MODE,FILES)

Returns a list of modified permissions, without chmodding files.
Accepts any of the three kinds of modes.

  @newmodes = getchmod("+x","file1","file2");
  # @newmodes holds the octal permissons of the files'
  # modes, if they were to be sent through chmod("+x"...)

=back

Exported by request:

=over 4

=item symchmod(MODE,FILES)

Takes a symbolic permissions mode, and chmods each file.

=item lschmod(MODE,FILES)

Takes an "ls" permissions mode, and chmods each file.

=item getsymchmod(MODE,FILES)

Returns a list of modified permissions, without chmodding files.
Accepts only symbolic permisson modes.

=item getlschmod(MODE,FILES)

Returns a list of modified permissions, without chmodding files.
Accepts only "ls" permisson modes.

=item getmod(FILES)

Returns a list of the current mode of each file.

=back

=head2 Variables

=over 4

=item $File::chmod::DEBUG

If set to a true value, it will report warnings, similar to those produced
by chmod() on your system.  Otherwise, the functions will not report errors.
Example: a file can not have file-locking and the set-gid bits on at the
same time.  If $File::chmod::DEBUG is true, the function will report an
error.  If not, you are not warned of the conflict.  It is set to 1 as
default.

=item $File::chmod::MASK

Contains the umask to apply to new file modes when using getsymchmod().  This
defaults to the return value of umask() at compile time.  Is only applied if
$UMASK is true.

=item $File::chmod::UMASK

This is a boolean which tells getsymchmod() whether or not to apply the umask
found in $MASK.  It defaults to true.

=back

=head1 REVISIONS

I<Note: this section was started with version 0.30.>

This is an in-depth look at the changes being made from version to version.

=head2 0.31 to 0.32

=over 4

=item B<license added>

I added a license to this module so that it can be used places without asking
my permission.  Sorry, Adam.

=back

=head2 0.30 to 0.31

=over 4

=item B<fixed getsymchmod() bug>

Whoa.  getsymchmod() was doing some crazy ish.  That's about all I can say.
I did a great deal of debugging, and fixed it up.  It ALL had to do with two
things:

  $or = (/+=/ ? 1 : 0); # should have been /[+=]/

  /u/ && $ok ? u_or() : u_not(); # should have been /u/ and $ok

=item B<fixed getmod() bug>

I was using map() incorrectly in getmod().  Fixed that.

=item B<condensed lschmod()>

I shorted it up, getting rid a variable.

=back

=head2 0.21 to 0.30

=over 4

=item B<added umask() honoring for symchmod()>

The symchmod() function now honors the $UMASK and $MASK variables.  $UMASK is
a boolean which indicates whether or not to honor the $MASK variable.  $MASK
holds a umask, and it defaults to umask().  $UMASK defaults to true.  These
variables are NOT exported.  They must explictly set (i.e. $File::chmod::UMASK
= 0).

=item B<function name changes>

Renamed internal function determine_mode() to mode().  However, if you happen
to be using determine_mode() somewhere, mode() will be called, but you'll also
get a warning about deprecation.

Renamed internal functions {or,not}_{l,s,t} to {l,s,t}_{or,not}.  This is to
keep in standard with the OTHER 6 pairs of bitwise functions, such as r_or()
and g_not().  I don't know WHY the others had 'not' or 'or' in the front.

=item B<fixed debugging bugs>

Certain calls to warn() were not guarded by the $DEBUG variable, and now they
are.  Also, for some reason, I left a debugging check (that didn't check to
see if $DEBUG was true) in getsymchmod(), line 118.  It printed "ENTERING /g/".
It's gone now.

=item B<fixed set-uid and set-gid bug>

Heh, it seems that in the previous version of File::chmod, the following code
went along broken:

  # or_s sub, File/chmod.pm, v0.21, line 330
  ($VAL & 00100) && do {
    $DEBUG && warn("execute bit must be on for set-uid"); 1;
  } && next;

Aside from me using '&&' more than enough (changed in the new code), this is
broken.  This is now fixed.

=item B<fixed file lock/set-gid bug>

The not_l() function (now renamed to l_not()) used to take the file mode and
bit-wise NOT it with ~02000.  However, it did not check if the file was locked
vs. set-gid.  Now, the function is C<$VAL &= ~02000 if not $VAL & 00010;>.

=item B<removed useless data structures>

I do not know why I had the $S variable, or %r, %w, and %x hashes.  In fact,
$S was declared in C<use vars qw( ... );>, but never given a value, and the
%r, %w, and %x hashes had a 'full' key which never got used.  And the hashes
themselves weren't really needed anyway.  Here is a list of the variables no
longer in use, and what they have been replaced with (if any):

  $S		nothing
  $U, $G, $O	$W
  %r, %w, %x	octal numbers
  @files	@_ (I had @files = @_; in nearly EVERY sub)
  $c		$_

=item B<compacted code>

The first version of File::chmod that was published was 0.13, and it was
written in approximately 10 days, being given the off-and-on treatment I end
up having to give several projects, due to more pressing matters.  Well, since
then, most of the code has stayed the same, although bugs were worked out.
Well, I got rid of a lot of slow, clunky, and redundant sections of code in
this version.  Sections include the processing of each character of the mode
in getsymchmod(), the getmod() subroutine, um, nearly ALL of the getsymchmod()
function, now that I look at it.

Here's part of the getsymchmod() rewrite:

  for ($c){
    if (/u/){
      u_or() if $MODE eq "+" or $MODE eq "=";
      u_not() if $MODE eq "-";
    }
  ...
  }

  # changed to

  /u/ && $or ? u_or() : u_and();
  # note: operating on $_, $c isn't used anymore
  # note: $or holds 1 if the $MODE was + or =, 0 if $MODE was -
  # note: previous was redundant.  didn't need $MODE eq "-" check
  #       because u_or() and u_not() both go to the next character

=back

=head1 PORTING

This is only good on Unix-like boxes.  I would like people to help me work on
File::chmod for any OS that deserves it.  If you would like to help, please
email me (address below) with the OS and any information you might have on how
chmod() should work on it; if you don't have any specific information, but
would still like to help, hey, that's good too.  I have the following
information (from L</perlport>):

=over 4

=item Win32

Only good for changing "owner" read-write access, "group", and "other" bits
are meaningless.  I<NOTE: Win32::File and Win32::FileSecurity already do
this.  I do not currently see a need to port File::chmod.>

=item MacOS

Only limited meaning. Disabling/enabling write permission is mapped to
locking/unlocking the file.

=item RISC OS

Only good for changing "owner" and "other" read-write access.

=back

=head1 AUTHOR

Jeff C<japhy> Pinyan, F<japhy.734+CPAN@gmail.com>, CPAN ID: PINYAN

=head1 SEE ALSO

  Stat::lsMode (by Mark-James Dominus, CPAN ID: MJD)
  chmod(1) manpage
  perldoc -f chmod
  perldoc -f stat

=head1 COPYRIGHT AND LICENCE

Copyright (C) 2007 by Jeff Pinyan

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.8 or,
at your option, any later version of Perl 5 you may have available.

=cut
