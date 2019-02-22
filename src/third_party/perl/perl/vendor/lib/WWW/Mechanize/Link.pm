package WWW::Mechanize::Link;

use strict;
use warnings;

=head1 NAME

WWW::Mechanize::Link - Link object for WWW::Mechanize

=head1 SYNOPSIS

Link object to encapsulate all the stuff that Mech needs but nobody
wants to deal with as an array.

=head1 Constructor

=head2 new()

    my $link = WWW::Mechanize::Link->new( {
        url  => $url,
        text => $text,
        name => $name,
        tag  => $tag,
        base => $base,
        attr => $attr_href,
    } );

For compatibility, this older interface is also supported:

 new( $url, $text, $name, $tag, $base, $attr_href )

Creates and returns a new C<WWW::Mechanize::Link> object.

=cut

sub new {
    my $class = shift;

    my $self;

    # The order of the first four must stay as they are for
    # compatibility with older code.
    if ( ref $_[0] eq 'HASH' ) {
        $self = [ @{$_[0]}{ qw( url text name tag base attrs ) } ];
    }
    else {
        $self = [ @_ ];
    }

    return bless $self, $class;
}

=head1 Accessors

=head2 $link->url()

URL from the link

=head2 $link->text()

Text of the link

=head2 $link->name()

NAME attribute from the source tag, if any.

=head2 $link->tag()

Tag name (one of: "a", "area", "frame", "iframe" or "meta").

=head2 $link->base()

Base URL to which the links are relative.

=head2 $link->attrs()

Returns hash ref of all the attributes and attribute values in the tag. 

=cut

sub url   { return ($_[0])->[0]; }
sub text  { return ($_[0])->[1]; }
sub name  { return ($_[0])->[2]; }
sub tag   { return ($_[0])->[3]; }
sub base  { return ($_[0])->[4]; }
sub attrs { return ($_[0])->[5]; }

=head2 $link->URI()

Returns the URL as a L<URI::URL> object.

=cut

sub URI {
    my $self = shift;

    require URI::URL;
    my $URI = URI::URL->new( $self->url, $self->base );

    return $URI;
}

=head2 $link->url_abs()

Returns a L<URI::URL> object for the absolute form of the string.

=cut

sub url_abs {
    my $self = shift;

    return $self->URI->abs;
}

=head1 SEE ALSO

L<WWW::Mechanize> and L<WWW::Mechanize::Image>

=head1 COPYRIGHT & LICENSE

Copyright 2004-2010 Andy Lester.

This program is free software; you can redistribute it and/or modify
it under the terms of either:

=over 4

=item * the GNU General Public License as published by the Free
Software Foundation; either version 1, or (at your option) any later
version, or

=item * the Artistic License version 2.0.

=back

=cut

# vi:et:sw=4 ts=4

1;
