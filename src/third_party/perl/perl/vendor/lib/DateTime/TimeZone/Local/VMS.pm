package DateTime::TimeZone::Local::VMS;
{
  $DateTime::TimeZone::Local::VMS::VERSION = '1.46';
}

use strict;
use warnings;

use parent 'DateTime::TimeZone::Local';

sub Methods { return qw( FromEnv ) }

sub EnvVars {
    return qw( TZ SYS$TIMEZONE_RULE SYS$TIMEZONE_NAME UCX$TZ TCPIP$TZ );
}

1;

# ABSTRACT: Determine the local system's time zone on VMS



=pod

=head1 NAME

DateTime::TimeZone::Local::VMS - Determine the local system's time zone on VMS

=head1 VERSION

version 1.46

=head1 SYNOPSIS

  my $tz = DateTime::TimeZone->new( name => 'local' );

  my $tz = DateTime::TimeZone::Local->TimeZone();

=head1 DESCRIPTION

This module provides methods for determining the local time zone on a
VMS platform.

NOTE: This is basically a stub pending an implementation by someone
who knows something about VMS.

=head1 HOW THE TIME ZONE IS DETERMINED

This class tries the following methods of determining the local time
zone:

=over 4

=item * %ENV

We check the following environment variables:

=over 8

=item * TZ

=item * SYS$TIMEZONE_RULE

=item * SYS$TIMEZONE_NAME

=item * UCX$TZ

=item * TCPIP$TZ

=back

=back

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2012 by Dave Rolsky.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut


__END__

