package Config::Any::JSON;

use strict;
use warnings;

use base 'Config::Any::Base';

=head1 NAME

Config::Any::JSON - Load JSON config files

=head1 DESCRIPTION

Loads JSON files. Example:

    {
        "name": "TestApp",
        "Controller::Foo": {
            "foo": "bar"
        },
        "Model::Baz": {
            "qux": "xyzzy"
        }
    }

=head1 METHODS

=head2 extensions( )

return an array of valid extensions (C<json>, C<jsn>).

=cut

sub extensions {
    return qw( json jsn );
}

=head2 load( $file )

Attempts to load C<$file> as a JSON file.

=cut

sub load {
    my $class = shift;
    my $file  = shift;

    open( my $fh, '<', $file ) or die $!;
    binmode $fh;
    my $content = do { local $/; <$fh> };
    close $fh;

    if ( eval { require Cpanel::JSON::XS } ) {
        my $decoder = Cpanel::JSON::XS->new->utf8->relaxed;
        return $decoder->decode( $content );
    }
    elsif ( eval { require JSON::MaybeXS } ) {
        my $decoder = JSON::MaybeXS::JSON()->new->utf8->relaxed;
        return $decoder->decode( $content );
    }
    elsif ( eval { require JSON::DWIW } ) {
        my $decoder = JSON::DWIW->new;
        my ( $data, $error ) = $decoder->from_json( $content );
        die $error if $error;
        return $data;
    }
    elsif ( eval { require JSON::XS } ) {
        my $decoder = JSON::XS->new->utf8->relaxed;
        return $decoder->decode( $content );
    }
    elsif ( eval { require JSON::Syck } ) {
        require Encode;
        return JSON::Syck::Load( Encode::decode('UTF-8', $content ) );
    }
    elsif ( eval { require JSON::PP; JSON::PP->VERSION( 2 ); } ) {
        my $decoder = JSON::PP->new->utf8->relaxed;
        return $decoder->decode( $content );
    }
    require JSON;
    if ( eval { JSON->VERSION( 2 ) } ) {
        return JSON::decode_json( $content );
    }
    else {
        return JSON::jsonToObj( $content );
    }
}

=head2 requires_any_of( )

Specifies that this modules requires one of, L<Cpanel::JSON::XS>,
L<JSON::MaybeXS>, L<JSON::DWIW>, L<JSON::XS>, L<JSON::Syck>, L<JSON::PP> or
L<JSON> in order to work.

=cut

sub requires_any_of { qw(
  Cpanel::JSON::XS
  JSON::MaybeXS
  JSON::DWIW
  JSON::XS
  JSON::Syck
  JSON::PP
  JSON
) }

=head1 AUTHOR

Brian Cassidy <bricas@cpan.org>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2016 by Brian Cassidy

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 SEE ALSO

=over 4

=item * L<Catalyst>

=item * L<Config::Any>

=item * L<Cpanel::JSON::XS>

=item * L<JSON::MaybeXS>

=item * L<JSON::DWIW>

=item * L<JSON::XS>

=item * L<JSON::Syck>

=item * L<JSON>

=back

=cut

1;
