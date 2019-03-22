package DateTime::TimeZone::OffsetOnly;
{
  $DateTime::TimeZone::OffsetOnly::VERSION = '1.46';
}

use strict;
use warnings;

use parent 'DateTime::TimeZone';

use DateTime::TimeZone::UTC;
use Params::Validate qw( validate SCALAR );

sub new {
    my $class = shift;
    my %p     = validate(
        @_, {
            offset => { type => SCALAR },
        }
    );

    my $offset = DateTime::TimeZone::offset_as_seconds( $p{offset} );

    die "Invalid offset: $p{offset}\n" unless defined $offset;

    return DateTime::TimeZone::UTC->new unless $offset;

    my $self = {
        name   => DateTime::TimeZone::offset_as_string($offset),
        offset => $offset,
    };

    return bless $self, $class;
}

sub is_dst_for_datetime {0}

sub offset_for_datetime       { $_[0]->{offset} }
sub offset_for_local_datetime { $_[0]->{offset} }

sub is_utc {0}

sub short_name_for_datetime { $_[0]->name }

sub category {undef}

sub STORABLE_freeze {
    my $self = shift;

    return $self->name;
}

sub STORABLE_thaw {
    my $self       = shift;
    my $cloning    = shift;
    my $serialized = shift;

    my $class = ref $self || $self;

    my $obj;
    if ( $class->isa(__PACKAGE__) ) {
        $obj = __PACKAGE__->new( offset => $serialized );
    }
    else {
        $obj = $class->new( offset => $serialized );
    }

    %$self = %$obj;

    return $self;
}

1;

# ABSTRACT: A DateTime::TimeZone object that just contains an offset



=pod

=head1 NAME

DateTime::TimeZone::OffsetOnly - A DateTime::TimeZone object that just contains an offset

=head1 VERSION

version 1.46

=head1 SYNOPSIS

  my $offset_tz = DateTime::TimeZone->new( name => '-0300' );

=head1 DESCRIPTION

This class is used to provide the DateTime::TimeZone API needed by
DateTime.pm, but with a fixed offset.  An object in this class always
returns the same offset as was given in its constructor, regardless of
the date.

=head1 USAGE

This class has the same methods as a real time zone object, but the
C<category()> method returns undef.

=head2 DateTime::TimeZone::OffsetOnly->new ( offset => $offset )

The value given to the offset parameter must be a string such as
"+0300".  Strings will be converted into numbers by the
C<DateTime::TimeZone::offset_as_seconds()> function.

=head2 $tz->offset_for_datetime( $datetime )

No matter what date is given, the offset provided to the constructor
is always used.

=head2 $tz->name()

=head2 $tz->short_name_for_datetime()

Both of these methods return the offset in string form.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2012 by Dave Rolsky.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut


__END__

