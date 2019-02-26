package DateTime::TimeZone::OlsonDB;
{
  $DateTime::TimeZone::OlsonDB::VERSION = '1.46';
}

use strict;
use warnings;

use vars qw( %MONTHS %DAYS $PLUS_ONE_DAY_DUR $MINUS_ONE_DAY_DUR );

use DateTime::TimeZone::OlsonDB::Rule;
use DateTime::TimeZone::OlsonDB::Zone;
use Params::Validate qw( validate SCALAR );

my $x = 1;
%MONTHS = map { $_ => $x++ } qw( Jan Feb Mar Apr May Jun
    Jul Aug Sep Oct Nov Dec);

$x = 1;
%DAYS = map { $_ => $x++ } qw( Mon Tue Wed Thu Fri Sat Sun );

$PLUS_ONE_DAY_DUR  = DateTime::Duration->new( days => 1 );
$MINUS_ONE_DAY_DUR = DateTime::Duration->new( days => -1 );

sub new {
    my $class = shift;

    return bless {
        rules => {},
        zones => {},
        links => {},
    }, $class;
}

sub parse_file {
    my $self = shift;
    my $file = shift;

    open my $fh, "<$file"
        or die "Cannot read $file: $!";

    while (<$fh>) {
        chomp;
        $self->_parse_line($_);
    }
}

sub _parse_line {
    my $self = shift;
    my $line = shift;

    return if $line =~ /^\s+$/;
    return if $line =~ /^#/;

    # remove any comments at the end of the line
    $line =~ s/\s*#.+$//;

    if ( $self->{in_zone} && $line =~ /^\t/ ) {
        $self->_parse_zone( $line, $self->{in_zone} );
        return;
    }

    foreach (qw( Rule Zone Link )) {
        if ( substr( $line, 0, 4 ) eq $_ ) {
            my $m = '_parse_' . lc $_;
            $self->$m($line);
        }
    }
}

sub _parse_rule {
    my $self = shift;
    my $rule = shift;

    my @items = split /\s+/, $rule, 10;

    shift @items;
    my $name = shift @items;

    my %rule;
    @rule{qw( from to type in on at save letter )} = @items;
    delete $rule{letter} if $rule{letter} eq '-';

    # As of the 2003a data, there are no rules with a type set
    delete $rule{type} if $rule{type} eq '-';

    push @{ $self->{rules}{$name} },
        DateTime::TimeZone::OlsonDB::Rule->new( name => $name, %rule );

    undef $self->{in_zone};
}

sub _parse_zone {
    my $self = shift;
    my $zone = shift;
    my $name = shift;

    my $expect = $name ? 5 : 6;
    my @items = grep { defined && length } split /\s+/, $zone, $expect;

    my %obs;
    unless ($name) {
        shift @items;    # remove "Zone"
        $name = shift @items;
    }

    @obs{qw( gmtoff rules format until )} = @items;

    if ( $obs{rules} =~ /\d\d?:\d\d/ ) {
        $obs{offset_from_std} = delete $obs{rules};
    }
    else {
        delete $obs{rules} if $obs{rules} eq '-';
    }

    delete $obs{until} unless defined $obs{until};

    push @{ $self->{zones}{$name} }, \%obs;

    $self->{in_zone} = $name;
}

sub _parse_link {
    my $self = shift;
    my $link = shift;

    my @items = split /\s+/, $link, 3;

    $self->{links}{ $items[2] } = $items[1];

    undef $self->{in_zone};
}

sub links { %{ $_[0]->{links} } }

sub zone_names { keys %{ $_[0]->{zones} } }

sub zone {
    my $self = shift;
    my $name = shift;

    die "Invalid zone name $name"
        unless exists $self->{zones}{$name};

    return DateTime::TimeZone::OlsonDB::Zone->new(
        name        => $name,
        observances => $self->{zones}{$name},
        olson_db    => $self,
    );
}

sub expanded_zone {
    my $self = shift;
    my %p    = validate(
        @_, {
            name           => { type => SCALAR },
            expand_to_year => {
                type    => SCALAR,
                default => (localtime)[5] + 1910
            },
        }
    );

    my $zone = $self->zone( $p{name} );

    $zone->expand_observances( $self, $p{expand_to_year} );

    return $zone;
}

sub rules_by_name {
    my $self = shift;
    my $name = shift;

    return unless defined $name;

    die "Invalid rule name $name"
        unless exists $self->{rules}{$name};

    return @{ $self->{rules}{$name} };
}

sub parse_day_spec {
    my ( $day, $month, $year ) = @_;

    return $day if $day =~ /^\d+$/;

    if ( $day =~ /^last(\w\w\w)$/ ) {
        my $dow = $DAYS{$1};

        my $last_day = DateTime->last_day_of_month(
            year      => $year,
            month     => $month,
            time_zone => 'floating',
        );

        my $dt = DateTime->new(
            year      => $year,
            month     => $month,
            day       => $last_day->day,
            time_zone => 'floating',
        );

        while ( $dt->day_of_week != $dow ) {
            $dt -= $PLUS_ONE_DAY_DUR;
        }

        return $dt->day;
    }
    elsif ( $day =~ /^(\w\w\w)([><])=(\d\d?)$/ ) {
        my $dow = $DAYS{$1};

        my $dt = DateTime->new(
            year      => $year,
            month     => $month,
            day       => $3,
            time_zone => 'floating',
        );

        my $dur = $2 eq '<' ? $MINUS_ONE_DAY_DUR : $PLUS_ONE_DAY_DUR;

        while ( $dt->day_of_week != $dow ) {
            $dt += $dur;
        }

        return $dt->day;
    }
    else {
        die "Invalid on spec for rule: $day\n";
    }
}

sub utc_datetime_for_time_spec {
    my %p = validate(
        @_, {
            spec            => { type => SCALAR },
            year            => { type => SCALAR },
            month           => { type => SCALAR },
            day             => { type => SCALAR },
            offset_from_utc => { type => SCALAR },
            offset_from_std => { type => SCALAR },
        },
    );

    # 'w'all - ignore it, because that's the default
    $p{spec} =~ s/w$//;

    # 'g'reenwich, 'u'tc, or 'z'ulu
    my $is_utc = $p{spec} =~ s/[guz]$//;

    # 's'tandard time - ignore DS offset
    my $is_std = $p{spec} =~ s/s$//;

    my ( $hour, $minute, $second ) = split /:/, $p{spec};
    $minute = 0 unless defined $minute;
    $second = 0 unless defined $second;

    my $add_day = 0;
    if ( $hour == 24 ) {
        $hour    = 0;
        $add_day = 1;
    }

    my $utc;
    if ($is_utc) {
        $utc = DateTime->new(
            year      => $p{year},
            month     => $p{month},
            day       => $p{day},
            hour      => $hour,
            minute    => $minute,
            second    => $second,
            time_zone => 'floating',
        );
    }
    else {
        my $local = DateTime->new(
            year      => $p{year},
            month     => $p{month},
            day       => $p{day},
            hour      => $hour,
            minute    => $minute,
            second    => $second,
            time_zone => 'floating',
        );

        $p{offset_from_std} = 0 if $is_std;

        my $dur = DateTime::Duration->new(
            seconds => $p{offset_from_utc} + $p{offset_from_std} );

        $utc = $local - $dur;
    }

    $utc->add( days => 1 ) if $add_day;

    return $utc;
}

1;

# ABSTRACT: An object to represent an Olson time zone database



=pod

=head1 NAME

DateTime::TimeZone::OlsonDB - An object to represent an Olson time zone database

=head1 VERSION

version 1.46

=head1 SYNOPSIS

  none yet

=head1 DESCRIPTION

This module parses the Olson database time zone definition files and
creates various objects representing time zone data.

Each time zone is broken down into several parts.  The first piece is
an observance, which is an offset from UTC and an abbreviation.  A
single zone may contain many observances, reflecting historical
changes in that time zone over time.  An observance may also refer to
a set of rules.

Rules are named, and may apply to many different zones.  For example,
the "US" rules apply to most of the time zones in the US,
unsurprisingly.  Rules are made of an offset from standard time and a
definition of when that offset changes.  Changes can be a one time
thing, or they can recur at regular times through a span of years.

Each rule may have an associated letter, which is used to generate an
abbreviated name for the time zone, along with the offset's
abbreviation.  For example, if the offset's abbreviation is "C%sT",
and the a rule specifies the letter "S", then the abbreviation when
that rule is in effect is "CST".

=head1 USAGE

Not yet documented.  This stuff is a mess.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2012 by Dave Rolsky.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut


__END__

