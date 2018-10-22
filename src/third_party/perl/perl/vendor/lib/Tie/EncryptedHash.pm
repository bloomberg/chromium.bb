#!/usr/bin/perl -s
##
## Tie::EncryptedHash - A tied hash with encrypted fields.
##
## Copyright (c) 2000, Vipul Ved Prakash.  All rights reserved.
## This code is based on Damian Conway's Tie::SecureHash.
##
## $Id: EncryptedHash.pm,v 1.8 2000/09/02 19:23:00 vipul Exp vipul $
## vi:expandtab=1;ts=4;

package Tie::EncryptedHash;
    
use strict;
use vars qw($VERSION $strict);
use Digest::MD5 qw(md5_base64);
use Crypt::CBC;
use Data::Dumper;
use Carp;

( $VERSION )  = '$Revision: 1.8 $' =~ /\s(\d+\.\d+)\s/; 

my $DEBUG = 0;

sub debug {
    return undef unless $DEBUG;
    my ($caller, undef) = caller;
    my (undef,undef,$line,$sub) = caller(1); $sub =~ s/.*://;
    $sub = sprintf "%10s()%4d",$sub,$line;
    print "$sub   " . (shift) . "\n";
}

# sub new {
#   my ($class,%args) = @_;
#   my %self = (); tie %self, $class;
# 	my $self = bless \%self, $class;
#   $self->{__password} = $args{__password} if $args{__password};
#   $self->{__cipher} = $args{__cipher} || qq{Blowfish};
# 	return $self;
# }

sub new {
    my ($class,%args) = @_;
    my $self = {}; tie %$self, $class;
    bless $self, $class;
    $self->{__password} = $args{__password} if $args{__password};
    $self->{__cipher} = $args{__cipher} || qq{Blowfish};
    return $self;
}


sub _access {

    my ($self, $key, $caller, $file, $value, $delete) = @_;
    my $class = ref $self || $self;
                                          # SPECIAL ATTRIBUTE
    if ( $key =~ /(__password|__hide|__scaffolding|__cipher)$/ ) {
        my $key = $1;
        unless($value||$delete) {return undef unless $caller eq $class}
        if ($delete && ($key =~ /__password/)) { 
          for (keys %{$$self{__scaffolding}}) {
            if ( ref $self->{$_} ) { 
              $self->{$_} = encrypt($self->{$_}, $self->{__scaffolding}{$_}, $self->{__cipher});
              delete $self->{__scaffolding}{$_};  
            }
          }
        }
        delete $$self{$key} if $delete;
        return $self->{$key} = $value if $value;
        return $self->{$key};
                                          # SECRET FIELD
    } elsif ( $key =~ m/^(_{1}[^_][^:]*)$/ ||$key =~ m/.*?::(_{1}[^_][^:]*)/ ) {
        my $ctext = $self->{$1}; 
        if ( ref $ctext && !($value)) {   # ENCRYPT REF AT FETCH
          my $pass = $self->{__scaffolding}{$1} || $self->{__password};
          return undef unless $pass;
          $self->{$1} = encrypt($ctext, $pass, $self->{__cipher});
          return $self->FETCH ($1);
        }
        my $ptext = qq{}; my $isnot = !( exists $self->{$1} );  
        my $auth = verify($self,$1);  
        return undef if !($auth) && ref $self->{$1};
        return undef if !($auth) && $self->{__hide};
        if ($auth && $auth ne "1") { $ptext = $auth }
        if ($value && $auth) {            # STORE 
            if ( ref $value ) { 
                $self->{__scaffolding}{$1} = $self->{__password}; $ctext = $value;
            } else { 
              my $key = $1;
              unless ($self->{__password}) {
                if ($value =~ m:^\S+\s\S{22}\s:) { 
                  return $self->{$key} = $value;
                } else { return undef }
              }
              $ctext = encrypt($value, $self->{__password}, $self->{__cipher});
            }
            $self->{$1} = $ctext;
            return $value;
        } elsif ($auth && $delete) {      # DELETE
            delete $$self{$1}   
        } elsif ($isnot && (!($value))) { # DOESN'T EXIST
           return;
        } elsif ((!($auth)) && $ctext) { 
            return $ctext;                # FETCH return ciphertext
        } elsif ($auth && !($isnot)) {    # FETCH return plaintext
            if (ref $ptext) { 
                $self->{$1} = $ptext;
                $self->{__scaffolding}{$1} = $self->{__password};  # Ref counting mechanism
                return $self->{$1};
            }
        } 
        return undef unless $auth;
        return $ptext;
                                          # PUBLIC FIELD
    } elsif ( $key =~ m/([^:]*)$/ || $key =~ m/.*?::([^:]*)/ )  {
        $self->{$1} = $value if $value;
        delete $$self{$1} if $delete;
        return $self->{$1} if $self->{$1};
        return undef;
    }

}

sub encrypt {  # ($plaintext, $password, $cipher)
    $_[0] = qq{REF }. Data::Dumper->new([$_[0]])->Indent(0)->Terse(0)->Purity(1)->Dumpxs if ref $_[0];
    return  qq{$_[2] } . md5_base64($_[0]) .qq{ } . 
            Crypt::CBC->new($_[1],$_[2])->encrypt_hex($_[0]) 
}

sub decrypt { # ($cipher $md5sum $ciphertext, $password)
	return undef unless $_[1];
    my ($m, $d, $c) = split /\s/,$_[0]; 
    my $ptext = Crypt::CBC->new($_[1],$m)->decrypt_hex($c);
    my $check = md5_base64($ptext);
    if ( $d eq $check ) {
      if ($ptext =~ /^REF (.*)/is) { 
        my ($VAR1,$VAR2,$VAR3,$VAR4,$VAR5,$VAR6,$VAR7,$VAR8);
        return eval qq{$1}; 
      }
      return $ptext;  
    }
}

sub verify { # ($self, $key)
    my ($self, $key) = splice @_,0,2;
    # debug ("$self->{__scaffolding}{$key}, $self->{__password}, $self->{$key}");
    return 1 unless $key =~ m:^_:;
    return 1 unless exists $self->{$key};
    return undef if ref $self->{$key} && ($self->{__scaffolding}{$key} ne 
                    $self->{__password});
    my $ptext = decrypt($self->{$key}, $self->{__password}); 
    return $ptext if $ptext;
}
   
sub each	{ CORE::each %{$_[0]} }
sub keys	{ CORE::keys %{$_[0]} }
sub values	{ CORE::values %{$_[0]} }
sub exists	{ CORE::exists $_[0]->{$_[1]} }

sub TIEHASH	# ($class, @args)
{
	my $class = ref($_[0]) || $_[0];
	my $self = bless {}, $class;
    $self->{__password} = $_[1] if $_[1];
    $self->{__cipher} = $_[2] || qq{Blowfish};
    return $self;
}

sub FETCH	# ($self, $key)
{
	my ($self, $key) = @_;
	my $entry = _access($self,$key,(caller)[0..1]);
	return $entry if $entry;
}

sub STORE	# ($self, $key, $value)
{
	my ($self, $key, $value) = @_;
	my $entry = _access($self,$key,(caller)[0..1],$value);
	return $entry if $entry;
}

sub DELETE	# ($self, $key)
{
	my ($self, $key) = @_;
	return _access($self,$key,(caller)[0..1],'',1);
}

sub CLEAR	# ($self)
{
	my ($self) = @_;
	return undef if grep { ! $self->verify($_) } 
                    grep { ! /__/ } CORE::keys %{$self};
	%{$self} = ();
}

sub EXISTS	# ($self, $key)
{
	my ($self, $key) = @_;
	my @context = (caller)[0..1];
	return _access($self,$key,@context) ? 1 : '';
}

sub FIRSTKEY	# ($self)
{
	my ($self) = @_;
	CORE::keys %{$self};
	goto &NEXTKEY;
}

sub NEXTKEY	# ($self)
{
	my $self = $_[0]; my $key;
	my @context = (caller)[0..1];
	while (defined($key = CORE::each %{$self})) {
		last if eval { _access($self,$key,@context) }
	}
	return $key;
}

sub DESTROY	# ($self)
{
}

1;
__END__


=head1 NAME

Tie::EncryptedHash - Hashes (and objects based on hashes) with encrypting fields.

=head1 SYNOPSIS

    use Tie::EncryptedHash;

    my %s = ();
    tie %s, Tie::EncryptedHash, 'passwd';

    $s{foo}  = "plaintext";     # Normal field, stored in plaintext.
    print $s{foo};              # (plaintext)
                                
    $s{_bar} = "signature";     # Fieldnames that begin in single
                                # underscore are encrypted.
    print $s{_bar};             # (signature)  Though, while the password 
                                # is set, they behave like normal fields.
    delete $s{__password};      # Delete password to disable access 
                                # to encrypting fields.
    print $s{_bar};             # (Blowfish NuRVFIr8UCAJu5AWY0w...)

    $s{__password} = 'passwd';  # Restore password to gain access.
    print $s{_bar};             # (signature)
                                
    $s{_baz}{a}{b} = 42;        # Refs are fine, we encrypt them too.


=head1 DESCRIPTION

Tie::EncryptedHash augments Perl hash semantics to build secure, encrypting
containers of data.  Tie::EncryptedHash introduces special hash fields that
are coupled with encrypt/decrypt routines to encrypt assignments at STORE()
and decrypt retrievals at FETCH().  By design, I<encrypting fields> are
associated with keys that begin in single underscore.  The remaining
keyspace is used for accessing normal hash fields, which are retained
without modification.

While the password is set, a Tie::EncryptedHash behaves exactly like a
standard Perl hash.  This is its I<transparent mode> of access.  Encrypting
and normal fields are identical in this mode.  When password is deleted,
encrypting fields are accessible only as ciphertext.  This is
Tie::EncryptedHash's I<opaque mode> of access, optimized for serialization.

Encryption is done with Crypt::CBC(3) which encrypts in the cipher block
chaining mode with Blowfish, DES or IDEA.  Tie::EncryptedHash uses Blowfish
by default, but can be instructed to employ any cipher supported by
Crypt::CBC(3).

=head1 MOTIVATION

Tie::EncryptedHash was designed for storage and communication of key
material used in public key cryptography algorithms.  I abstracted out the
mechanism for encrypting selected fields of a structured data record because
of the sheer convenience of this data security method.

Quite often, applications that require data confidentiality eschew strong
cryptography in favor of OS-based access control mechanisms because of the
additional costs of cryptography integration.  Besides cipher
implementations, which are available as ready-to-deploy perl modules, use of
cryptography in an application requires code to aid conversion and
representation of encrypted data.  This code is usually encapsulated in a
data access layer that manages encryption, decryption, access control and
re-structuring of flat plaintext according to a data model.
Tie::EncryptedHash provides these functions under the disguise of a Perl
hash so perl applications can use strong cryptography without the cost of
implementing a complex data access layer.


=head1 CONSTRUCTION

=head2 Tied Hash  

C<tie %h, Tie::EncryptedHash, 'Password', 'Cipher';>

Ties %h to Tie::EncryptedHash and sets the value of password and cipher to
'Password' and 'Cipher'.  Both arguments are optional.

=head2 Blessed Object

C<$h = new Tie::EncryptedHash __password => 'Password', 
                         __cipher => 'Cipher';>

The new() constructor returns an object that is both tied and blessed into
Tie::EncryptedHash.  Both arguments are optional.  When used in this manner,
Tie::EncryptedHash behaves like a class with encrypting data members.

=head1 RESERVED ATTRIBUTES

The attributes __password, __cipher and __hide are reserved for
communication with object methods.  They are "write-only" from everywhere
except the class to which the hash is tied.  __scaffolding is inaccessible.
Tie::EncryptedHash stores the current encryption password and some transient
data structures in these fields and restricts access to them on need-to-know
basis.

=head2 __password

C<$h{__password} = "new password";
delete $h{__password};>

The password is stored under the attribute C<__password>.  In addition to
specifying a password at construction, assigning to the __password attribute
sets the current encryption password to the assigned value.  Deleting the
__password unsets it and switches the hash into opaque mode.

=head2 __cipher

C<$h{__cipher} = 'DES'; $h{__cipher} = 'Blowfish';>

The cipher used for encryption/decryption is stored under the attribute
__cipher.  The value defaults to 'Blowfish'.

=head2 __hide

C<$h{__hide} = 1;>

Setting this attribute I<hides> encrypting fields in opaque mode.  'undef'
is returned at FETCH() and EXISTS().

=head1 BEHAVIOR

=head2 References

A reference stored in an encrypting field is serialized before encryption.
The data structure represented by the reference is folded into a single line
of ciphertext which is stored under the first level key.  In the opaque
mode, therefore, only the first level of keys of the hash will be visible.

=head2 Opaque Mode

The opaque mode introduces several other constraints on access of encrypting
fields.  Encrypting fields return ciphertext on FETCH() unless __hide
attribute is set, which forces Tie::EncryptedHash to behave as if encrypting
fields don't exist.  Irrespective of __hide, however, DELETE() and CLEAR()
fail in opaque mode.  So does STORE() on an existing encrypting field.
Plaintext assignments to encrypting fields are silently ignored, but
ciphertext assignments are fine.  Ciphertext assignments can be used to move
data between different EncryptedHashes.

=head2 Multiple Passwords and Ciphers

Modality of Tie::EncryptedHash's access system breaks down when more than
one password is used to with different encrypting fields.  This is a
feature.  Tie::EncryptedHash lets you mix passwords and ciphers in the same
hash.  Assign new values to __password and __cipher and create a new
encrypting field.  Transparent mode will be restricted to fields encrypted
with the current password.

=head2 Error Handling

Tie::Encrypted silently ignores access errors.  It doesn't carp/croak when
you perform an illegal operation (like assign plaintext to an encrypting
field in opaque mode).  This is to prevent data lossage, the kind that
results from abnormal termination of applications.

=head1 QUIRKS 

=head2 Autovivification 

Due to the nature of autovivified references (which spring into existence
when an undefined reference is dereferenced), references are stored as
plaintext in transparent mode.  Analogous ciphertext representations are
maintained in parallel and restored to encrypting fields when password is
deleted.  This process is completely transparent to the user, though it's
advisable to delete the password after the final assignment to a
Tie::EncryptedHash.  This ensures plaintext representations and scaffolding
data structures are duly flushed.

=head2 Data::Dumper

Serialization of references is done with Data::Dumper, therefore the nature
of data that can be assigned to encrypting fields is limited by what
Data::Dumper can grok.  We set $Data::Dumper::Purity = 1, so
self-referential and recursive structures should be OK.

=head2 Speed 

Tie::EncryptedHash'es keep their contents encrypted as much as possible, so
there's a rather severe speed penalty.  With Blowfish, STORE() on
EncryptedHash can be upto 70 times slower than a standard perl hash.
Reference STORE()'es will be quicker, but speed gain will be adjusted at
FETCH().  FETCH() is about 35 times slower than a standard perl hash.  DES
affords speed improvements of upto 2x, but is not considered secure for
long-term storage of data.  These values were computed on a DELL PIII-300
Mhz notebook with 128 Mb RAM running perl 5.003 on Linux 2.2.16.  Variations
in speed might be different on your machine.

=head1 STANDARD USAGE

The standard usage for this module would be something along the lines of:
populate Tie::EncryptedHash with sensitive data, delete the password,
serialize the encrypted hash with Data::Dumper, store the result on disk or
send it over the wire to another machine.  Later, when the sensitive data is
required, procure the EncryptedHash, set the password and accesses the
encrypted data fields.

=head1 SEE ALSO

Data::Dumper(3),
Crypt::CBC(3),
Crypt::DES(3),
Crypt::Blowfish(3),
Tie::SecureHash(3)

=head1 ACKNOWLEDGEMENTS

The framework of Tie::EncryptedHash derives heavily from Damian Conway's
Tie::SecureHash.  Objects that are blessed as well as tied are just one of
the pleasant side-effects of stealing Damian's code.  Thanks to Damian for
this brilliant module.

PacificNet (http://www.pacificnet.net) loaned me the aforementioned notebook
to hack from the comfort of my bed.  Thanks folks!

=head1 AUTHOR

Vipul Ved Prakash <mail@vipul.net>

=head1 LICENSE 

This module is distributed under the same license as Perl itself.


