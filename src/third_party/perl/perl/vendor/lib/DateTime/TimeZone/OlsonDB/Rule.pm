package DateTime::TimeZone::OlsonDB::Rule;
{
  $DateTime::TimeZone::OlsonDB::Rule::VERSION = '1.46';
}

use strict;
use warnings;

use DateTime;
use DateTime::Duration;
use DateTime::TimeZone::OlsonDB;

use Params::Validate qw( validate SCALAR );

sub new {
    my $class = shift;
    my %p     = validate(
        @_, {
            name   => { type => SCALAR },
            from   => { type => SCALAR },
            to     => { type => SCALAR },
            type   => { type => SCALAR, default => undef },
            in     => { type => SCALAR },
            on     => { type => SCALAR },
            at     => { type => SCALAR },
            save   => { type => SCALAR },
            letter => { type => SCALAR, default => '' },
        },
    );

    my $save = $p{save};

    # The handling of q{-} and q{1} are to account for new syntax introduced
    # in 2009u (and hopefully gone in future versions).
    if ( $save && $save ne q{-} ) {
        if ( $save =~ /^\d+$/ ) {
            $p{offset_from_std} = 3600 * $save;
        }
        else {
            $p{offset_from_std}
                = DateTime::TimeZone::offset_as_seconds($save);
        }
    }
    else {
        $p{offset_from_std} = 0;
    }

    return bless \%p, $class;
}

sub name            { $_[0]->{name} }
sub offset_from_std { $_[0]->{offset_from_std} }
sub letter          { $_[0]->{letter} }
sub min_year        { $_[0]->{from} }

sub max_year {
          $_[0]->{to} eq 'only' ? $_[0]->min_year
        : $_[0]->{to} eq 'max'  ? undef
        :                         $_[0]->{to};
}

sub is_infinite { $_[0]->{to} eq 'max' ? 1 : 0 }

sub month { $DateTime::TimeZone::OlsonDB::MONTHS{ $_[0]->{in} } }
sub on    { $_[0]->{on} }
sub at    { $_[0]->{at} }

sub utc_start_datetime_for_year {
    my $self            = shift;
    my $year            = shift;
    my $offset_from_utc = shift;

    # should be the offset of the _previous_ rule
    my $offset_from_std = shift;

    my $day = DateTime::TimeZone::OlsonDB::parse_day_spec( $self->on,
        $self->month, $year );

    my $utc = DateTime::TimeZone::OlsonDB::utc_datetime_for_time_spec(
        spec            => $self->at,
        year            => $year,
        month           => $self->month,
        day             => $day,
        offset_from_utc => $offset_from_utc,
        offset_from_std => $offset_from_std,
    );

    return $utc;
}

1;
