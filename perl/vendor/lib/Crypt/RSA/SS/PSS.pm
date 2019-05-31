package Crypt::RSA::SS::PSS;
use strict;
use warnings;

## Crypt::RSA::SS:PSS
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.

use base 'Crypt::RSA::Errorhandler';
use Math::Prime::Util qw/random_bytes/;
use Crypt::RSA::DataFormat qw(octet_len os2ip i2osp octet_xor mgf1);
use Crypt::RSA::Primitives;
use Crypt::RSA::Debug qw(debug);
use Digest::SHA qw(sha1);

$Crypt::RSA::SS::PSS::VERSION = '1.99';

sub new { 
    my ($class, %params) = @_;
    my $self = bless { primitives => new Crypt::RSA::Primitives, 
                       hlen       => 20,
                       VERSION    => $Crypt::RSA::SS::PSS::VERSION,
                     }, $class;
    if ($params{Version}) { 
        # do versioning here
    }
    return $self;
}


sub sign { 
    my ($self, %params) = @_; 
    my $key = $params{Key}; my $M = $params{Message} || $params{Plaintext};
    return $self->error("No Key parameter", \$M, \%params) unless $key;
    return $self->error("No Message or Plaintext parameter", \$key, \%params) unless $M;
    my $k = octet_len ($key->n);
    my $salt = random_bytes($self->{hlen});
    my $em = $self->encode ($M, $salt, $k-1);
    my $m = os2ip ($em);
    my $sig = $self->{primitives}->core_sign (Key => $key, Message => $m);
    my $S = i2osp ($sig, $k);
    return ($S, $salt) if wantarray;
    return $S;
}    


sub verify_with_salt { 
    my ($self, %params) = @_;
    my $key = $params{Key}; my $M = $params{Message} || $params{Plaintext};
    my $S = $params{Signature}; my $salt = $params{Salt};
    return $self->error("No Key parameter", \$S, \%params) unless $key;
    return $self->error("No Signature parameter", \$key, \%params) unless $S;
    my $k = octet_len ($key->n);
    my $em; 
    unless ($em = $self->encode ($M, $salt, $k-1)) { 
        return if $self->errstr eq "Message too long.";
        return $self->error ("Modulus too short.", \$M, \$S, \$key, \%params) if 
        $self->errstr eq "Intended encoded message length too short."; 
    }
    return $self->error ("Invalid signature.", \$M, \$S, $key, \%params) if length($S) < $k;
    my $s = os2ip ($S);
    my $m = $self->{primitives}->core_verify (Key => $key, Signature => $s) || 
        $self->error ("Invalid signature.", \$M, \$S, $key, \%params);
    my $em1 = i2osp ($m, $k-1) || 
        return $self->error ("Invalid signature.", \$M, \$S, $key, \%params);
    return 1 if $em eq $em1;
    return $self->error ("Invalid signature.", \$M, \$S, $key, \%params);
}


sub verify { 
    my ($self, %params) = @_;
    my $key = $params{Key}; my $M = $params{Message} || $params{Plaintext}; 
    my $S = $params{Signature}; 
    return $self->error("No Key parameter", \$S, \$M, \%params) unless $key;
    return $self->error("No Signature parameter", \$key, \$M, \%params) unless $S;
    return $self->error("No Message or Plaintext parameter", \$key, \$S, \%params) unless $M;
    my $k = octet_len ($key->n);
    my $s = os2ip ($S);
    my $m = $self->{primitives}->core_verify (Key => $key, Signature => $s) || 
        $self->error ("Invalid signature.", \$M, \$S, $key, \%params);
    my $em1 = i2osp ($m, $k-1) || 
        return $self->error ("Invalid signature.", \$M, \$S, $key, \%params);
    return 1 if $self->verify_with_salt_recovery ($M, $em1);
    return $self->error ("Invalid signature.", \$M, \$S, $key, \%params);
}


sub encode { 
    my ($self, $M, $salt, $emlen) = @_;  
    return $self->error ("Intended encoded message length too short.", \$M, \$salt ) 
        if $emlen < 2 * $self->{hlen}; 
    my $H = $self->hash ("$salt$M");
    my $padlength = $emlen - (2*$$self{hlen}); 
    my $PS = chr(0)x$padlength;
    my $db = $salt . $PS; 
    my $dbmask = $self->mgf ($H, $emlen - $self->{hlen});
    my $maskeddb = octet_xor ($db, $dbmask);
    my $em = $H . $maskeddb; 
    return $em;
}


sub verify_with_salt_recovery { 
    my ($self, $M, $EM) = @_; 
    my $hlen = $$self{hlen}; 
    my $emlen = length ($EM); 
    return $self->error ("Encoded message too short.", \$M, \$EM) if $emlen < (2*$hlen); 
    my $H = substr $EM, 0, $hlen; 
    my $maskeddb = substr $EM, $hlen; 
    my $dbmask = $self->mgf ($H, $emlen-$hlen);
    my $db = octet_xor ($maskeddb, $dbmask); 
    my $salt = substr $db, 0, $hlen;
    my $PS = substr $db, $hlen; 
    my $check = chr(0) x ($emlen-(2*$hlen)); debug ("PS: $PS");
    return $self->error ("Inconsistent.") unless $check eq $PS; 
    my $H1 = $self->hash ("$salt$M");
    return 1 if $H eq $H1; 
    return $self->error ("Inconsistent.");
} 


sub hash { 
    my ($self, $data) = @_;
    return sha1 ($data);
}
    

sub mgf { 
    my ($self, @data) = @_;
    return mgf1 (@data);
}


sub version { 
    my $self = shift;
    return $self->{VERSION};
}


sub signblock { 
    return -1;
}


sub verifyblock { 
    my ($self, %params) = @_;
    return octet_len($params{Key}->n);
}


1;

=head1 NAME

Crypt::RSA::SS::PSS - Probabilistic Signature Scheme based on RSA. 

=head1 SYNOPSIS

    my $pss = new Crypt::RSA::SS::PSS; 

    my $signature = $pss->sign (
                        Message => $message,
                        Key     => $private, 
                    ) || die $pss->errstr;

    my $verify    = $pss->verify (
                        Message   => $message, 
                        Key       => $key, 
                        Signature => $signature, 
                    ) || die $pss->errstr;


=head1 DESCRIPTION

PSS (Probabilistic Signature Scheme) is a provably secure method of
creating digital signatures with RSA. "Provable" means that the
difficulty of forging signatures can be directly related to inverting
the RSA function. "Probabilistic" alludes to the randomly generated salt
value included in the signature to enhance security. For more details
on PSS, see [4] & [13].

=head1 METHODS

=head2 B<new()>

Constructor. 

=head2 B<version()>

Returns the version number of the module. 

=head2 B<sign()>

Computes a PSS signature on a message with the private key of the signer.
In scalar context, sign() returns the computed signature. In array
context, it returns the signature and the random salt. The signature can
verified with verify() or verify_with_salt(). sign() takes a hash argument
with the following mandatory keys:

=over 4

=item B<Message> 

Message to be signed, a string of arbitrary length.  

=item B<Key> 

Private key of the signer, a Crypt::RSA::Key::Private object.

=back

=head2 B<verify()>

Verifies a signature generated with sign(). The salt is recovered from the
signature and need not be passed. Returns a true value on success and
false on failure. $self->errstr is set to "Invalid signature." or
appropriate error on failure. verify() takes a hash argument with the
following mandatory keys:

=over 4

=item B<Key>

Public key of the signer, a Crypt::RSA::Key::Public object. 

=item B<Message>

The original signed message, a string of arbitrary length. 

=item B<Signature>

Signature computed with sign(), a string. 

=item B<Version>

Version of the module that was used for creating the Signature. This is an
optional argument. When present, verify() will ensure before proceeding
that the installed version of the module can successfully verify the
Signature.

=back

=head2 B<verify_with_salt()>

Verifies a signature given the salt. Takes the same arguments as verify()
in addition to B<Salt>, which is a 20-byte string returned by sign() in
array context.

=head1 ERROR HANDLING

See ERROR HANDLING in Crypt::RSA(3) manpage.

=head1 BIBLIOGRAPHY

See BIBLIOGRAPHY in Crypt::RSA(3) manpage.

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=head1 SEE ALSO 

Crypt::RSA(3), Crypt::RSA::Primitives(3), Crypt::RSA::Keys(3),
Crypt::RSA::EME::OAEP(3)

=cut


