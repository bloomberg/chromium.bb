package Crypt::RSA::ES::OAEP; 
use strict;
use warnings;

## Crypt::RSA::ES::OAEP 
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.

use base 'Crypt::RSA::Errorhandler';
use Math::Prime::Util  qw/random_bytes/;
use Crypt::RSA::DataFormat qw(bitsize os2ip i2osp octet_xor mgf1 octet_len);
use Crypt::RSA::Primitives;
use Crypt::RSA::Debug      qw(debug);
use Digest::SHA            qw(sha1);
use Sort::Versions         qw(versioncmp);
use Carp;

$Crypt::RSA::ES::OAEP::VERSION = '1.99';

sub new { 
    my ($class, %params) = @_;
    my $self = bless { primitives => new Crypt::RSA::Primitives, 
                       P          => "",
                       hlen       => 20,
                       VERSION    => $Crypt::RSA::ES::OAEP::VERSION,
                      }, $class;
    if ($params{Version}) { 
        if (versioncmp($params{Version}, '1.15') == -1) { 
            $$self{P} = "Crypt::RSA"; 
            $$self{VERSION} = $params{Version};
        } elsif (versioncmp($params{Version}, $$self{VERSION}) == 1) { 
            croak "Required version ($params{Version}) greater than installed version ($$self{VERSION}) of $class.\n";
        }
    }
    return $self;
}


sub encrypt { 
    my ($self, %params) = @_; 
    my $key = $params{Key}; my $M = $params{Message} || $params{Plaintext};
    return $self->error ("Missing Message or Plaintext parameter", \$key, \%params) unless $M;
    return $self->error ($key->errstr, \$M, $key, \%params) unless $key->check;
    my $k = octet_len ($key->n);  debug ("octet_len of modulus: $k");
    my $em = $self->encode ($M, $self->{P}, $k-1) || 
        return $self->error ($self->errstr, \$M, $key, \%params);
    my $m = os2ip ($em);
    my $c = $self->{primitives}->core_encrypt ( Plaintext => $m, Key => $key );
    my $ec = i2osp ($c, $k);  debug ("ec: $ec");
    return $ec;
}    


sub decrypt { 
    my ($self, %params) = @_;
    my $key = $params{Key}; my $C = $params{Cyphertext} || $params{Ciphertext}; 
    return $self->error ("Missing Cyphertext or Ciphertext parameter", \$key, \%params) unless $C;
    return $self->error ($key->errstr, $key, \%params) unless $key->check;
    my $k = octet_len ($key->n);  
    my $c = os2ip ($C);
    if (bitsize($c) > bitsize($key->n)) { 
        return $self->error ("Decryption error.", $key, \%params) 
    }
    my $m = $self->{primitives}->core_decrypt (Cyphertext => $c, Key => $key) || 
        return $self->error ("Decryption error.", $key, \%params);
    my $em = i2osp ($m, $k-1) || 
        return $self->error ("Decryption error.", $key, \%params);
    my $M; $self->errstrrst;  # reset the errstr
    unless ($M = $self->decode ($em, $$self{P})) { 
        return $self->error ("Decryption error.", $key, \%params) if $self->errstr();
        return $M;
    } 
    return $M;
} 


sub encode { 
    my ($self, $M, $P, $emlen) = @_; 
    my $hlen = $$self{hlen};
    my $mlen = length($M);
    return $self->error ("Message too long.", \$P, \$M) if $mlen > $emlen-(2*$hlen)-1;
    my ($PS, $pslen) = ("", 0);
    if ($pslen = $emlen-(2*$hlen+1+$mlen)) { 
        $PS = chr(0)x$pslen;
    }
    my $phash = $self->hash ($P);
    my $db = $phash . $PS . chr(1) . $M; 
    my $seed = random_bytes($hlen);
    my $dbmask = $self->mgf ($seed, $emlen-$hlen);
    my $maskeddb = octet_xor ($db, $dbmask);
    my $seedmask = $self->mgf ($maskeddb, $hlen);
    my $maskedseed = octet_xor ($seed, $seedmask);
    my $em = $maskedseed . $maskeddb;

    debug ("emlen == $emlen");
    debug ("M == $M [" . length($M) . "]"); 
    debug ("PS == $PS [$pslen]"); 
    debug ("phash == $phash [" . length($phash) . "]"); 
    debug ("seed == $seed [" . length($seed) . "]"); 
    debug ("seedmask == $seedmask [" . length($seedmask) . "]"); 
    debug ("db == $db [" . length($db) . "]"); 
    debug ("dbmask == $dbmask [" . length($dbmask) . "]"); 
    debug ("maskeddb == $maskeddb [" . length($maskeddb) . "]"); 
    debug ("em == $em [" . length($em) . "]"); 

    return $em;
}


sub decode { 
    my ($self, $em, $P) = @_; 
    my $hlen = $$self{hlen};

    debug ("P == $P");
    return $self->error ("Decoding error.", \$P) if length($em) < 2*$hlen+1;
    my $maskedseed = substr $em, 0, $hlen; 
    my $maskeddb = substr $em, $hlen; 
    my $seedmask = $self->mgf ($maskeddb, $hlen);
    my $seed = octet_xor ($maskedseed, $seedmask);
    my $dbmask = $self->mgf ($seed, length($em) - $hlen);
    my $db = octet_xor ($maskeddb, $dbmask); 
    my $phash = $self->hash ($P); 

    debug ("em == $em [" . length($em) . "]"); 
    debug ("phash == $phash [" . length($phash) . "]"); 
    debug ("seed == $seed [" . length($seed) . "]"); 
    debug ("seedmask == $seedmask [" . length($seedmask) . "]"); 
    debug ("maskedseed == $maskedseed [" . length($maskedseed) . "]"); 
    debug ("db == $db [" . length($db) . "]"); 
    debug ("maskeddb == $maskeddb [" . length($maskeddb) . "]"); 
    debug ("dbmask == $dbmask [" . length($dbmask) . "]"); 

    my ($phashorig) = substr $db, 0, $hlen;
    debug ("phashorig == $phashorig [" . length($phashorig) . "]"); 
    return $self->error ("Decoding error.", \$P) unless $phashorig eq $phash; 
    $db = substr $db, $hlen;
    my ($chr0, $chr1) = (chr(0), chr(1));
    my ($ps, $m);
    debug ("db == $db [" . length($db) . "]"); 
    unless ( ($ps, undef, $m) = $db =~ /^($chr0*)($chr1)(.*)$/s ) { 
        return $self->error ("Decoding error.", \$P);
    } 

    return $m;
}


sub hash { 
    my ($self, $data) = @_;
    return sha1 ($data);
}


sub mgf {
    my ($self, @data) = @_;
    return mgf1 (@data);
}


sub encryptblock { 
    my ($self, %params) = @_;
    return octet_len ($params{Key}->n) - 42;
} 


sub decryptblock { 
    my ($self, %params) = @_;
    return octet_len ($params{Key}->n);
}


# should be able to call this as a class method.
sub version {
    my $self = shift;
    return $self->{VERSION};
}


1;

=head1 NAME

Crypt::RSA::ES::OAEP - Plaintext-aware encryption with RSA. 

=head1 SYNOPSIS

    my $oaep = new Crypt::RSA::ES::OAEP; 

    my $ct = $oaep->encrypt( Key => $key, Message => $message ) || 
                die $oaep->errstr; 

    my $pt = $oaep->decrypt( Key => $key, Cyphertext => $ct )   || 
                die $oaep->errstr; 

=head1 DESCRIPTION

This module implements Optimal Asymmetric Encryption, a plaintext-aware
encryption scheme based on RSA. The notion of plaintext-aware implies it's
computationally infeasible to obtain full or partial information about a
message from a cyphertext, and computationally infeasible to generate a
valid cyphertext without knowing the corresponding message.
Plaintext-aware schemes, such as OAEP, are semantically secure,
non-malleable and secure against chosen-ciphertext attack. For more
information on OAEP and plaintext-aware encryption, see [3], [9] & [13].

=head1 METHODS

=head2 B<new()>

Constructor. 

=head2 B<version()>

Returns the version number of the module.

=head2 B<encrypt()>

Encrypts a string with a public key and returns the encrypted string
on success. encrypt() takes a hash argument with the following
mandatory keys:

=over 4

=item B<Message>

A string to be encrypted. The length of this string should not exceed k-42
octets, where k is the octet length of the RSA modulus. If Message is
longer than k-42, the method will fail and set $self->errstr to "Message
too long." This means the key must be at least _336_ bits long if you are
to use OAEP.

=item B<Key>

Public key of the recipient, a Crypt::RSA::Key::Public object.

=back

=head2 B<decrypt()>

Decrypts cyphertext with a private key and returns plaintext on
success. $self->errstr is set to "Decryption Error." or appropriate
error on failure. decrypt() takes a hash argument with the following
mandatory keys:

=over 4

=item B<Cyphertext>

A string encrypted with encrypt(). The length of the cyphertext must be k
octets, where k is the length of the RSA modulus.

=item B<Key>

Private key of the receiver, a Crypt::RSA::Key::Private object.

=item B<Version>

Version of the module that was used for creating the Cyphertext. This is
an optional argument. When present, decrypt() will ensure before
proceeding that the installed version of the module can successfully
decrypt the Cyphertext.

=back

=head1 ERROR HANDLING

See ERROR HANDLING in Crypt::RSA(3) manpage.

=head1 BIBLIOGRAPHY 

See BIBLIOGRAPHY in Crypt::RSA(3) manpage.

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=head1 SEE ALSO 

Crypt::RSA(3), Crypt::RSA::Primitives(3), Crypt::RSA::Keys(3),
Crypt::RSA::SSA::PSS(3)

=cut


