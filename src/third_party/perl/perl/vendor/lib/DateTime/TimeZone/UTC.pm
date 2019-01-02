package DateTime::TimeZone::UTC;
{
  $DateTime::TimeZone::UTC::VERSION = '1.46';
}

use strict;
use warnings;

use parent 'Class::Singleton', 'DateTime::TimeZone';

sub new {
    return shift->instance;
}

sub _new_instance {
    my $class = shift;

    return bless { name => 'UTC' }, $class;
}

sub is_dst_for_datetime {0}

sub offset_for_datetime       {0}
sub offset_for_local_datetime {0}

sub short_name_for_datetime {'UTC'}

sub category {undef}

sub is_utc {1}

1;

# ABSTRACT: The UTC time zone



=pod

=head1 NAME

DateTime::TimeZone::UTC - The UTC time zone

=head1 VERSION

version 1.46

=head1 SYNOPSIS

  my $utc_tz = DateTime::TimeZone::UTC->new;

=head1 DESCRIPTION

This class is used to provide the DateTime::TimeZone API needed by
DateTime.pm for the UTC time zone, which is not explicitly included in
the Olson time zone database.

The offset for this object will always be zero.

=head1 USAGE

This class has the same methods as a real time zone object, but the
C<category()> method returns undef and C<is_utc()> returns true.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2012 by Dave Rolsky.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut


__END__

