package Test::MockTime;

use strict;
use warnings;
use Carp();
use Exporter();
*import = \&Exporter::import;
our @EXPORT_OK = qw(
  set_relative_time
  set_absolute_time
  set_fixed_time
  restore_time
);
our %EXPORT_TAGS = ( 'all' => \@EXPORT_OK, );
our $VERSION     = '0.17';
our $offset      = 0;
our $fixed       = undef;

BEGIN {
    *CORE::GLOBAL::time      = \&Test::MockTime::time;
    *CORE::GLOBAL::localtime = \&Test::MockTime::localtime;
    *CORE::GLOBAL::gmtime    = \&Test::MockTime::gmtime;
}

sub set_relative_time {
    my ($relative) = @_;
    if (   ( $relative eq __PACKAGE__ )
        || ( UNIVERSAL::isa( $relative, __PACKAGE__ ) ) )
    {
        Carp::carp("Test::MockTime::set_relative_time called incorrectly\n");
    }
    $offset = $_[-1];    # last argument. might have been called in a OO syntax?
    return $offset;
}

sub _time {
    my ( $time, $spec ) = @_;
    if ( $time !~ /\A -? \d+ \z/xms ) {
        $spec ||= '%Y-%m-%dT%H:%M:%SZ';
    }
    if ($spec) {
        require Time::Piece;
        $time = Time::Piece->strptime( $time, $spec )->epoch();
    }
    return $time;
}

sub set_absolute_time {
    my ( $time, $spec ) = @_;
    if ( ( $time eq __PACKAGE__ ) || ( UNIVERSAL::isa( $time, __PACKAGE__ ) ) )
    {
        Carp::carp("Test::MockTime::set_absolute_time called incorrectly\n");
    }
    $time = _time( $time, $spec );
    $offset = $time - CORE::time;
    return $offset;
}

sub set_fixed_time {
    my ( $time, $spec ) = @_;
    if ( ( $time eq __PACKAGE__ ) || ( UNIVERSAL::isa( $time, __PACKAGE__ ) ) )
    {
        Carp::carp("Test::MockTime::set_fixed_time called incorrectly\n");
    }
    $time = _time( $time, $spec );
    $fixed = $time;
    return $fixed;
}

sub time() {
    if ( defined $fixed ) {
        return $fixed;
    }
    else {
        return ( CORE::time + $offset );
    }
}

sub localtime (;$) {
    my ($time) = @_;
    if ( !defined $time ) {
        $time = Test::MockTime::time();
    }
    return CORE::localtime($time);
}

sub gmtime (;$) {
    my ($time) = @_;
    if ( !defined $time ) {
        $time = Test::MockTime::time();
    }
    return CORE::gmtime($time);
}

sub restore {
    $offset = 0;
    $fixed  = undef;
    return;
}
*restore_time = \&restore;

1;
__END__

=head1 NAME

Test::MockTime - Replaces actual time with simulated time 

=head1 VERSION

Version 0.17

=head1 SYNOPSIS

  use Test::MockTime qw( :all );
  set_relative_time(-600);

  # do some tests depending on time increasing from 600 seconds ago

  set_absolute_time(0);

  # do some more tests depending on time starting from the epoch
  # epoch may vary according to platform.  see perlport.

  set_fixed_time(CORE::time());

  # do some more tests depending on time staying at the current actual time

  set_absolute_time('1970-01-01T00:00:00Z');

  # do some tests depending on time starting at Unix epoch time

  set_fixed_time('01/01/1970 00:00:00', '%m/%d/%Y %H:%M:%S');

  # do some tests depending on time staying at the Unix epoch time

  restore_time();

  # resume normal service

=head1 DESCRIPTION

This module was created to enable test suites to test code at specific 
points in time. Specifically it overrides localtime, gmtime and time at
compile time and then relies on the user supplying a mock time via 
set_relative_time, set_absolute_time or set_fixed_time to alter future 
calls to gmtime,time or localtime.

=head1 SUBROUTINES/METHODS

=over

=item set_absolute_time

If given a single, numeric argument, the argument is an absolute time (for
example, if 0 is supplied, the absolute time will be the epoch), and
calculates the offset to allow subsequent calls to time, gmtime and localtime
to reflect this.

for example, in the following code

  Test::MockTime::set_absolute_time(0);
  my ($start) = time;
  sleep 2;
  my ($end) = time;

The $end variable should contain 2 seconds past the epoch;

If given two arguments, the first argument is taken to be an absolute time in
some string format (for example, "01/01/1970 00:00:00").  The second argument
is taken to be a C<strptime> format string (for example, "%m/%d/%Y %H:%M:%S").
If a single argument is given, but that argument is not numeric, a
C<strptime> format string of "%Y-%m-%dT%H:%M:%SZ" is assumed.

for example, in the following code

  Test::MockTime::set_absolute_time('1970-01-01T00:00:00Z');
  my ($start) = time;
  sleep 2;
  my ($end) = time;

The $end variable should contain 2 seconds past the Unix epoch;

=item set_relative_time($relative)

takes as an argument an relative value from current time (for example, if -10
is supplied, current time be converted to actual machine time - 10 seconds)
and calculates the offset to allow subsequent calls to time,gmtime and localtime
to reflect this.

for example, in the following code

  my ($start) = time;
  Test::MockTime::set_relative_time(-600);
  sleep 600;
  my ($end) = time;

The $end variable should contain either the same or very similar values to the
$start variable.

=item set_fixed_time

If given a single, numeric argument, the argument is an absolute time (for
example, if 0 is supplied, the absolute time will be the epoch).  All
subsequent calls to gmtime, localtime and time will return this value.

for example, in the following code

  Test::MockTime::set_fixed_time(time)
  my ($start) = time;
  sleep 3;
  my ($end) = time;

the $end variable and the $start variable will contain the same results

If given two arguments, the first argument is taken to be an absolute time in
some string format (for example, "01/01/1970 00:00:00").  The second argument
is taken to be a C<strptime> format string (for example, "%m/%d/%Y %H:%M:%S").
If a single argument is given, but that argument is not numeric, a
C<strptime> format string of "%Y-%m-%dT%H:%M:%SZ" is assumed.

=item restore()

restore the default time handling values.  C<restore_time> is an alias. When
exported with the 'all' tag, this subroutine is exported as C<restore_time>.

=back

=head1 CONFIGURATION AND ENVIRONMENT

Test::MockTime requires no configuration files or environment variables.

=head1 DEPENDENCIES

Test::MockTime depends on the following non-core Perl modules.

=over

=item *
L<Time::Piece 1.08 or greater|Time::Piece>

=back

=head1 INCOMPATIBILITIES

None reported

=head1 BUGS AND LIMITATIONS

Probably.
 
=head1 AUTHOR

David Dick <ddick@cpan.org>

=head1 LICENSE AND COPYRIGHT

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

=head1 ACKNOWLEDGEMENTS

Thanks to a use.perl.org journal entry <http://use.perl.org/~geoff/journal/20660> by 
Geoffrey Young.
