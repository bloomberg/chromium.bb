package Spreadsheet::ParseXLSX::Decryptor::Agile;
our $AUTHORITY = 'cpan:DOY';
$Spreadsheet::ParseXLSX::Decryptor::Agile::VERSION = '0.27';
use strict;
use warnings;

use base 'Spreadsheet::ParseXLSX::Decryptor';

sub decrypt {
    my $self = shift;
    my ($encryptedValue, $blockKey) = @_;

    my $key = $self->_generateDecryptionKey($blockKey);
    my $iv = $self->_generateInitializationVector('', $self->{blockSize});
    my $cbc = Crypt::Mode::CBC->new($self->{cipherAlgorithm}, 0);
    return $cbc->decrypt($encryptedValue, $key, $iv);
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

    if (length($hash) > $self->{keyLength}) {
        $hash = substr($hash, 0, $self->{keyLength});
    } elsif (length($hash) < $self->{keyLength}) {
        $hash .= "\x36" x ($self->{keyLength} - length($hash));
    }
    return $hash;
}

sub _generateInitializationVector {
    my $self = shift;
    my ($blockKey, $blockSize) = @_;

    my $iv;
    if ($blockKey) {
        $iv = $self->{hashProc}->($self->{salt} . $blockKey);
    } else {
        $iv = $self->{salt};
    }

    if (length($iv) > $blockSize) {
        $iv = substr($iv, 0, $blockSize);
    } elsif (length($iv) < $blockSize) {
        $iv = $iv . ("\x36" x ($blockSize - length($iv)));
    }

    return $iv;
}

sub decryptFile {
    my $self = shift;
    my ($inFile, $outFile, $bufferLength, $key, $fileSize) = @_;

    my $cbc = Crypt::Mode::CBC->new($self->{cipherAlgorithm}, 0);

    my $inbuf;
    my $i = 0;

    while (($fileSize > 0) && (my $inlen = $inFile->read($inbuf, $bufferLength))) {
        my $blockId = pack('L<', $i);

        my $iv = $self->_generateInitializationVector($blockId, $self->{blockSize});

        if ($inlen < $bufferLength) {
            $inbuf .= "\x00" x ($bufferLength - $inlen);
        }

        my $outbuf = $cbc->decrypt($inbuf, $key, $iv);
        if ($fileSize < $inlen) {
            $inlen = $fileSize;
        }

        $outFile->write($outbuf, $inlen);
        $i++;
        $fileSize -= $inlen;
    }
}

sub verifyPassword {
    my $self = shift;
    my ($encryptedVerifier, $encryptedVerifierHash) = @_;

    my $encryptedVerifierHash0 = $self->{hashProc}->($self->decrypt($encryptedVerifier, "\xfe\xa7\xd2\x76\x3b\x4b\x9e\x79"));
    $encryptedVerifierHash = $self->decrypt($encryptedVerifierHash, "\xd7\xaa\x0f\x6d\x30\x61\x34\x4e");

    die "Wrong password: $self" unless ($encryptedVerifierHash0 eq $encryptedVerifierHash);
}


1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Spreadsheet::ParseXLSX::Decryptor::Agile

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
