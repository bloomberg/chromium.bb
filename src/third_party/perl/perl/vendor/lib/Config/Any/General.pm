package Config::Any::General;

use strict;
use warnings;

use base 'Config::Any::Base';

=head1 NAME

Config::Any::General - Load Config::General files

=head1 DESCRIPTION

Loads Config::General files. Example:

    name = TestApp
    <Component Controller::Foo>
        foo bar
        bar [ arrayref-value ]
    </Component>
    <Model Baz>
        qux xyzzy
    </Model>

=head1 METHODS

=head2 extensions( )

return an array of valid extensions (C<cnf>, C<conf>).

=cut

sub extensions {
    return qw( cnf conf );
}

=head2 load( $file )

Attempts to load C<$file> via Config::General.

=cut

sub load {
    my $class = shift;
    my $file  = shift;
    my $args  = shift || {};

    $args->{ -ConfigFile } = $file;

    require Config::General;
    Config::General->VERSION( '2.47' );

    $args->{ -ForceArray } = 1 unless exists $args->{ -ForceArray };

    my $configfile = Config::General->new( %$args );
    my $config     = { $configfile->getall };

    return $config;
}

=head2 requires_all_of( )

Specifies that this module requires L<Config::General> in order to work.

=cut

sub requires_all_of { [ 'Config::General' => '2.47' ] }

=head1 AUTHOR

Brian Cassidy <bricas@cpan.org>

=head1 CONTRIBUTORS

Joel Bernstein <rataxis@cpan.org>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2016 by Brian Cassidy

Portions Copyright 2006 Portugal Telecom

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 SEE ALSO

=over 4

=item * L<Catalyst>

=item * L<Config::Any>

=item * L<Config::General>

=back

=cut

1;
