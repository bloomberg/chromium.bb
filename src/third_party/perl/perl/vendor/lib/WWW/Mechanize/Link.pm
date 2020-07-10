package WWW::Mechanize::Link;

use strict;
use warnings;

our $VERSION = '1.91';

#ABSTRACT: Link object for WWW::Mechanize


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


sub url   { return ($_[0])->[0]; }
sub text  { return ($_[0])->[1]; }
sub name  { return ($_[0])->[2]; }
sub tag   { return ($_[0])->[3]; }
sub base  { return ($_[0])->[4]; }
sub attrs { return ($_[0])->[5]; }


sub URI {
    my $self = shift;

    require URI::URL;
    my $URI = URI::URL->new( $self->url, $self->base );

    return $URI;
}


sub url_abs {
    my $self = shift;

    return $self->URI->abs;
}


1;

__END__

=pod

=encoding UTF-8

=head1 NAME

WWW::Mechanize::Link - Link object for WWW::Mechanize

=head1 VERSION

version 1.91

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

=head2 $link->URI()

Returns the URL as a L<URI::URL> object.

=head2 $link->url_abs()

Returns a L<URI::URL> object for the absolute form of the string.

=head1 SEE ALSO

L<WWW::Mechanize> and L<WWW::Mechanize::Image>

=head1 AUTHOR

Andy Lester <andy at petdance.com>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2004-2016 by Andy Lester.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
