#!/usr/bin/perl -sw
##
## Crypt::RSA - Pure-perl implementation of RSA encryption/signing
##              algorithms.
##
## Copyright (c) 2000-2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: RSA.pm,v 1.48 2001/09/25 12:44:55 vipul Exp $

package Crypt::RSA;
use FindBin qw($Bin);
use lib "$Bin/../../lib";
use strict;
use base 'Class::Loader';
use base 'Crypt::RSA::Errorhandler';
use Crypt::RSA::Key;
use Crypt::RSA::DataFormat qw(steak octet_len);
use Convert::ASCII::Armour;
use Carp;

$Crypt::RSA::VERSION = '1.99';  # change this elsewhere too!

my %DEFAULTS = ( 
    'ES'    => { Name  => 'OAEP_ES'    },
    'SS'    => { Name  => 'PSS_SS'     },
    'PP'    => { Name  => 'ASCII_PP'   },
);


my %KNOWNMAP = (

  # ENCRYPTION SCHEMES

            OAEP_ES =>  { Module => "Crypt::RSA::ES::OAEP"      },
        PKCS1v21_ES =>  { Module => "Crypt::RSA::ES::OAEP"      },
        PKCS1v15_ES =>  { Module => "Crypt::RSA::ES::PKCS1v15"  },

  # SIGNATURE SCHEMES

             PSS_SS =>  { Module => "Crypt::RSA::SS::PSS"       },
        PKCS1v21_SS =>  { Module => "Crypt::RSA::SS::PSS"       },
        PKCS1v15_SS =>  { Module => "Crypt::RSA::SS::PKCS1v15"  },

  # POST PROCESSORS

           ASCII_PP =>  { Module => "Convert::ASCII::Armour"    },

);


sub new { 

    my ($class, %params) = @_;
    my %self = (%DEFAULTS, %params);
    my $self = bless \%self, $class;

    $self->_storemap (%KNOWNMAP); 
    
    for (qw(ES SS PP)) { 
        $$self{$_} = { Name => $$self{$_} . "_$_" } unless ref $$self{$_};
        $$self{lc($_)} = $self->_load ( %{$$self{$_}} );
    }

    $$self{keychain} = new Crypt::RSA::Key; 

    return bless \%self, $class; 

}


sub keygen { 

    my ($self, %params) = @_;
    $params{KF} = $$self{KF} if $$self{KF};

    my @keys;
    return (@keys = $self->{keychain}->generate (%params))
                  ? @keys 
                  : $self->error ($self->{keychain}->errstr);

} 


sub encrypt { 

    my ($self, %params)   = @_;
    my $plaintext         = $params{Message} || $params{Plaintext};
    my $key               = $params{Key}; 

    return $self->error ($key->errstr, \%params, $key, \$plaintext) 
        unless $key->check();

    my $blocksize = blocksize ( $$self{es}->encryptblock (Key => $key),
                                length($plaintext)
                              );

    return $self->error("Message too long.", \$key, \%params) if $blocksize <= 0;

    my $cyphertext;
    my @segments = steak ($plaintext, $blocksize);
    for (@segments) {
        $cyphertext .= $self->{es}->encrypt (Message => $_, Key => $key)
            || return $self->error ($self->{es}->errstr, \$key, \%params);
    }

    if ($params{Armour} || $params{Armor}) { 
        $cyphertext = $self->{pp}->armour ( 
             Object   => 'RSA ENCRYPTED MESSAGE', 
             Headers  => {  
                Scheme  => $$self{ES}{Module} || ${$KNOWNMAP{$$self{ES}{Name}}}{Module},
                Version => $self->{es}->version() 
             }, 
             Content  => { Cyphertext => $cyphertext },
             Compress => 1, 
        );
    } 

    return $cyphertext;

}


sub decrypt { 

    my ($self , %params) = @_;
    my $cyphertext       = $params{Cyphertext} || $params{Ciphertext};
    my $key              = $params{Key}; 

    return $self->error ($key->errstr, \%params, $key) unless $key->check();

    if ($params{Armour} || $params{Armor}) { 
        my $decoded = $self->{pp}->unarmour ($cyphertext) ||
            return $self->error ($self->{pp}->errstr());
        $cyphertext = $$decoded{Content}{Cyphertext}
    }

    my $plaintext;
    my $blocksize = blocksize ( $$self{es}->decryptblock (Key => $key),
                                length($cyphertext)
                              );

    return $self->error("Message too long.") if $blocksize <= 0;

    my @segments = steak ($cyphertext, $blocksize);
    for (@segments) {
        $plaintext .= $self->{es}->decrypt (Cyphertext=> $_, Key => $key)
            || return $self->error ($self->{es}->errstr, \$key, \%params);
    }

    return $plaintext;

}


sub sign { 

    my ($self, %params) = @_;
    my $signature = $self->{ss}->sign (%params) 
                 || return $self->error ($self->{ss}->errstr,
                        $params{Key}, \%params);

    if ($params{Armour} || $params{Armor}) { 
        $signature      = $self->{pp}->armour ( 
               Object   => "RSA SIGNATURE", 
               Headers  => {  
                    Scheme  => $$self{SS}{Module} || ${$KNOWNMAP{$$self{SS}{Name}}}{Module},
                    Version => $self->{ss}->version() }, 
               Content  => { Signature => $signature },
        );
    }

    return $signature;

} 


sub verify { 

    my ($self, %params) = @_;

    if ($params{Armour} || $params{Armor}) { 
        my $decoded  = $self->{pp}->unarmour ($params{Signature}) ||
            return $self->error ($self->{pp}->errstr());
        $params{Signature} = $$decoded{Content}{Signature}
    }

    my $verify = $self->{ss}->verify (%params) || 
        return $self->error ($self->{ss}->errstr, $params{Key}, \%params);

    return $verify;

}


sub blocksize { 

    my ($blocksize, $ptsize) = @_;
    return $ptsize if $blocksize == -1;
    return 0 if $blocksize < 1;
    return $blocksize;
        
}

1; 


=head1 NAME

Crypt::RSA - RSA public-key cryptosystem.

=head1 SYNOPSIS

    my $rsa = new Crypt::RSA; 

    my ($public, $private) = 
        $rsa->keygen ( 
            Identity  => 'Lord Macbeth <macbeth@glamis.com>',
            Size      => 1024,  
            Password  => 'A day so foul & fair', 
            Verbosity => 1,
        ) or die $rsa->errstr();


    my $cyphertext = 
        $rsa->encrypt ( 
            Message    => $message,
            Key        => $public,
            Armour     => 1,
        ) || die $rsa->errstr();


    my $plaintext = 
        $rsa->decrypt ( 
            Cyphertext => $cyphertext, 
            Key        => $private,
            Armour     => 1,
        ) || die $rsa->errstr();


    my $signature = 
        $rsa->sign ( 
            Message    => $message, 
            Key        => $private
        ) || die $rsa->errstr();


    my $verify = 
        $rsa->verify (
            Message    => $message, 
            Signature  => $signature, 
            Key        => $public
        ) || die $rsa->errstr();

=head1 NOTE

This manual assumes familiarity with public-key cryptography and the RSA
algorithm. If you don't know what these are or how they work, please refer
to the sci.crypt FAQ[15]. A formal treatment of RSA can be found in [1].

=head1 DESCRIPTION

Crypt::RSA is a pure-perl, cleanroom implementation of the RSA public-key
cryptosystem. It uses Math::Pari(3), a perl interface to the blazingly
fast PARI library, for big integer arithmetic and number theoretic
computations.

Crypt::RSA provides arbitrary size key-pair generation, plaintext-aware
encryption (OAEP) and digital signatures with appendix (PSS). For
compatibility with SSLv3, RSAREF2, PGP and other applications that follow
the PKCS #1 v1.5 standard, it also provides PKCS #1 v1.5 encryption and
signatures.

Crypt::RSA is structured as bundle of modules that encapsulate different
parts of the RSA cryptosystem. The RSA algorithm is implemented in
Crypt::RSA::Primitives(3). Encryption schemes, located under
Crypt::RSA::ES, and signature schemes, located under Crypt::RSA::SS, use
the RSA algorithm to build encryption/signature schemes that employ secure
padding. (See the note on Security of Padding Schemes.)

The key generation engine and other functions that work on both components
of the key-pair are encapsulated in Crypt::RSA::Key(3).
Crypt::RSA::Key::Public(3) & Crypt::RSA::Key::Private(3) provide
mechanisms for storage & retrival of keys from disk, decoding & encoding
of keys in certain formats, and secure representation of keys in memory.
Finally, the Crypt::RSA module provides a convenient, DWIM wrapper around
the rest of the modules in the bundle.

=head1 SECURITY OF PADDING SCHEMES

It has been conclusively shown that textbook RSA is insecure[3,7]. Secure
RSA requires that plaintext is padded in a specific manner before
encryption and signing. There are four main standards for padding: PKCS
#1 v1.5 encryption & signatures, and OAEP encryption & PSS signatures.
Crypt::RSA implements these as four modules that 
provide overloaded encrypt(), decrypt(), sign() and verify() methods that
add padding functionality to the basic RSA operations.

Crypt::RSA::ES::PKCS1v15(3) implements PKCS #1 v1.5 encryption,
Crypt::RSA::SS::PKCS1v15(3) implements PKCS #1 v1.5 signatures,
Crypt::RSA::ES::OAEP(3) implements Optimal Asymmetric Encryption and
Crypt::RSA::SS::PSS(3) Probabilistic Signatures.

PKCS #1 v1.5 schemes are older and hence more widely deployed, but PKCS #1
v1.5 encryption has certain flaws that make it vulnerable to
chosen-cyphertext attacks[9]. Even though Crypt::RSA works around these
vulnerabilities, it is recommended that new applications use OAEP and PSS,
both of which are provably secure[13]. In any event,
Crypt::RSA::Primitives (without padding) should never be used directly.

That said, there exists a scheme called Simple RSA[16] that provides
security without padding. However, Crypt::RSA doesn't implement this
scheme yet.

=head1 METHODS

=over 4

=item B<new()>

The constructor. When no arguments are provided, new() returns an object
loaded with default values. This object can be customized by specifying
encryption & signature schemes, key formats and post processors. For
details see the section on B<Customizing the Crypt::RSA
object> later in this manpage.

=item B<keygen()>

keygen() generates and returns an RSA key-pair of specified bitsize.
keygen() is a synonym for Crypt::RSA::Key::generate(). Parameters and
return values are described in the Crypt::RSA::Key(3) manpage.

=item B<encrypt()>

encrypt() performs RSA encryption on a string of arbitrary length with a
public key using the encryption scheme bound to the object. The default
scheme is OAEP. encrypt() returns cyphertext (a string) on success and
undef on failure. It takes a hash as argument with following keys:

=over 4

=item B<Message>

An arbitrary length string to be encrypted.

=item B<Key>

Public key of the recipient, a Crypt::RSA::Key::Public(3) or
compatible object.

=item B<Armour>

A boolean parameter that forces cyphertext through a post processor after
encrpytion. The default post processor is Convert::ASCII::Armour(3) that
encodes binary octets in 6-bit clean ASCII messages. The cyphertext is
returned as-is, when the Armour key is not present.

=back

=item B<decrypt()>

decrypt() performs RSA decryption with a private key using the encryption
scheme bound to the object. The default scheme is OAEP. decrypt() returns
plaintext on success and undef on failure. It takes a hash as argument
with following keys:

=over 4

=item B<Cyphertext>

Cyphertext of arbitrary length. 

=item B<Key>

Private key, a Crypt::RSA::Key::Private(3) or compatible object.

=item B<Armour> 

Boolean parameter that specifies whether the Cyphertext is encoded with a
post processor.

=back

=item B<sign()>

sign() creates an RSA signature on a string with a private key using the
signature scheme bound to the object. The default scheme is
PSS. sign() returns a signature on success and undef on failure. It takes
a hash as argument with following keys:

=over 4

=item B<Message>

A string of arbitrary length to be signed.

=item B<Key>

Private key of the sender, a Crypt::RSA::Key::Private(3) or
compatible object.

=item B<Armour>

A boolean parameter that forces the computed signature to be post
processed.

=back

=item B<verify()>

verify() verifies an RSA signature with a public key using the signature
scheme bound to the object. The default scheme is PSS. verify() returns a
true value on success and undef on failure. It takes a hash as argument
with following keys:

=over 4 

=item B<Message>

A signed message, a string of arbitrary length. 

=item B<Key>

Public key of the signer, a Crypt::RSA::Key::Public(3) or
compatible object.

=item B<Sign> 

A signature computed with sign().

=item B<Armour>

Boolean parameter that specifies whether the Signature has been 
post processed.

=back

=back

=head1 MODULES

Apart from Crypt::RSA, the following modules are intended for application
developer and end-user consumption:

=over 4

=item B<Crypt::RSA::Key>

RSA key pair generator.

=item B<Crypt::RSA::Key::Public>

RSA Public Key Management.

=item B<Crypt::RSA::Key::Private>

RSA Private Key Management.

=item B<Crypt::RSA::ES::OAEP>

Plaintext-aware encryption with RSA.

=item B<Crypt::RSA::SS::PSS>

Probabilistic Signature Scheme based on RSA.

=item B<Crypt::RSA::ES::PKCS1v15>

PKCS #1 v1.5 encryption scheme. 

=item B<Crypt::RSA::SS::PKCS1v15>

PKCS #1 v1.5 signature scheme. 

=back

=head1 CUSTOMISING A CRYPT::RSA OBJECT

A Crypt::RSA object can be customized by passing any of the following keys
in a hash to new(): ES to specify the encryption scheme, SS to specify the
signature scheme, PP to specify the post processor, and KF to specify the
key format. The value associated with these keys can either be a name (a
string) or a hash reference that specifies a module name, its constructor,
and constructor arguments. For example:

    my $rsa = new Crypt::RSA ( ES => 'OAEP' );

                    or 

    my $rsa = new Crypt::RSA ( ES => { Module => 'Crypt::RSA::ES::OAEP' } );

A module thus specified need not be included in the Crypt::RSA bundle, but
it must be interface compatible with the ones provided with Crypt::RSA.

As of this writing, the following names are recognised:

=over 4

=item B<ES> (Encryption Scheme) 

    'OAEP', 'PKCS1v15'

=item B<SS> (Signature Scheme) 

    'PSS', 'PKCS1v15'

=item B<KF> (Key Format) 

    'Native', 'SSH'

=item B<PP> (Post Processor) 

    'ASCII' 

=back 

=head1 ERROR HANDLING

All modules in the Crypt::RSA bundle use a common error handling method
(implemented in Crypt::RSA::Errorhandler(3)). When a method fails it
returns undef and calls $self->error() with the error message. This error
message is available to the caller through the errstr() method. For more
details see the Crypt::RSA::Errorhandler(3) manpage.

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=head1 ACKNOWLEDGEMENTS

Thanks to Ilya Zakharevich for help with Math::Pari, Benjamin Trott for
several patches including SSH key support, Genèche Ramanoudjame for
extensive testing and numerous bug reports, Shizukesa on #perl for
suggesting the error handling method used in this module, and Dave Paris
for good advice.

=head1 LICENSE 

Copyright (c) 2000-2008, Vipul Ved Prakash. This code is free software;
it is distributed under the same license as Perl itself.

I have received requests for commercial licenses of
Crypt::RSA, from those who desire contractual support and
indemnification. I'd be happy to provide a commercial license
if you need one. Please send me mail at C<mail@vipul.net> with
the subject "Crypt::RSA license". Please don't send me mail
asking if you need a commercial license. You don't, if
Artistic of GPL suit you fine.

=head1 SEE ALSO

Crypt::RSA::Primitives(3), Crypt::RSA::DataFormat(3),
Crypt::RSA::Errorhandler(3), Crypt::RSA::Debug(3), Crypt::Primes(3),
Crypt::Random(3), Crypt::CBC(3), Crypt::Blowfish(3),
Tie::EncryptedHash(3), Convert::ASCII::Armour(3), Math::Pari(3),
Class::Loader(3), crypt-rsa-interoperability(3),
crypt-rsa-interoperability-table(3).

=head1 REPORTING BUGS

All bug reports related to Crypt::RSA should go to rt.cpan.org 
at C<http://rt.cpan.org/Dist/Display.html?Queue=Crypt-RSA>

Crypt::RSA is considered to be stable. If you are running into a
problem, it's likely of your own making. Please check your code
and consult the documentation before posting a bug report. A
google search with the error message might also shed light if it
is a common mistake that you've made.

If the module installation fails with a "Segmentation Fault" or
"Bus Error", it is likely a Math::Pari issue. Please consult
Math::Pari bugs on rt.cpan.org or open a bug there. There have
been known issues on HP-UX and SunOS systems (with Math::Pari), 
so if you are on those OSes, please consult Math::Pari 
resources before opening a Crypt::RSA bug.

=head1 BIBLIOGRAPHY

Chronologically sorted (for the most part).

=over 4

=item 1 B<R. Rivest, A. Shamir, L. Aldeman.> A Method for Obtaining Digital Signatures and Public-Key Cryptosystems (1978).

=item 2 B<U. Maurer.> Fast Generation of Prime Numbers and Secure Public-Key Cryptographic Parameters (1994).

=item 3 B<M. Bellare, P. Rogaway.> Optimal Asymmetric Encryption - How to Encrypt with RSA (1995).

=item 4 B<M. Bellare, P. Rogaway.> The Exact Security of Digital Signatures - How to sign with RSA and Rabin (1996).

=item 5 B<B. Schneier.> Applied Cryptography, Second Edition (1996).

=item 6 B<A. Menezes, P. Oorschot, S. Vanstone.> Handbook of Applied Cryptography (1997).

=item 7 B<D. Boneh.> Twenty Years of Attacks on the RSA Cryptosystem (1998).

=item 8 B<D. Bleichenbacher, M. Joye, J. Quisquater.> A New and Optimal Chosen-message Attack on RSA-type Cryptosystems (1998).

=item 9 B<B. Kaliski, J. Staddon.> Recent Results on PKCS #1: RSA Encryption Standard, RSA Labs Bulletin Number 7 (1998).

=item 10 B<B. Kaliski, J. Staddon.> PKCS #1: RSA Cryptography Specifications v2.0, RFC 2437 (1998).

=item 11 B<SSH Communications Security.> SSH 1.2.7 source code (1998).

=item 12 B<S. Simpson.> PGP DH vs. RSA FAQ v1.5 (1999).

=item 13 B<RSA Laboratories.> Draft I, PKCS #1 v2.1: RSA Cryptography Standard (1999).

=item 14 B<E. Young, T. Hudson, OpenSSL Team.> OpenSSL 0.9.5a source code (2000).

=item 15 B<Several Authors.> The sci.crypt FAQ at
http://www.faqs.org/faqs/cryptography-faq/part01/index.html

=item 16 B<Victor Shoup.> A Proposal for an ISO Standard for Public Key Encryption (2001).

=cut
