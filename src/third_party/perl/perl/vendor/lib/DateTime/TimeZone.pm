package DateTime::TimeZone;
{
  $DateTime::TimeZone::VERSION = '1.46';
}

use 5.006;

use strict;
use warnings;

use DateTime::TimeZone::Catalog;
use DateTime::TimeZone::Floating;
use DateTime::TimeZone::Local;
use DateTime::TimeZone::OffsetOnly;
use DateTime::TimeZone::UTC;
use Params::Validate 0.72 qw( validate validate_pos SCALAR ARRAYREF BOOLEAN );

use constant INFINITY => 100**1000;
use constant NEG_INFINITY => -1 * ( 100**1000 );

# the offsets for each span element
use constant UTC_START   => 0;
use constant UTC_END     => 1;
use constant LOCAL_START => 2;
use constant LOCAL_END   => 3;
use constant OFFSET      => 4;
use constant IS_DST      => 5;
use constant SHORT_NAME  => 6;

my %SpecialName = map { $_ => 1 }
    qw( EST MST HST CET EET MET WET EST5EDT CST6CDT MST7MDT PST8PDT );

sub new {
    my $class = shift;
    my %p     = validate(
        @_,
        { name => { type => SCALAR } },
    );

    if ( exists $DateTime::TimeZone::Catalog::LINKS{ $p{name} } ) {
        $p{name} = $DateTime::TimeZone::Catalog::LINKS{ $p{name} };
    }
    elsif ( exists $DateTime::TimeZone::Catalog::LINKS{ uc $p{name} } ) {
        $p{name} = $DateTime::TimeZone::Catalog::LINKS{ uc $p{name} };
    }

    unless ( $p{name} =~ m,/,
        || $SpecialName{ $p{name} } ) {
        if ( $p{name} eq 'floating' ) {
            return DateTime::TimeZone::Floating->instance;
        }

        if ( $p{name} eq 'local' ) {
            return DateTime::TimeZone::Local->TimeZone();
        }

        if ( $p{name} eq 'UTC' || $p{name} eq 'Z' ) {
            return DateTime::TimeZone::UTC->instance;
        }

        return DateTime::TimeZone::OffsetOnly->new( offset => $p{name} );
    }

    my $subclass = $p{name};
    $subclass =~ s/-/_/g;
    $subclass =~ s{/}{::}g;
    my $real_class = "DateTime::TimeZone::$subclass";

    die "The timezone '$p{name}' in an invalid name.\n"
        unless $real_class =~ /^\w+(::\w+)*$/;

    unless ( $real_class->can('instance') ) {
        my $e = do {
            local $@;
            local $SIG{__DIE__};
            eval "require $real_class";
            $@;
        };

        if ($e) {
            my $regex = join '.', split /::/, $real_class;
            $regex .= '\\.pm';

            if ( $e =~ /^Can't locate $regex/i ) {
                die
                    "The timezone '$p{name}' could not be loaded, or is an invalid name.\n";
            }
            else {
                die $e;
            }
        }
    }

    my $zone = $real_class->instance( name => $p{name}, is_olson => 1 );

    if ( $zone->is_olson() ) {
        my $object_version
            = $zone->can('olson_version')
            ? $zone->olson_version()
            : 'unknown';
        my $catalog_version = DateTime::TimeZone::Catalog->OlsonVersion();

        if ( $object_version ne $catalog_version ) {
            warn
                "Loaded $real_class, which is from an older version ($object_version) of the Olson database than this installation of DateTime::TimeZone ($catalog_version).\n";
        }
    }

    return $zone;
}

sub _init {
    my $class = shift;
    my %p     = validate(
        @_, {
            name     => { type => SCALAR },
            spans    => { type => ARRAYREF },
            is_olson => { type => BOOLEAN, default => 0 },
        },
    );

    my $self = bless {
        name     => $p{name},
        spans    => $p{spans},
        is_olson => $p{is_olson},
    }, $class;

    foreach my $k (qw( last_offset last_observance rules max_year )) {
        my $m = "_$k";
        $self->{$k} = $self->$m() if $self->can($m);
    }

    return $self;
}

sub is_olson { $_[0]->{is_olson} }

sub is_dst_for_datetime {
    my $self = shift;

    my $span = $self->_span_for_datetime( 'utc', $_[0] );

    return $span->[IS_DST];
}

sub offset_for_datetime {
    my $self = shift;

    my $span = $self->_span_for_datetime( 'utc', $_[0] );

    return $span->[OFFSET];
}

sub offset_for_local_datetime {
    my $self = shift;

    my $span = $self->_span_for_datetime( 'local', $_[0] );

    return $span->[OFFSET];
}

sub short_name_for_datetime {
    my $self = shift;

    my $span = $self->_span_for_datetime( 'utc', $_[0] );

    return $span->[SHORT_NAME];
}

sub _span_for_datetime {
    my $self = shift;
    my $type = shift;
    my $dt   = shift;

    my $method = $type . '_rd_as_seconds';

    my $end = $type eq 'utc' ? UTC_END : LOCAL_END;

    my $span;
    my $seconds = $dt->$method();
    if ( $seconds < $self->max_span->[$end] ) {
        $span = $self->_spans_binary_search( $type, $seconds );
    }
    else {
        my $until_year = $dt->utc_year + 1;
        $span = $self->_generate_spans_until_match( $until_year, $seconds,
            $type );
    }

    # This means someone gave a local time that doesn't exist
    # (like during a transition into savings time)
    unless ( defined $span ) {
        my $err = 'Invalid local time for date';
        $err .= ' ' . $dt->iso8601 if $type eq 'utc';
        $err .= " in time zone: " . $self->name;
        $err .= "\n";

        die $err;
    }

    return $span;
}

sub _spans_binary_search {
    my $self = shift;
    my ( $type, $seconds ) = @_;

    my ( $start, $end ) = _keys_for_type($type);

    my $min = 0;
    my $max = scalar @{ $self->{spans} } + 1;
    my $i   = int( $max / 2 );

    # special case for when there are only 2 spans
    $i++ if $max % 2 && $max != 3;

    $i = 0 if @{ $self->{spans} } == 1;

    while (1) {
        my $current = $self->{spans}[$i];

        if ( $seconds < $current->[$start] ) {
            $max = $i;
            my $c = int( ( $i - $min ) / 2 );
            $c ||= 1;

            $i -= $c;

            return if $i < $min;
        }
        elsif ( $seconds >= $current->[$end] ) {
            $min = $i;
            my $c = int( ( $max - $i ) / 2 );
            $c ||= 1;

            $i += $c;

            return if $i >= $max;
        }
        else {

            # Special case for overlapping ranges because of DST and
            # other weirdness (like Alaska's change when bought from
            # Russia by the US).  Always prefer latest span.
            if ( $current->[IS_DST] && $type eq 'local' ) {

                # Asia/Dhaka in 2009j goes into DST without any known
                # end-of-DST date (wtf, Bangladesh).
                return $current if $current->[UTC_END] == INFINITY;

                my $next = $self->{spans}[ $i + 1 ];

                # Sometimes we will get here and the span we're
                # looking at is the last that's been generated so far.
                # We need to try to generate one more or else we run
                # out.
                $next ||= $self->_generate_next_span;

                die "No next span in $self->{max_year}" unless defined $next;

                if (   ( !$next->[IS_DST] )
                    && $next->[$start] <= $seconds
                    && $seconds <= $next->[$end] ) {
                    return $next;
                }
            }

            return $current;
        }
    }
}

sub _generate_next_span {
    my $self = shift;

    my $last_idx = $#{ $self->{spans} };

    my $max_span = $self->max_span;

    # Kind of a hack, but AFAIK there are no zones where it takes
    # _more_ than a year for a _future_ time zone change to occur, so
    # by looking two years out we can ensure that we will find at
    # least one more span.  Of course, I will no doubt be proved wrong
    # and this will cause errors.
    $self->_generate_spans_until_match( $self->{max_year} + 2,
        $max_span->[UTC_END] + ( 366 * 86400 ), 'utc' );

    return $self->{spans}[ $last_idx + 1 ];
}

sub _generate_spans_until_match {
    my $self                = shift;
    my $generate_until_year = shift;
    my $seconds             = shift;
    my $type                = shift;

    my @changes;
    my @rules = @{ $self->_rules };
    foreach my $year ( $self->{max_year} .. $generate_until_year ) {
        for ( my $x = 0; $x < @rules; $x++ ) {
            my $last_offset_from_std;

            if ( @rules == 2 ) {
                $last_offset_from_std
                    = $x
                    ? $rules[0]->offset_from_std
                    : $rules[1]->offset_from_std;
            }
            elsif ( @rules == 1 ) {
                $last_offset_from_std = $rules[0]->offset_from_std;
            }
            else {
                my $count = scalar @rules;
                die
                    "Cannot generate future changes for zone with $count infinite rules\n";
            }

            my $rule = $rules[$x];

            my $next = $rule->utc_start_datetime_for_year( $year,
                $self->{last_offset}, $last_offset_from_std );

            # don't bother with changes we've seen already
            next if $next->utc_rd_as_seconds < $self->max_span->[UTC_END];

            push @changes,
                DateTime::TimeZone::OlsonDB::Change->new(
                type                 => 'rule',
                utc_start_datetime   => $next,
                local_start_datetime => $next + DateTime::Duration->new(
                    seconds => $self->{last_observance}->total_offset
                        + $rule->offset_from_std
                ),
                short_name => sprintf(
                    $self->{last_observance}->format, $rule->letter
                ),
                observance => $self->{last_observance},
                rule       => $rule,
                );
        }
    }

    $self->{max_year} = $generate_until_year;

    my @sorted
        = sort { $a->utc_start_datetime <=> $b->utc_start_datetime } @changes;

    my ( $start, $end ) = _keys_for_type($type);

    my $match;
    for ( my $x = 1; $x < @sorted; $x++ ) {
        my $last_total_offset
            = $x == 1
            ? $self->max_span->[OFFSET]
            : $sorted[ $x - 2 ]->total_offset;

        my $span = DateTime::TimeZone::OlsonDB::Change::two_changes_as_span(
            @sorted[ $x - 1, $x ], $last_total_offset );

        $span = _span_as_array($span);

        push @{ $self->{spans} }, $span;

        $match = $span
            if $seconds >= $span->[$start] && $seconds < $span->[$end];
    }

    return $match;
}

sub max_span { $_[0]->{spans}[-1] }

sub _keys_for_type {
    $_[0] eq 'utc' ? ( UTC_START, UTC_END ) : ( LOCAL_START, LOCAL_END );
}

sub _span_as_array {
    [
        @{ $_[0] }{
            qw( utc_start utc_end local_start local_end offset is_dst short_name )
            }
    ];
}

sub is_floating {0}

sub is_utc {0}

sub has_dst_changes {0}

sub name { $_[0]->{name} }
sub category { ( split /\//, $_[0]->{name}, 2 )[0] }

sub is_valid_name {
    my $tz;
    {
        local $@;
        local $SIG{__DIE__};
        $tz = eval { $_[0]->new( name => $_[1] ) };
    }

    return $tz && $tz->isa('DateTime::TimeZone') ? 1 : 0;
}

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
        $obj = __PACKAGE__->new( name => $serialized );
    }
    else {
        $obj = $class->new( name => $serialized );
    }

    %$self = %$obj;

    return $self;
}

#
# Functions
#
sub offset_as_seconds {
    {
        local $@;
        local $SIG{__DIE__};
        shift if eval { $_[0]->isa('DateTime::TimeZone') };
    }

    my $offset = shift;

    return undef unless defined $offset;

    return 0 if $offset eq '0';

    my ( $sign, $hours, $minutes, $seconds );
    if ( $offset =~ /^([\+\-])?(\d\d?):(\d\d)(?::(\d\d))?$/ ) {
        ( $sign, $hours, $minutes, $seconds ) = ( $1, $2, $3, $4 );
    }
    elsif ( $offset =~ /^([\+\-])?(\d\d)(\d\d)(\d\d)?$/ ) {
        ( $sign, $hours, $minutes, $seconds ) = ( $1, $2, $3, $4 );
    }
    else {
        return undef;
    }

    $sign = '+' unless defined $sign;
    return undef unless $hours >= 0   && $hours <= 99;
    return undef unless $minutes >= 0 && $minutes <= 59;
    return undef
        unless !defined($seconds) || ( $seconds >= 0 && $seconds <= 59 );

    my $total = $hours * 3600 + $minutes * 60;
    $total += $seconds if $seconds;
    $total *= -1 if $sign eq '-';

    return $total;
}

sub offset_as_string {
    {
        local $@;
        local $SIG{__DIE__};
        shift if eval { $_[0]->isa('DateTime::TimeZone') };
    }

    my $offset = shift;

    return undef unless defined $offset;
    return undef unless $offset >= -359999 && $offset <= 359999;

    my $sign = $offset < 0 ? '-' : '+';

    $offset = abs($offset);

    my $hours = int( $offset / 3600 );
    $offset %= 3600;
    my $mins = int( $offset / 60 );
    $offset %= 60;
    my $secs = int($offset);

    return (
        $secs
        ? sprintf( '%s%02d%02d%02d', $sign, $hours, $mins, $secs )
        : sprintf( '%s%02d%02d',     $sign, $hours, $mins )
    );
}

# These methods all operate on data contained in the DateTime/TimeZone/Catalog.pm file.

sub all_names {
    return
        wantarray
        ? @DateTime::TimeZone::Catalog::ALL
        : [@DateTime::TimeZone::Catalog::ALL];
}

sub categories {
    return wantarray
        ? @DateTime::TimeZone::Catalog::CATEGORY_NAMES
        : [@DateTime::TimeZone::Catalog::CATEGORY_NAMES];
}

sub links {
    return
        wantarray
        ? %DateTime::TimeZone::Catalog::LINKS
        : {%DateTime::TimeZone::Catalog::LINKS};
}

sub names_in_category {
    shift if $_[0]->isa('DateTime::TimeZone');
    return unless exists $DateTime::TimeZone::Catalog::CATEGORIES{ $_[0] };

    return wantarray
        ? @{ $DateTime::TimeZone::Catalog::CATEGORIES{ $_[0] } }
        : [ $DateTime::TimeZone::Catalog::CATEGORIES{ $_[0] } ];
}

sub countries {
    wantarray
        ? ( sort keys %DateTime::TimeZone::Catalog::ZONES_BY_COUNTRY )
        : [ sort keys %DateTime::TimeZone::Catalog::ZONES_BY_COUNTRY ];
}

sub names_in_country {
    shift if $_[0]->isa('DateTime::TimeZone');

    return
        unless
        exists $DateTime::TimeZone::Catalog::ZONES_BY_COUNTRY{ lc $_[0] };

    return
        wantarray
        ? @{ $DateTime::TimeZone::Catalog::ZONES_BY_COUNTRY{ lc $_[0] } }
        : $DateTime::TimeZone::Catalog::ZONES_BY_COUNTRY{ lc $_[0] };
}

1;

# ABSTRACT: Time zone object base class and factory



=pod

=head1 NAME

DateTime::TimeZone - Time zone object base class and factory

=head1 VERSION

version 1.46

=head1 SYNOPSIS

  use DateTime;
  use DateTime::TimeZone;

  my $tz = DateTime::TimeZone->new( name => 'America/Chicago' );

  my $dt = DateTime->now();
  my $offset = $tz->offset_for_datetime($dt);

=head1 DESCRIPTION

This class is the base class for all time zone objects.  A time zone
is represented internally as a set of observances, each of which
describes the offset from GMT for a given time period.

Note that without the C<DateTime.pm> module, this module does not do
much.  It's primary interface is through a C<DateTime> object, and
most users will not need to directly use C<DateTime::TimeZone>
methods.

=head1 USAGE

This class has the following methods:

=head2 DateTime::TimeZone->new( name => $tz_name )

Given a valid time zone name, this method returns a new time zone
blessed into the appropriate subclass.  Subclasses are named for the
given time zone, so that the time zone "America/Chicago" is the
DateTime::TimeZone::America::Chicago class.

If the name given is a "link" name in the Olson database, the object
created may have a different name.  For example, there is a link from
the old "EST5EDT" name to "America/New_York".

When loading a time zone from the Olson database, the constructor
checks the version of the loaded class to make sure it matches the
version of the current DateTime::TimeZone installation. If they do not
match it will issue a warning. This is useful because time zone names
may fall out of use, but you may have an old module file installed for
that time zone.

There are also several special values that can be given as names.

If the "name" parameter is "floating", then a
C<DateTime::TimeZone::Floating> object is returned.  A floating time
zone does have I<any> offset, and is always the same time.  This is
useful for calendaring applications, which may need to specify that a
given event happens at the same I<local> time, regardless of where it
occurs.  See RFC 2445 for more details.

If the "name" parameter is "UTC", then a C<DateTime::TimeZone::UTC>
object is returned.

If the "name" is an offset string, it is converted to a number, and a
C<DateTime::TimeZone::OffsetOnly> object is returned.

=head3 The "local" time zone

If the "name" parameter is "local", then the module attempts to
determine the local time zone for the system.

The method for finding the local zone varies by operating system. See
the appropriate module for details of how we check for the local time
zone.

=over 4

=item * L<DateTime::TimeZone::Local::Unix>

=item * L<DateTime::TimeZone::Local::Win32>

=item * L<DateTime::TimeZone::Local::VMS>

=back

If a local time zone is not found, then an exception will be thrown.

=head2 $tz->offset_for_datetime( $dt )

Given a C<DateTime> object, this method returns the offset in seconds
for the given datetime.  This takes into account historical time zone
information, as well as Daylight Saving Time.  The offset is
determined by looking at the object's UTC Rata Die days and seconds.

=head2 $tz->offset_for_local_datetime( $dt )

Given a C<DateTime> object, this method returns the offset in seconds
for the given datetime.  Unlike the previous method, this method uses
the local time's Rata Die days and seconds.  This should only be done
when the corresponding UTC time is not yet known, because local times
can be ambiguous due to Daylight Saving Time rules.

=head2 $tz->is_dst_for_datetime( $dt )

Given a C<DateTime> object, this method returns true if the DateTime is
currently in Daylight Saving Time.

=head2 $tz->name

Returns the name of the time zone.

=head2 $tz->short_name_for_datetime( $dt )

Given a C<DateTime> object, this method returns the "short name" for
the current observance and rule this datetime is in.  These are names
like "EST", "GMT", etc.

It is B<strongly> recommended that you do not rely on these names for
anything other than display.  These names are not official, and many
of them are simply the invention of the Olson database maintainers.
Moreover, these names are not unique.  For example, there is an "EST"
at both -0500 and +1000/+1100.

=head2 $tz->is_floating

Returns a boolean indicating whether or not this object represents a
floating time zone, as defined by RFC 2445.

=head2 $tz->is_utc

Indicates whether or not this object represents the UTC (GMT) time
zone.

=head2 $tz->has_dst_changes

Indicates whether or not this zone has I<ever> had a change to and
from DST, either in the past or future.

=head2 $tz->is_olson

Returns true if the time zone is a named time zone from the Olson
database.

=head2 $tz->category

Returns the part of the time zone name before the first slash.  For
example, the "America/Chicago" time zone would return "America".

=head2 DateTime::TimeZone->is_valid_name($name)

Given a string, this method returns a boolean value indicating whether
or not the string is a valid time zone name.  If you are using
C<DateTime::TimeZone::Alias>, any aliases you've created will be valid.

=head2 DateTime::TimeZone->all_names

This returns a pre-sorted list of all the time zone names.  This list
does not include link names.  In scalar context, it returns an array
reference, while in list context it returns an array.

=head2 DateTime::TimeZone->categories

This returns a list of all time zone categories.  In scalar context,
it returns an array reference, while in list context it returns an
array.

=head2 DateTime::TimeZone->links

This returns a hash of all time zone links, where the keys are the
old, deprecated names, and the values are the new names.  In scalar
context, it returns a hash reference, while in list context it returns
a hash.

=head2 DateTime::TimeZone->names_in_category( $category )

Given a valid category, this method returns a list of the names in
that category, without the category portion.  So the list for the
"America" category would include the strings "Chicago",
"Kentucky/Monticello", and "New_York". In scalar context, it returns
an array reference, while in list context it returns an array.

The list is returned in order of population by zone, which should mean
that this order will be the best to use for most UIs.

=head2 DateTime::TimeZone->countries()

Returns a sorted list of all the valid country codes (in lower-case)
which can be passed to C<names_in_country()>. In scalar context, it
returns an array reference, while in list context it returns an array.

If you need to convert country codes to names or vice versa you can
use C<Locale::Country> to do so.

=head2 DateTime::TimeZone->names_in_country( $country_code )

Given a two-letter ISO3166 country code, this method returns a list of
time zones used in that country. The country code may be of any
case. In scalar context, it returns an array reference, while in list
context it returns an array.

=head2 DateTime::TimeZone->offset_as_seconds( $offset )

Given an offset as a string, this returns the number of seconds
represented by the offset as a positive or negative number.  Returns
C<undef> if $offset is not in the range C<-99:59:59> to C<+99:59:59>.

The offset is expected to match either
C</^([\+\-])?(\d\d?):(\d\d)(?::(\d\d))?$/> or
C</^([\+\-])?(\d\d)(\d\d)(\d\d)?$/>.  If it doesn't match either of
these, C<undef> will be returned.

This means that if you want to specify hours as a single digit, then
each element of the offset must be separated by a colon (:).

=head2 DateTime::TimeZone->offset_as_string( $offset )

Given an offset as a number, this returns the offset as a string.
Returns C<undef> if $offset is not in the range C<-359999> to C<359999>.

=head2 Storable Hooks

This module provides freeze and thaw hooks for C<Storable> so that the
huge data structures for Olson time zones are not actually stored in
the serialized structure.

If you subclass C<DateTime::TimeZone>, you will inherit its hooks,
which may not work for your module, so please test the interaction of
your module with Storable.

=head1 SUPPORT

Support for this module is provided via the datetime@perl.org email list. See
http://datetime.perl.org/wiki/datetime/page/Mailing_List for details.

Please submit bugs to the CPAN RT system at
http://rt.cpan.org/NoAuth/ReportBug.html?Queue=datetime%3A%3Atimezone
or via email at bug-datetime-timezone@rt.cpan.org.

=head1 DONATIONS

If you'd like to thank me for the work I've done on this module,
please consider making a "donation" to me via PayPal. I spend a lot of
free time creating free software, and would appreciate any support
you'd care to offer.

Please note that B<I am not suggesting that you must do this> in order
for me to continue working on this particular software. I will
continue to do so, inasmuch as I have in the past, for as long as it
interests me.

Similarly, a donation made in this way will probably not make me work
on this software much more, unless I get so many donations that I can
consider working on free software full time, which seems unlikely at
best.

To donate, log into PayPal and send money to autarch@urth.org or use
the button on this page:
L<http://www.urth.org/~autarch/fs-donation.html>

=head1 CREDITS

This module was inspired by Jesse Vincent's work on
Date::ICal::Timezone, and written with much help from the
datetime@perl.org list.

=head1 SEE ALSO

datetime@perl.org mailing list

http://datetime.perl.org/

The tools directory of the DateTime::TimeZone distribution includes
two scripts that may be of interest to some people.  They are
parse_olson and tests_from_zdump.  Please run them with the --help
flag to see what they can be used for.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2012 by Dave Rolsky.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut


__END__

