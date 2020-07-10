package Spreadsheet::ParseXLSX::Decryptor::Standard;
our $AUTHORITY = 'cpan:DOY';
$Spreadsheet::ParseXLSX::Decryptor::Standard::VERSION = '0.27';
use strict;
use warnings;

use base 'Spreadsheet::ParseXLSX::Decryptor';

sub decrypt {
    my $self = shift;
    my ($encryptedValue) = @_;

    my $key = $self->_generateDecryptionKey("\x00" x 4);
    my $ecb = Crypt::Mode::ECB->new($self->{cipherAlgorithm}, 0);
    return $ecb->decrypt($encryptedValue, $key);
}

sub decryptFile {
    my $self = shift;
    my ($inFile, $outFile, $bufferLength, $fileSize) = @_;

    my $key = $self->_generateDecryptionKey("\x00" x 4);
    my $ecb = Crypt::Mode::ECB->new($self->{cipherAlgorithm}, 0);

    my $inbuf;
    my $i = 0;

    while (($fileSize > 0) && (my $inlen = $inFile->read($inbuf, $bufferLength))) {
        if ($inlen < $bufferLength) {
            $inbuf .= "\x00" x ($bufferLength - $inlen);
        }

        my $outbuf = $ecb->decrypt($inbuf, $key);
        if ($fileSize < $inlen) {
            $inlen = $fileSize;
        }

        $outFile->write($outbuf, $inlen);
        $i++;
        $fileSize -= $inlen;
    }
}

sub _generateDecryptionKey {
    my $self = shift;
    my ($blockKey) = @_;

    my $hash;
    unless ($self->{pregeneratedKey}) {
        $hash = $self->{hashProc}->($self->{salt} . Encode::encode('UTF-16LE', $self->{password}));
        for (my $i = 0; $i < $self->{spinCount}; $i++) {
            $hash = $self->{hashProc}->(pack('L<', $i) . $hash);
        }
        $self->{pregeneratedKey} = $hash;
    }

    $hash = $self->{hashProc}->($self->{pregeneratedKey} . $blockKey);

    my $x1 = $self->{hashProc}->(("\x36" x 64) ^ $hash);
    if (length($x1) >= $self->{keyLength}) {
        $hash = substr($x1, 0, $self->{keyLength});
    } else {
        my $x2 = $self->{hashProc}->(("\x5C" x 64) ^ $hash);
        $hash = substr($x1 . $x2, 0, $self->{keyLength});
    }

    return $hash;
}

sub verifyPassword {
    my $self = shift;
    my ($encryptedVerifier, $encryptedVerifierHash) = @_;

    my $verifier = $self->decrypt($encryptedVerifier);
    my $verifierHash = $self->decrypt($encryptedVerifierHash);

    my $verifierHash0 = $self->{hashProc}->($verifier);

    die "Wrong password: $self" unless ($verifierHash0 eq substr($verifierHash, 0, length($verifierHash0)));
}


1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Spreadsheet::ParseXLSX::Decryptor::Standard

=head1 VERSION

version 0.27

=for Pod::Coverage   decrypt
  decryptFile
  verifyPassword

=head1 AUTHOR

Jesse Luehrs <doy@tozt.net>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2016 by Jesse Luehrs.

This is free software, licensed under:

  The MIT (X11) License

=cut
