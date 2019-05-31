use strict;
use warnings;
package Devel::PartialDump; # git description: v0.19-3-ga398185
# vim: set ts=8 sts=4 sw=4 tw=115 et :
# ABSTRACT: Partial dumping of data structures, optimized for argument printing.
# KEYWORDS: development debugging dump dumper diagnostics deep data structures

our $VERSION = '0.20';

use Carp ();
use Scalar::Util qw(looks_like_number reftype blessed);

use namespace::clean 0.19;

use Class::Tiny {
    max_length => undef,
    max_elements => 6,
    max_depth => 2,
    stringify => 0,
    pairs => 1,
    objects => 1,
    list_delim => ", ",
    pair_delim => ": ",
};

use Sub::Exporter -setup => {
    exports => [qw(dump warn show show_scalar croak carp confess cluck $default_dumper)],
    groups => {
        easy => [qw(dump warn show show_scalar carp croak)],
        carp => [qw(croak carp)],
    },
    collectors => {
        override_carp => sub {
            no warnings 'redefine';
            require Carp;
            *Carp::caller_info = \&replacement_caller_info;
        },
    },
};

# a replacement for Carp::caller_info
sub replacement_caller_info {
    my $i = shift(@_) + 1;

    package DB; # git description: v0.19-3-ga398185
    my %call_info;
    @call_info{
    qw(pack file line sub has_args wantarray evaltext is_require)
    } = caller($i);

    return unless (defined $call_info{pack});

    my $sub_name = Carp::get_subname(\%call_info);

    if ($call_info{has_args}) {
        $sub_name .= '(' . Devel::PartialDump::dump(@DB::args) . ')';
    }

    $call_info{sub_name} = $sub_name;

    return wantarray() ? %call_info : \%call_info;
}


sub warn_str {
    my ( @args ) = @_;
    my $self;

    if ( blessed($args[0]) and $args[0]->isa(__PACKAGE__) ) {
        $self = shift @args;
    } else {
        $self = our $default_dumper;
    }
    return $self->_join(
        map {
            !ref($_) && defined($_)
            ? $_
            : $self->dump($_)
        } @args
    );
}

sub warn {
    Carp::carp(warn_str(@_));
}

foreach my $f ( qw(carp croak confess cluck) ) {
    no warnings 'redefine';
    eval "sub $f {
        local \$Carp::CarpLevel = \$Carp::CarpLevel + 1;
        Carp::$f(warn_str(\@_));
    }";
}

sub show {
    my ( @args ) = @_;
    my $self;

    if ( blessed($args[0]) and $args[0]->isa(__PACKAGE__) ) {
        $self = shift @args;
    } else {
        $self = our $default_dumper;
    }

    $self->warn(@args);

    return ( @args == 1 ? $args[0] : @args );
}

sub show_scalar ($) { goto \&show }

sub _join {
    my ( $self, @strings ) = @_;

    my $ret = "";

    if ( @strings ) {
        my $sep = $, || $" || " ";
        my $re = qr/(?: \s| \Q$sep\E )$/x;

        my $last = pop @strings;

        foreach my $string ( @strings ) {
            $ret .= $string;
            $ret .= $sep unless $string =~ $re;
        }

        $ret .= $last;
    }

    return $ret;
}

sub dump {
    my ( @args ) = @_;
    my $self;

    if ( blessed($args[0]) and $args[0]->isa(__PACKAGE__) ) {
        $self = shift @args;
    } else {
        $self = our $default_dumper;
    }

    my $method = "dump_as_" . ( $self->should_dump_as_pairs(@args) ? "pairs" : "list" );

    my $dump = $self->$method(1, @args);

    if ( defined $self->max_length and length($dump) > $self->max_length ) {
        my $max_length = $self->max_length - 3;
        $max_length = 0 if $max_length < 0;
        substr( $dump, $max_length, length($dump) - $max_length ) = '...';
    }

    if ( not defined wantarray ) {
        CORE::warn "$dump\n";
    } else {
        return $dump;
    }
}

sub should_dump_as_pairs {
    my ( $self, @what ) = @_;

    return unless $self->pairs;

    return if @what % 2 != 0; # must be an even list

    for ( my $i = 0; $i < @what; $i += 2 ) {
        return if ref $what[$i]; # plain strings are keys
    }

    return 1;
}

sub dump_as_pairs {
    my ( $self, $depth, @what ) = @_;

    my $truncated;
    if ( defined $self->max_elements and ( @what / 2 ) > $self->max_elements ) {
        $truncated = 1;
        @what = splice(@what, 0, $self->max_elements * 2 );
    }

    return join( $self->list_delim, $self->_dump_as_pairs($depth, @what), ($truncated ? "..." : ()) );
}

sub _dump_as_pairs {
    my ( $self, $depth, @what ) = @_;

    return unless @what;

    my ( $key, $value, @rest ) = @what;

    return (
        ( $self->format_key($depth, $key) . $self->pair_delim . $self->format($depth, $value) ),
        $self->_dump_as_pairs($depth, @rest),
    );
}

sub dump_as_list {
    my ( $self, $depth, @what ) = @_;

    my $truncated;
    if ( defined $self->max_elements and @what > $self->max_elements ) {
        $truncated = 1;
        @what = splice(@what, 0, $self->max_elements );
    }

    return join( $self->list_delim, ( map { $self->format($depth, $_) } @what ), ($truncated ? "..." : ()) );
}

sub format {
    my ( $self, $depth, $value ) = @_;

    defined($value)
        ? ( ref($value)
            ? ( blessed($value)
                ? $self->format_object($depth, $value)
                : $self->format_ref($depth, $value) )
            : ( looks_like_number($value)
                ? $self->format_number($depth, $value)
                : $self->format_string($depth, $value) ) )
        : $self->format_undef($depth, $value),
}

sub format_key {
    my ( $self, $depth, $key ) = @_;
    return $key;
}

sub format_ref {
    my ( $self, $depth, $ref ) = @_;

    if ( $depth > $self->max_depth ) {
        return overload::StrVal($ref);
    } else {
        my $reftype = reftype($ref);
                $reftype = 'SCALAR'
                    if $reftype eq 'REF' || $reftype eq 'LVALUE';
        my $method = "format_" . lc $reftype;

        if ( $self->can($method) ) {
            return $self->$method( $depth, $ref );
        } else {
            return overload::StrVal($ref);
        }
    }
}

sub format_array {
    my ( $self, $depth, $array ) = @_;

    my $class = blessed($array) || '';
    $class .= "=" if $class;

    return $class . "[ " . $self->dump_as_list($depth + 1, @$array) . " ]";
}

sub format_hash {
    my ( $self, $depth, $hash ) = @_;

    my $class = blessed($hash) || '';
    $class .= "=" if $class;

    return $class . "{ " . $self->dump_as_pairs($depth + 1, map { $_ => $hash->{$_} } sort keys %$hash) . " }";
}

sub format_scalar {
    my ( $self, $depth, $scalar ) = @_;

    my $class = blessed($scalar) || '';
    $class .= "=" if $class;

    return $class . "\\" . $self->format($depth + 1, $$scalar);
}

sub format_object {
    my ( $self, $depth, $object ) = @_;

    if ( $self->objects ) {
        return $self->format_ref($depth, $object);
    } else {
        return $self->stringify ? "$object" : overload::StrVal($object);
    }
}

sub format_string {
    my ( $self, $depth, $str ) =@_;
    # FIXME use String::Escape ?

    # remove vertical whitespace
    $str =~ s/\n/\\n/g;
    $str =~ s/\r/\\r/g;

    # reformat nonprintables
    $str =~ s/(\P{IsPrint})/"\\x{" . sprintf("%x", ord($1)) . "}"/ge;

    $self->quote($str);
}

sub quote {
    my ( $self, $str ) = @_;

    qq{"$str"};
}

sub format_undef { "undef" }

sub format_number {
    my ( $self, $depth, $value ) = @_;
    return "$value";
}

our $default_dumper = __PACKAGE__->new;

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Devel::PartialDump - Partial dumping of data structures, optimized for argument printing.

=head1 VERSION

version 0.20

=head1 SYNOPSIS

    use Devel::PartialDump;

    sub foo {
        print "foo called with args: " . Devel::PartialDump->new->dump(@_);
    }

    use Devel::PartialDump qw(warn);

    # warn is overloaded to create a concise dump instead of stringifying $some_bad_data
    warn "this made a boo boo: ", $some_bad_data

=head1 DESCRIPTION

This module is a data dumper optimized for logging of arbitrary parameters.

It attempts to truncate overly verbose data, in a way that is hopefully more
useful for diagnostics warnings than

    warn Dumper(@stuff);

Unlike other data dumping modules there are no attempts at correctness or cross
referencing, this is only meant to provide a slightly deeper look into the data
in question.

There is a default recursion limit, and a default truncation of long lists, and
the dump is formatted on one line (new lines in strings are escaped), to aid in
readability.

You can enable it temporarily by importing functions like C<warn>, C<croak> etc
to get more informative errors during development, or even use it as:

    BEGIN { local $@; eval "use Devel::PartialDump qw(...)" }

to get DWIM formatting only if it's installed, without introducing a
dependency.

=head1 SAMPLE OUTPUT

=over 4

=item C<< "foo" >>

    "foo"

=item C<< "foo" => "bar" >>

    foo: "bar"

=item C<< foo => "bar", gorch => [ 1, "bah" ] >>

    foo: "bar", gorch: [ 1, "bah" ]

=item C<< [ { foo => ["bar"] } ] >>

    [ { foo: ARRAY(0x9b265d0) } ]

=item C<< [ 1 .. 10 ] >>

    [ 1, 2, 3, 4, 5, 6, ... ]

=item C<< "foo\nbar" >>

    "foo\nbar"

=item C<< "foo" . chr(1) >>

    "foo\x{1}"

=back

=head1 ATTRIBUTES

=over 4

=item max_length

The maximum character length of the dump.

Anything bigger than this will be truncated.

Not defined by default.

=item max_elements

The maximum number of elements (array elements or pairs in a hash) to print.

Defaults to 6.

=item max_depth

The maximum level of recursion.

Defaults to 2.

=item stringify

Whether or not to let objects stringify themselves, instead of using
L<overload/StrVal> to avoid side effects.

Defaults to false (no overloading).

=item pairs

=for stopwords autodetect

Whether or not to autodetect named args as pairs in the main C<dump> function.
If this attribute is true, and the top level value list is even sized, and
every odd element is not a reference, then it will dumped as pairs instead of a
list.

=back

=head1 EXPORTS

All exports are optional, nothing is exported by default.

This module uses L<Sub::Exporter>, so exports can be renamed, curried, etc.

=over 4

=item warn

=item show

=item show_scalar

=item croak

=item carp

=item confess

=item cluck

=item dump

See the various methods for behavior documentation.

These methods will use C<$Devel::PartialDump::default_dumper> as the invocant if the
first argument is not blessed and C<isa> L<Devel::PartialDump>, so they can be
used as functions too.

Particularly C<warn> can be used as a drop in replacement for the built in
warn:

    warn "blah blah: ", $some_data;

by importing

    use Devel::PartialDump qw(warn);

C<$some_data> will be have some of it's data dumped.

=item $default_dumper

The default dumper object to use for export style calls.

Can be assigned to to alter behavior globally.

This is generally useful when using the C<warn> export as a drop in replacement
for C<CORE::warn>.

=back

=head1 METHODS

=over 4

=item warn @blah

A wrapper for C<dump> that prints strings plainly.

=item show @blah

=item show_scalar $x

Like C<warn>, but instead of returning the value from C<warn> it returns its
arguments, so it can be used in the middle of an expression.

Note that

    my $x = show foo();

will actually evaluate C<foo> in list context, so if you only want to dump a
single element and retain scalar context use

    my $x = show_scalar foo();

which has a prototype of C<$> (as opposed to taking a list).

=for stopwords Ingy

This is similar to the venerable Ingy's fabulous and amazing L<XXX> module.

=item carp

=item croak

=item confess

=item cluck

Drop in replacements for L<Carp> exports, that format their arguments like
C<warn>.

=item dump @stuff

Returns a one line, human readable, concise dump of @stuff.

If called in void context, will C<warn> with the dump.

Truncates the dump according to C<max_length> if specified.

=item dump_as_list $depth, @stuff

=item dump_as_pairs $depth, @stuff

Dump C<@stuff> using the various formatting functions.

Dump as pairs returns comma delimited pairs with C<< => >> between the key and the value.

Dump as list returns a comma delimited dump of the values.

=item format $depth, $value

=item format_key $depth, $key

=item format_object $depth, $object

=item format_ref $depth, $Ref

=item format_array $depth, $array_ref

=item format_hash $depth, $hash_ref

=item format_undef $depth, undef

=item format_string $depth, $string

=item format_number $depth, $number

=item quote $string

The various formatting methods.

You can override these to provide a custom format.

C<format_array> and C<format_hash> recurse with C<$depth + 1> into
C<dump_as_list> and C<dump_as_pairs> respectively.

C<format_ref> delegates to C<format_array> and C<format_hash> and does the
C<max_depth> tracking. It will simply stringify the ref if the recursion limit
has been reached.

=back

=head1 SUPPORT

Bugs may be submitted through L<the RT bug tracker|https://rt.cpan.org/Public/Dist/Display.html?Name=Devel-PartialDump>
(or L<bug-Devel-PartialDump@rt.cpan.org|mailto:bug-Devel-PartialDump@rt.cpan.org>).

There is also a mailing list available for users of this distribution, at
L<http://lists.perl.org/list/moose.html>.

There is also an irc channel available for users of this distribution, at
L<C<#moose> on C<irc.perl.org>|irc://irc.perl.org/#moose>.

=head1 AUTHOR

יובל קוג'מן (Yuval Kogman) <nothingmuch@woobling.org>

=head1 CONTRIBUTORS

=for stopwords Karen Etheridge Florian Ragwitz Steven Lee Leo Lapworth Jesse Luehrs David Golden Paul Howarth

=over 4

=item *

Karen Etheridge <ether@cpan.org>

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Steven Lee <stevenwh.lee@gmail.com>

=item *

Leo Lapworth <web@web-teams-computer.local>

=item *

Jesse Luehrs <doy@tozt.net>

=item *

David Golden <dagolden@cpan.org>

=item *

Paul Howarth <paul@city-fan.org>

=back

=head1 COPYRIGHT AND LICENCE

This software is copyright (c) 2008 by יובל קוג'מן (Yuval Kogman).

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
