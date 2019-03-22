package DateTime::TimeZone::Local;
{
  $DateTime::TimeZone::Local::VERSION = '1.46';
}

use strict;
use warnings;

use Class::Load qw( is_class_loaded load_class try_load_class );
use DateTime::TimeZone;
use File::Spec;

sub TimeZone {
    my $class = shift;

    my $subclass = $class->_load_subclass();

    for my $meth ( $subclass->Methods() ) {
        my $tz = $subclass->$meth();

        return $tz if $tz;
    }

    die "Cannot determine local time zone\n";
}

{
    # Stolen from File::Spec. My theory is that other folks can write
    # the non-existent modules if they feel a need, and release them
    # to CPAN separately.
    my %subclass = (
        MSWin32 => 'Win32',
        VMS     => 'VMS',
        MacOS   => 'Mac',
        os2     => 'OS2',
        epoc    => 'Epoc',
        NetWare => 'Win32',
        symbian => 'Win32',
        dos     => 'OS2',
        cygwin  => 'Unix',
    );

    sub _load_subclass {
        my $class = shift;

        my $os_name = $subclass{$^O} || $^O;
        my $subclass = $class . '::' . $os_name;

        return $subclass if is_class_loaded($subclass);

        return $subclass if try_load_class($subclass);

        $subclass = $class . '::Unix';

        load_class($subclass);

        return $subclass;
    }
}

sub FromEnv {
    my $class = shift;

    foreach my $var ( $class->EnvVars() ) {
        if ( $class->_IsValidName( $ENV{$var} ) ) {
            my $tz;
            {
                local $@;
                local $SIG{__DIE__};
                $tz = eval { DateTime::TimeZone->new( name => $ENV{$var} ) };
            }
            return $tz if $tz;
        }
    }

    return;
}

sub _IsValidName {
    shift;

    return 0 unless defined $_[0];
    return 0 if $_[0] eq 'local';

    return $_[0] =~ m{^[\w/\-\+]+$};
}

1;

# ABSTRACT: Determine the local system's time zone



=pod

=head1 NAME

DateTime::TimeZone::Local - Determine the local system's time zone

=head1 VERSION

version 1.46

=head1 SYNOPSIS

  my $tz = DateTime::TimeZone->new( name => 'local' );

  my $tz = DateTime::TimeZone::Local->TimeZone();

=head1 DESCRIPTION

This module provides an interface for determining the local system's
time zone. Most of the functionality for doing this is in OS-specific
subclasses.

=head1 USAGE

This class provides the following methods:

=head2 DateTime::TimeZone::Local->TimeZone()

This attempts to load an appropriate subclass and asks it to find the
local time zone. This method is called by when you pass "local" as the
time zone name to C<< DateTime:TimeZone->new() >>.

If your OS is not explicitly handled, you can create a module with a
name of the form C<DateTime::TimeZone::Local::$^O>. If it exists, it
will be used instead of falling back to the Unix subclass.

If no OS-specific module exists, we fall back to using the Unix
subclass.

See L<DateTime::TimeZone::Local::Unix>,
L<DateTime::TimeZone::Local::Win32>, and
L<DateTime::TimeZone::Local::VMS> for OS-specific details.

=head1 SUBCLASSING

If you want to make a new OS-specific subclass, there are several
methods provided by this module you should know about.

=head2 $class->Methods()

This method should be provided by your class. It should provide a list
of methods that will be called to try to determine the local time
zone.

Each of these methods is expected to return a new
C<DateTime::TimeZone> object if it determines the time zone.

=head2 $class->FromEnv()

This method tries to find a valid time zone in an C<%ENV> value. It
calls C<< $class->EnvVars() >> to determine which keys to look at.

To use this from a subclass, simply return "FromEnv" as one of the
items from C<< $class->Methods() >>.

=head2 $class->EnvVars()

This method should be provided by your subclass. It should return a
list of env vars to be checked by C<< $class->FromEnv() >>.

=head2 $class->_IsValidName($name)

Given a possible time zone name, this returns a boolean indicating
whether or not the the name looks valid. It always return false for
"local" in order to avoid infinite loops.

=head1 EXAMPLE SUBCLASS

Here is a simple example subclass:

  package DateTime::TimeZone::SomeOS;

  use strict;
  use warnings;

  use base 'DateTime::TimeZone::Local';


  sub Methods { qw( FromEnv FromEther ) }

  sub EnvVars { qw( TZ ZONE ) }

  sub FromEther
  {
      my $class = shift;

      ...
  }

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2012 by Dave Rolsky.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut


__END__

