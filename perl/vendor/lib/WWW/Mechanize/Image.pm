package WWW::Mechanize::Image;

use strict;
use warnings;

our $VERSION = '1.91';

#ABSTRACT: Image object for WWW::Mechanize


sub new {
    my $class = shift;
    my $parms = shift || {};

    my $self = bless {}, $class;

    for my $parm ( qw( url base tag height width alt name attrs ) ) {
        # Check for what we passed in, not whether it's defined
        $self->{$parm} = $parms->{$parm} if exists $parms->{$parm};
    }

    # url and tag are always required
    for ( qw( url tag ) ) {
        exists $self->{$_} or die "WWW::Mechanize::Image->new must have a $_ argument";
    }

    return $self;
}


sub url     { return ($_[0])->{url}; }
sub base    { return ($_[0])->{base}; }
sub name    { return ($_[0])->{name}; }
sub tag     { return ($_[0])->{tag}; }
sub height  { return ($_[0])->{height}; }
sub width   { return ($_[0])->{width}; }
sub alt     { return ($_[0])->{alt}; }
sub attrs   { return ($_[0])->{attrs}; }


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

WWW::Mechanize::Image - Image object for WWW::Mechanize

=head1 VERSION

version 1.91

=head1 SYNOPSIS

Image object to encapsulate all the stuff that Mech needs

=head1 Constructor

=head2 new()

Creates and returns a new C<WWW::Mechanize::Image> object.

    my $image = WWW::Mechanize::Image->new( {
        url    => $url,
        base   => $base,
        tag    => $tag,
        name   => $name,     # From the INPUT tag
        height => $height,   # optional
        width  => $width,    # optional
        alt    => $alt,      # optional
        attrs  => $attr_ref, # optional
    } );

=head1 Accessors

=head2 $link->url()

URL from the link

=head2 $link->base()

Base URL to which the links are relative.

=head2 $link->name()

Name for the field from the NAME attribute, if any.

=head2 $link->tag()

Tag name (either "image" or "input")

=head2 $link->height()

Image height

=head2 $link->width()

Image width

=head2 $link->alt()

ALT attribute from the source tag, if any.

=head2 $link->attrs()

Hash ref of all the attributes and attribute values in the tag.

=head2 $link->URI()

Returns the URL as a L<URI::URL> object.

=head2 $link->url_abs()

Returns the URL as an absolute URL string.

=head1 SEE ALSO

L<WWW::Mechanize> and L<WWW::Mechanize::Link>

=head1 AUTHOR

Andy Lester <andy at petdance.com>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2004-2016 by Andy Lester.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
