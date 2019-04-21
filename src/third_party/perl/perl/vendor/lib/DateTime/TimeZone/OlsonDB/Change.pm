package DateTime::TimeZone::OlsonDB::Change;
{
  $DateTime::TimeZone::OlsonDB::Change::VERSION = '1.46';
}

use strict;
use warnings;

use Params::Validate qw( validate SCALAR UNDEF OBJECT );

sub new {
    my $class = shift;
    my %p     = validate(
        @_, {
            utc_start_datetime   => { type => UNDEF | OBJECT },
            local_start_datetime => { type => UNDEF | OBJECT },
            short_name           => { type => SCALAR },
            observance           => { type => OBJECT },
            rule                 => { type => OBJECT, default => undef },
            type => {
                type  => SCALAR,
                regex => qr/^(?:observance|rule)$/
            },
        }
    );

    # These are almost always mutually exclusive, except when adding
    # an observance change and the last rule has no offset, but the
    # new observance has an anonymous rule.  In that case, prefer the
    # offset from std defined in the observance to that in the
    # previous rule (what a mess!).
    if ( $p{type} eq 'observance' ) {
        $p{offset_from_std} = $p{rule}->offset_from_std if defined $p{rule};
        $p{offset_from_std} = $p{observance}->offset_from_std
            if $p{observance}->offset_from_std;
        $p{offset_from_std} ||= 0;
    }
    else {
        $p{offset_from_std} = $p{observance}->offset_from_std;
        $p{offset_from_std} = $p{rule}->offset_from_std if defined $p{rule};
    }

    $p{offset_from_utc} = $p{observance}->offset_from_utc;

    $p{is_dst} = 0;
    $p{is_dst} = 1 if $p{rule} && $p{rule}->offset_from_std;
    $p{is_dst} = 1 if $p{observance}->offset_from_std;

    if ( $p{short_name} =~ m{(\w+)/(\w+)} ) {
        $p{short_name} = $p{is_dst} ? $2 : $1;
    }

    return bless \%p, $class;
}

sub utc_start_datetime   { $_[0]->{utc_start_datetime} }
sub local_start_datetime { $_[0]->{local_start_datetime} }
sub short_name           { $_[0]->{short_name} }
sub is_dst               { $_[0]->{is_dst} }
sub observance           { $_[0]->{observance} }
sub rule                 { $_[0]->{rule} }
sub offset_from_utc      { $_[0]->{offset_from_utc} }
sub offset_from_std      { $_[0]->{offset_from_std} }
sub total_offset         { $_[0]->offset_from_utc + $_[0]->offset_from_std }

sub two_changes_as_span {
    my ( $c1, $c2, $last_total_offset ) = @_;

    my ( $utc_start, $local_start );

    if ( defined $c1->utc_start_datetime ) {
        $utc_start   = $c1->utc_start_datetime->utc_rd_as_seconds;
        $local_start = $c1->local_start_datetime->utc_rd_as_seconds;
    }
    else {
        $utc_start = $local_start = '-inf';
    }

    my $utc_end   = $c2->utc_start_datetime->utc_rd_as_seconds;
    my $local_end = $utc_end + $c1->total_offset;

    return {
        utc_start   => $utc_start,
        utc_end     => $utc_end,
        local_start => $local_start,
        local_end   => $local_end,
        short_name  => $c1->short_name,
        offset      => $c1->total_offset,
        is_dst      => $c1->is_dst,
    };
}

sub _debug_output {
    my $self = shift;

    my $obs = $self->observance;

    if ( $self->utc_start_datetime ) {
        print " UTC:        ", $self->utc_start_datetime->datetime,   "\n";
        print " Local:      ", $self->local_start_datetime->datetime, "\n";
    }
    else {
        print " First change (starts at -inf)\n";
    }

    print " Short name: ", $self->short_name,     "\n";
    print " UTC offset: ", $obs->offset_from_utc, "\n";

    if ( $obs->offset_from_std || $self->rule ) {
        if ( $obs->offset_from_std ) {
            print " Std offset: ", $obs->offset_from_std, "\n";
        }

        if ( $self->rule ) {
            print " Std offset: ", $self->rule->offset_from_std, ' - ',
                $self->rule->name, " rule\n";
        }
    }
    else {
        print " Std offset: 0 - no rule\n";
    }

    print "\n";
}

1;
