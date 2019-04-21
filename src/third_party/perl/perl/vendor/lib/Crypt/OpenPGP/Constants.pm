package Crypt::OpenPGP::Constants;
use strict;

use vars qw( %CONSTANTS );

%CONSTANTS = (
    'PGP_PKT_PUBKEY_ENC' => 1,
    'PGP_PKT_SIGNATURE'  => 2,
    'PGP_PKT_SYMKEY_ENC' => 3,
    'PGP_PKT_ONEPASS_SIG' => 4,
    'PGP_PKT_SECRET_KEY'  => 5,
    'PGP_PKT_PUBLIC_KEY'  => 6,
    'PGP_PKT_SECRET_SUBKEY' => 7,
    'PGP_PKT_COMPRESSED'    => 8,
    'PGP_PKT_ENCRYPTED'     => 9,
    'PGP_PKT_MARKER'        => 10,
    'PGP_PKT_PLAINTEXT'     => 11,
    'PGP_PKT_RING_TRUST'    => 12,
    'PGP_PKT_USER_ID'       => 13,
    'PGP_PKT_PUBLIC_SUBKEY' => 14,
    'PGP_PKT_ENCRYPTED_MDC' => 18,
    'PGP_PKT_MDC'           => 19,

    'DEFAULT_CIPHER' => 2,
    'DEFAULT_DIGEST' => 2,
    'DEFAULT_COMPRESS' => 1,
);

use vars qw( %TAGS );
my %RULES = (
    '^PGP_PKT' => 'packet',
);

for my $re (keys %RULES) {
    $TAGS{ $RULES{$re} } = [ grep /$re/, keys %CONSTANTS ];
}

sub import {
    my $class = shift;

    my @to_export;
    my @args = @_;
    for my $item (@args) {
        push @to_export,
            $item =~ s/^:// ? @{ $TAGS{$item} } : $item;
    }

    no strict 'refs';
    my $pkg = caller;
    for my $con (@to_export) {
        warn __PACKAGE__, " does not export the constant '$con'"
            unless exists $CONSTANTS{$con};
        *{"${pkg}::$con"} = sub () { $CONSTANTS{$con} }
    }
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::Constants - Exportable constants

=head1 DESCRIPTION

I<Crypt::OpenPGP::Constants> provides a list of common and useful
constants for use in I<Crypt::OpenPGP>.

=head1 USAGE

None of the constants are exported by default; you have to ask for
them explicitly. Some of the constants are grouped into bundles that
you can grab all at once; alternatively you can just take the
individual constants, one by one.

If you wish to import a group, your I<use> statement should look
something like this:

    use Crypt::OpenPGP::Constants qw( :group );

Here are the groups:

=over 4

=item * packet

All of the I<PGP_PKT_*> constants. These are constants that define
packet types.

=back

Other exportable constants, not belonging to a group, are:

=over 4

=item * DEFAULT_CIPHER

=item * DEFAULT_DIGEST

=item * DEFAULT_COMPRESS

Default cipher, digest, and compression algorithms, to be used if no
specific cipher, digest, or compression algorithm is otherwise
specified.

=back

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
