package Crypt::OpenSSL::X509;

use strict;
use vars qw($VERSION @EXPORT_OK);
use Exporter;
use base qw(Exporter);

$VERSION = '1.812';

@EXPORT_OK = qw(
  FORMAT_UNDEF FORMAT_ASN1 FORMAT_TEXT FORMAT_PEM
  FORMAT_PKCS12 FORMAT_SMIME FORMAT_ENGINE FORMAT_IISSGC OPENSSL_VERSION_NUMBER
);

sub Crypt::OpenSSL::X509::has_extension_oid {
  my $x509 = shift;
  my $oid  = shift;

  if (not $Crypt::OpenSSL::X509::exts_by_oid) {
      $Crypt::OpenSSL::X509::exts_by_oid = $x509->extensions_by_oid;
  }

  return $$Crypt::OpenSSL::X509::exts_by_oid{$oid} ? 1 : 0;
}

sub Crypt::OpenSSL::X509::Extension::is_critical {
  my $ext = shift;
  my $crit = $ext->critical();

  return $crit ? 1 : 0;
}

# return a hash for the values of keyUsage or nsCertType
sub Crypt::OpenSSL::X509::Extension::hash_bit_string {
  my $ext  = shift;

  my @bits = split(//, $ext->bit_string);
  my $len  = @bits;

  my %bit_str_hash = ();

  if ($len == 9) { # bits for keyUsage

    %bit_str_hash = (
      'Digital Signature' => $bits[0],
      'Non Repudiation'   => $bits[1],
      'Key Encipherment'  => $bits[2],
      'Data Encipherment' => $bits[3],
      'Key Agreement'     => $bits[4],
      'Certificate Sign'  => $bits[5],
      'CRL Sign'          => $bits[6],
      'Encipher Only'     => $bits[7],
      'Decipher Only'     => $bits[8],);

  } elsif ($len == 8) {    #bits for nsCertType

    %bit_str_hash = (
      'SSL Client'        => $bits[0],
      'SSL Server'        => $bits[1],
      'S/MIME'            => $bits[2],
      'Object Signing'    => $bits[3],
      'Unused'            => $bits[4],
      'SSL CA'            => $bits[5],
      'S/MIME CA'         => $bits[6],
      'Object Signing CA' => $bits[7],);
  }

  return %bit_str_hash;
}

sub Crypt::OpenSSL::X509::Extension::extKeyUsage {
  my $ext = shift;

  my @vals = split(/ /, $ext->extendedKeyUsage);

  return @vals;
}

sub Crypt::OpenSSL::X509::is_selfsigned {
  my $x509 = shift;

  return $x509->subject eq $x509->issuer;
}

BOOT_XS: {
  require DynaLoader;

  # DynaLoader calls dl_load_flags as a static method.
  *dl_load_flags = DynaLoader->can('dl_load_flags');

  do {__PACKAGE__->can('bootstrap') || \&DynaLoader::bootstrap}->(__PACKAGE__, $VERSION);
}

END {
  __PACKAGE__->__X509_cleanup;
}

1;

__END__

=head1 NAME

Crypt::OpenSSL::X509 - Perl extension to OpenSSL's X509 API.

=head1 SYNOPSIS

  use Crypt::OpenSSL::X509;

  my $x509 = Crypt::OpenSSL::X509->new_from_file('cert.pem');

  print $x509->pubkey() . "\n";
  print $x509->subject() . "\n";
  print $x509->hash() . "\n";
  print $x509->email() . "\n";
  print $x509->issuer() . "\n";
  print $x509->issuer_hash() . "\n";
  print $x509->notBefore() . "\n";
  print $x509->notAfter() . "\n";
  print $x509->modulus() . "\n";
  print $x509->exponent() . "\n";
  print $x509->fingerprint_md5() . "\n";
  print $x509->fingerprint_sha256() . "\n";
  print $x509->as_string() . "\n";

  my $x509 = Crypt::OpenSSL::X509->new_from_string(
    $der_encoded_data, Crypt::OpenSSL::X509::FORMAT_ASN1
  );

  # given a time offset of $seconds, will the certificate be valid?
  if ($x509->checkend($seconds)) {
    # cert is expired at $seconds offset
  } else {
    # cert is ok at $seconds offset
  }

  my $exts = $x509->extensions_by_oid();

  foreach my $oid (keys %$exts) {
    my $ext = $$exts{$oid};
    print $oid, " ", $ext->object()->name(), ": ", $ext->value(), "\n";
  }


=head1 ABSTRACT

  Crypt::OpenSSL::X509 - Perl extension to OpenSSL's X509 API.

=head1 DESCRIPTION

  This implement a large majority of OpenSSL's useful X509 API.

  The email() method supports both certificates where the
  subject is of the form:
  "... CN=Firstname lastname/emailAddress=user@domain", and also
  certificates where there is a X509v3 Extension of the form
  "X509v3 Subject Alternative Name: email=user@domain".

=head2 EXPORT

None by default.

On request:

	FORMAT_UNDEF FORMAT_ASN1 FORMAT_TEXT FORMAT_PEM
	FORMAT_PKCS12 FORMAT_SMIME FORMAT_ENGINE FORMAT_IISSGC


=head1 FUNCTIONS

=head2 X509 CONSTRUCTORS

=over 4

=item new ( )

Create a new X509 object.

=item new_from_string ( STRING [ FORMAT ] )

=item new_from_file ( FILENAME [ FORMAT ] )

Create a new X509 object from a string or file. C<FORMAT> should be C<FORMAT_ASN1> or C<FORMAT_PEM>.

=back

=head2 X509 ACCESSORS

=over 4

=item subject

Subject name as a string.

=item issuer

Issuer name as a string.

=item issuer_hash

Issuer name hash as a string.

=item serial

Serial number as a string.

=item hash

Alias for subject_hash

=item subject_hash

Subject name hash as a string.

=item notBefore

C<notBefore> time as a string.

=item notAfter

C<notAfter> time as a string.

=item email

Email address as a string.

=item version

Certificate version as a string.

=item sig_alg_name

Signature algorithm name as a string.

=item key_alg_name

Public key algorithm name as a string.

=item curve

Name of the EC curve used in the public key.

=back

=head2 X509 METHODS

=over 4

=item subject_name ( )

=item issuer_name ( )

Return a Name object for the subject or issuer name. Methods for handling Name objects are given below.

=item is_selfsigned ( )

Return Boolean value if subject and issuer name are the same.

=item as_string ( [ FORMAT ] )

Return the certificate as a string in the specified format. C<FORMAT> can be one of C<FORMAT_PEM> (the default) or C<FORMAT_ASN1>.

=item modulus ( )

Return the modulus for an RSA public key as a string of hex digits. For DSA and EC return the public key. Other algorithms are not supported.

=item bit_length ( )

Return the length of the modulus as a number of bits.

=item fingerprint_md5 ( )

=item fingerprint_sha1 ( )

=item fingerprint_sha224 ( )

=item fingerprint_sha256 ( )

=item fingerprint_sha384 ( )

=item fingerprint_sha512 ( )

Return the specified message digest for the certificate.

=item checkend( OFFSET )

Given an offset in seconds, will the certificate be expired? Returns True if the certificate will be expired. False otherwise.

=item pubkey ( )

Return the RSA, DSA, or EC public key.

=item num_extensions ( )

Return the number of extensions in the certificate.

=item extension ( INDEX )

Return the Extension specified by the integer C<INDEX>.
Methods for handling Extension objects are given below.

=item extensions_by_oid ( )

=item extensions_by_name ( )

=item extensions_by_long_name ( )

Return a hash of Extensions indexed by OID or name.

=item has_extension_oid ( OID )

Return true if the certificate has the extension specified by C<OID>.

=back

=head2 X509::Extension METHODS

=over 4

=item critical ( )

Return a value indicating if the extension is critical or not.
FIXME: the value is an ASN.1 BOOLEAN value.

=item object ( )

Return the ObjectID of the extension.
Methods for handling ObjectID objects are given below.

=item value ( )

Return the value of the extension as an asn1parse(1) style hex dump.

=item as_string ( )

Return a human-readable version of the extension as formatted by X509V3_EXT_print. Note that this will return an empty string for OIDs with unknown ASN.1 encodings.

=back

=head2 X509::ObjectID METHODS

=over 4

=item name ( )

Return the long name of the object as a string.

=item oid ( )

Return the numeric dot-separated form of the object identifier as a string.

=back

=head2 X509::Name METHODS

=over 4

=item as_string ( )

Return a string representation of the Name

=item entries ( )

Return an array of Name_Entry objects. Methods for handling Name_Entry objects are given below.

=item has_entry ( TYPE [ LASTPOS ] )

=item has_long_entry ( TYPE [ LASTPOS ] )

=item has_oid_entry ( TYPE [ LASTPOS ] )

Return true if a name has an entry of the specified C<TYPE>. Depending on the function the C<TYPE> may be in the short form (e.g. C<CN>), long form (C<commonName>) or OID (C<2.5.4.3>). If C<LASTPOS> is specified then the search is made from that index rather than from the start.

=item get_index_by_type ( TYPE [ LASTPOS ] )

=item get_index_by_long_type ( TYPE [ LASTPOS ] )

=item get_index_by_oid_type ( TYPE [ LASTPOS ] )

Return the index of an entry of the specified C<TYPE> in a name. Depending on the function the C<TYPE> may be in the short form (e.g. C<CN>), long form (C<commonName>) or OID (C<2.5.4.3>). If C<LASTPOS> is specified then the search is made from that index rather than from the start.

=item get_entry_by_type ( TYPE [ LASTPOS ] )

=item get_entry_by_long_type ( TYPE [ LASTPOS ] )

These methods work similarly to get_index_by_* but return the Name_Entry rather than the index.

=back

=head2 X509::Name_Entry METHODS

=over 4

=item as_string ( [ LONG ] )

Return a string representation of the Name_Entry of the form C<typeName=Value>. If C<LONG> is 1, the long form of the type is used.

=item type ( [ LONG ] )

Return a string representation of the type of the Name_Entry. If C<LONG> is 1, the long form of the type is used.

=item value ( )

Return a string representation of the value of the Name_Entry.

=item is_printableString ( )

=item is_ia5string ( )

=item is_utf8string ( )

=item is_asn1_type ( [ASN1_TYPE] )

Return true if the Name_Entry value is of the specified type. The value of C<ASN1_TYPE> should be as listed in OpenSSL's C<asn1.h>.

=back

=head1 SEE ALSO

OpenSSL(1), Crypt::OpenSSL::RSA, Crypt::OpenSSL::Bignum

=head1 AUTHOR

Dan Sully

=head1 CONTRIBUTORS

=over

=item * kmx, release 1.8.9

=item * Sebastian Andrzej Siewior

=item * David O'Callaghan, E<lt>david.ocallaghan@cs.tcd.ieE<gt>

=item * Daniel Kahn Gillmor E<lt>dkg@fifthhorseman.netE<gt>

=back

=head1 COPYRIGHT AND LICENSE

Copyright 2004-2018 by Dan Sully

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
