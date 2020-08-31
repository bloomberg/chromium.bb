package Config::Any::Perl;

use strict;
use warnings;

use base 'Config::Any::Base';
use File::Spec;
use Cwd ();

=head1 NAME

Config::Any::Perl - Load Perl config files

=head1 DESCRIPTION

Loads Perl files. Example:

    {
        name => 'TestApp',
        'Controller::Foo' => {
            foo => 'bar'
        },
        'Model::Baz' => {
            qux => 'xyzzy'
        }
    }

=head1 METHODS

=head2 extensions( )

return an array of valid extensions (C<pl>, C<perl>).

=cut

sub extensions {
    return qw( pl perl );
}

=head2 load( $file )

Attempts to load C<$file> as a Perl file.

=cut

sub load {
    my $class = shift;
    my $file  = shift;

    my( $exception, $content );
    {
        local $@;
        # previously this would load based on . being in @INC, and wouldn't
        # trigger taint errors even if '.' probably should have been considered
        # tainted.  untaint for backwards compatibility.
        my ($cwd) = Cwd::cwd() =~ /\A(.*)\z/s;
        $content = do File::Spec->rel2abs($file, $cwd);
        $exception = $@ || $!
            if !defined $content;
    }
    die $exception if $exception;

    return $content;
}

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

=back

=cut

1;
