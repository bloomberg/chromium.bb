package Config::Any::XML;

use strict;
use warnings;

use base 'Config::Any::Base';

=head1 NAME

Config::Any::XML - Load XML config files

=head1 DESCRIPTION

Loads XML files. Example:

    <config>
        <name>TestApp</name>
        <component name="Controller::Foo">
            <foo>bar</foo>
        </component>
        <model name="Baz">
            <qux>xyzzy</qux>
        </model>
    </config>

=head1 METHODS

=head2 extensions( )

return an array of valid extensions (C<xml>).

=cut

sub extensions {
    return qw( xml );
}

=head2 load( $file )

Attempts to load C<$file> as an XML file.

=cut

sub load {
    my $class = shift;
    my $file  = shift;
    my $args  = shift || {};

    require XML::Simple;
    my $config = XML::Simple::XMLin(
        $file,
        ForceArray => [ qw( component model view controller ) ],
        %$args
    );

    return $class->_coerce( $config );
}

sub _coerce {
    # coerce the XML-parsed config into the correct format
    my $class  = shift;
    my $config = shift;
    my $out;
    for my $k ( keys %$config ) {
        my $ref = $config->{ $k };
        my $name = ref $ref eq 'HASH' ? delete $ref->{ name } : undef;
        if ( defined $name ) {
            $out->{ $k }->{ $name } = $ref;
        }
        else {
            $out->{ $k } = $ref;
        }
    }
    $out;
}

=head2 requires_all_of( )

Specifies that this module requires L<XML::Simple> and L<XML::NamespaceSupport>
in order to work.

=cut

sub requires_all_of { 'XML::Simple', 'XML::NamespaceSupport' }

=head1 CAVEATS

=head2 Strict Mode

If, by some chance, L<XML::Simple> has already been loaded with the strict
flag turned on, then you will likely get errors as warnings will become
fatal exceptions and certain arguments to XMLin() will no longer be optional.

See L<XML::Simple's strict mode documentation|XML::Simple/STRICT_MODE> for
more information.

=head1 AUTHORS

Brian Cassidy <bricas@cpan.org>

Joel Bernstein <rataxis@cpan.org>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2016 by Brian Cassidy

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 SEE ALSO

=over 4

=item * L<Catalyst>

=item * L<Config::Any>

=item * L<XML::Simple>

=back

=cut

1;
