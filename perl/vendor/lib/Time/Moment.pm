package Time::Moment;
use strict;
use warnings;

use Carp qw[];

BEGIN {
    our $VERSION = '0.44';
    require XSLoader; XSLoader::load(__PACKAGE__, $VERSION);
}

BEGIN {
    unless (exists &Time::Moment::now) {
        require Time::HiRes;
        eval sprintf <<'EOC', __FILE__;
# line 17 %s

# expects normalized tm values; algorithm is only valid for tm year's [1, 199]
sub timegm {
    my ($y, $d, $h, $m, $s) = @_[5,7,2,1,0];
    return ((1461 * --$y >> 2) + $d - 25202) * 86400 + $h * 3600 + $m * 60 + $s;
}

sub now {
    @_ == 1 || Carp::croak(q/Usage: Time::Moment->now()/);
    my ($class) = @_;

    my ($sec, $usec) = Time::HiRes::gettimeofday();
    my $offset = int((timegm(localtime($sec)) - $sec) / 60);
    return $class->from_epoch($sec, $usec * 1000)
                 ->with_offset_same_instant($offset);
}

sub now_utc {
    @_ == 1 || Carp::croak(q/Usage: Time::Moment->now_utc()/);
    my ($class) = @_;

    my ($sec, $usec) = Time::HiRes::gettimeofday();
    return $class->from_epoch($sec, $usec * 1000);
}
EOC
        die $@ if $@;
    }
}

BEGIN {
    delete @Time::Moment::{qw(timegm)};
}

sub __as_DateTime {
    my ($tm) = @_;
    return DateTime->from_epoch(
        epoch     => $tm->epoch,
        time_zone => $tm->strftime('%Z'),
    )->set_nanosecond($tm->nanosecond);
}

sub __as_Time_Piece {
    my ($tm) = @_;
    return scalar Time::Piece::gmtime($tm->epoch);
}

sub DateTime::__as_Time_Moment {
    my ($dt) = @_;

    (!$dt->time_zone->is_floating)
      or Carp::croak(q/Cannot coerce an instance of DateTime in the 'floating' /
                    .q/time zone to an instance of Time::Moment/);

    return Time::Moment->from_epoch($dt->epoch, $dt->nanosecond)
                       ->with_offset_same_instant(int($dt->offset / 60));
}

sub Time::Piece::__as_Time_Moment {
    my ($tp) = @_;
    return Time::Moment->from_epoch($tp->epoch)
                       ->with_offset_same_instant(int($tp->tzoffset / 60));
}

sub STORABLE_freeze {
    my ($self, $cloning) = @_;
    return if $cloning;
    return pack 'nnNNN', 0x544D, $self->offset, $self->utc_rd_values;
}

sub STORABLE_thaw {
    my ($self, $cloning, $packed) = @_;
    return if $cloning;
    (length($packed) == 16 && vec($packed, 0, 16) == 0x544D) # TM
      or die(q/Cannot deserialize corrupted data/); # Don't replace die with Carp!
    my ($offset, $rdn, $sod, $nos) = unpack 'xxnNNN', $packed;
    $offset = ($offset & 0x7FFF) - 0x8000 if ($offset & 0x8000);
    my $seconds = ($rdn - 719163) * 86400 + $sod;
    $$self = ${ ref($self)->from_epoch($seconds, $nos)
                          ->with_offset_same_instant($offset) };
}

sub TO_JSON {
    return $_[0]->to_string;
}

sub TO_CBOR {
    # Use the standard tag for date/time string; see RFC 7049 Section 2.4.1
    return CBOR::XS::tag(0, $_[0]->to_string);
}

sub FREEZE {
    return $_[0]->to_string;
}

sub THAW {
    my ($class, undef, $string) = @_;
    return $class->from_string($string);
}

# Alias
*with_offset = \&with_offset_same_instant;

# used by DateTime::TimeZone
sub utc_year {
    return $_[0]->with_offset_same_instant(0)->year;
}

1;

