package Cpanel::JSON::XS::Type;

=pod

=head1 NAME

Cpanel::JSON::XS::Type - Type support for JSON encode

=head1 SYNOPSIS

 use Cpanel::JSON::XS;
 use Cpanel::JSON::XS::Type;


 encode_json([10, "10", 10.25], [JSON_TYPE_INT, JSON_TYPE_INT, JSON_TYPE_STRING]);
 # '[10,10,"10.25"]'

 encode_json([10, "10", 10.25], json_type_arrayof(JSON_TYPE_INT));
 # '[10,10,10]'

 encode_json(1, JSON_TYPE_BOOL);
 # 'true'

 my $perl_struct = { key1 => 1, key2 => "2", key3 => 1 };
 my $type_spec = { key1 => JSON_TYPE_STRING, key2 => JSON_TYPE_INT, key3 => JSON_TYPE_BOOL };
 my $json_string = encode_json($perl_struct, $type_spec);
 # '{"key1":"1","key2":2,"key3":true}'

 my $perl_struct = { key1 => "value1", key2 => "value2", key3 => 0, key4 => 1, key5 => "string", key6 => "string2" };
 my $type_spec = json_type_hashof(JSON_TYPE_STRING);
 my $json_string = encode_json($perl_struct, $type_spec);
 # '{"key1":"value1","key2":"value2","key3":"0","key4":"1","key5":"string","key6":"string2"}'

 my $perl_struct = { key1 => { key2 => [ 10, "10", 10.6 ] }, key3 => "10.5" };
 my $type_spec = { key1 => json_type_anyof(JSON_TYPE_FLOAT, json_type_hashof(json_type_arrayof(JSON_TYPE_INT))), key3 => JSON_TYPE_FLOAT };
 my $json_string = encode_json($perl_struct, $type_spec);
 # '{"key1":{"key2":[10,10,10]},"key3":10.5}'


 my $value = decode_json('false', 1, my $type);
 # $value is 0 and $type is JSON_TYPE_BOOL

 my $value = decode_json('0', 1, my $type);
 # $value is 0 and $type is JSON_TYPE_INT

 my $value = decode_json('"0"', 1, my $type);
 # $value is 0 and $type is JSON_TYPE_STRING

 my $json_string = '{"key1":{"key2":[10,"10",10.6]},"key3":"10.5"}';
 my $perl_struct = decode_json($json_string, 0, my $type_spec);
 # $perl_struct is { key1 => { key2 => [ 10, 10, 10.6 ] }, key3 => 10.5 }
 # $type_spec is { key1 => { key2 => [ JSON_TYPE_INT, JSON_TYPE_STRING, JSON_TYPE_FLOAT ] }, key3 => JSON_TYPE_STRING }

=head1 DESCRIPTION

This module provides stable JSON type support for the
L<Cpanel::JSON::XS|Cpanel::JSON::XS> encoder which doesn't depend on
any internal perl scalar flags or characteristics. Also it provides
real JSON types for L<Cpanel::JSON::XS|Cpanel::JSON::XS> decoder.

In most cases perl structures passed to
L<encode_json|Cpanel::JSON::XS/encode_json> come from other functions
or from other modules and caller of Cpanel::JSON::XS module does not
have control of internals or they are subject of change. So it is not
easy to support enforcing types as described in the
L<simple scalars|Cpanel::JSON::XS/simple scalars> section.

For services based on JSON contents it is sometimes needed to correctly
process and enforce JSON types.

The function L<decode_json|Cpanel::JSON::XS/decode_json> takes optional
third scalar parameter and fills it with specification of json types.

The function L<encode_json|Cpanel::JSON::XS/encode_json> takes a perl
structure as its input and optionally also a json type specification in
the second parameter.

If the specification is not provided (or is undef) internal perl
scalar flags are used for the resulting JSON type. The internal flags
can be changed by perl itself, but also by external modules. Which
means that types in resulting JSON string aren't stable. Specially it
does not work reliable for dual vars and scalars which were used in
both numeric and string operations. See L<simple
scalars|Cpanel::JSON::XS/simple scalars>.

=head2 JSON type specification for scalars:

=over 4

=item JSON_TYPE_BOOL

It enforces JSON boolean in resulting JSON, i.e. either C<true> or
C<false>. For determining whether the scalar passed to the encoder
is true, standard perl boolean logic is used.

=item JSON_TYPE_INT

It enforces JSON number without fraction part in the resulting JSON.
Equivalent of perl function L<int|perlfunc/int> is used for conversion.

=item JSON_TYPE_FLOAT

It enforces JSON number with fraction part in the resulting JSON.
Equivalent of perl operation C<+0> is used for conversion.

=item JSON_TYPE_STRING

It enforces JSON string type in the resulting JSON.

=item JSON_TYPE_NULL

It represents JSON C<null> value. Makes sense only when passing
perl's C<undef> value.

=back

For each type, there also exists a type with the suffix C<_OR_NULL>
which encodes perl's C<undef> into JSON C<null>. Without type with
suffix C<_OR_NULL> perl's C<undef> is converted to specific type
according to above rules.

=head2 JSON type specification for arrays:

=over 4

=item [...]

The array must contain the same number of elements as in the perl
array passed for encoding. Each element of the array describes the
JSON type which is enforced for the corresponding element of the
perl array.

=item json_type_arrayof

This function takes a JSON type specification as its argument which
is enforced for every element of the passed perl array.

=back

=head2 JSON type specification for hashes:

=over 4

=item {...}

Each hash value for corresponding key describes the JSON type
specification for values of passed perl hash structure. Keys in hash
which are not present in passed perl hash structure are simple
ignored and not used.

=item json_type_hashof

This function takes a JSON type specification as its argument which
is enforced for every value of passed perl hash structure.

=back

=head2 JSON type specification for alternatives:

=over 4

=item json_type_anyof

This function takes a list of JSON type alternative specifications
(maximally one scalar, one array, and one hash) as its input and the
JSON encoder chooses one that matches.

=item json_type_null_or_anyof

Like L<C<json_type_anyof>|/json_type_anyof>, but scalar can be only
perl's C<undef>.

=item json_type_weaken

This function can be used as an argument for L</json_type_arrayof>,
L</json_type_hashof> or L</json_type_anyof> functions to create weak
references suitable for complicated recursive structures. It depends
on L<the weaken function from Scalar::Util|Scalar::Util/weaken> module.
See following example:

  my $struct = {
      type => JSON_TYPE_STRING,
      array => json_type_arrayof(JSON_TYPE_INT),
  };
  $struct->{recursive} = json_type_anyof(
      json_type_weaken($struct),
      json_type_arrayof(JSON_TYPE_STRING),
  );

=back

=head1 AUTHOR

Pali E<lt>pali@cpan.orgE<gt>

=head1 COPYRIGHT & LICENSE

Copyright (c) 2017, GoodData Corporation. All rights reserved.

This module is available under the same licences as perl, the Artistic
license and the GPL.

=cut

use strict;
use warnings;

BEGIN {
  if (eval { require Scalar::Util }) {
    Scalar::Util->import('weaken');
  } else {
    *weaken = sub($) { die 'Scalar::Util is required for weaken' };
  }
}

# This exports needed XS constants to perl
use Cpanel::JSON::XS ();

use Exporter;
our @ISA = qw(Exporter);
our @EXPORT = our @EXPORT_OK = qw(
  json_type_arrayof
  json_type_hashof
  json_type_anyof
  json_type_null_or_anyof
  json_type_weaken
  JSON_TYPE_NULL
  JSON_TYPE_BOOL
  JSON_TYPE_INT
  JSON_TYPE_FLOAT
  JSON_TYPE_STRING
  JSON_TYPE_BOOL_OR_NULL
  JSON_TYPE_INT_OR_NULL
  JSON_TYPE_FLOAT_OR_NULL
  JSON_TYPE_STRING_OR_NULL
  JSON_TYPE_ARRAYOF_CLASS
  JSON_TYPE_HASHOF_CLASS
  JSON_TYPE_ANYOF_CLASS
);

use constant JSON_TYPE_WEAKEN_CLASS => 'Cpanel::JSON::XS::Type::Weaken';

sub json_type_anyof {
  my ($scalar, $array, $hash);
  my ($scalar_weaken, $array_weaken, $hash_weaken);
  foreach (@_) {
    my $type = $_;
    my $ref = ref($_);
    my $weaken;
    if ($ref eq JSON_TYPE_WEAKEN_CLASS) {
      $type = ${$type};
      $ref = ref($type);
      $weaken = 1;
    }
    if ($ref eq '') {
      die 'Only one scalar type can be specified in anyof' if defined $scalar;
      $scalar = $type;
      $scalar_weaken = $weaken;
    } elsif ($ref eq 'ARRAY' or $ref eq JSON_TYPE_ARRAYOF_CLASS) {
      die 'Only one array type can be specified in anyof' if defined $array;
      $array = $type;
      $array_weaken = $weaken;
    } elsif ($ref eq 'HASH' or $ref eq JSON_TYPE_HASHOF_CLASS) {
      die 'Only one hash type can be specified in anyof' if defined $hash;
      $hash = $type;
      $hash_weaken = $weaken;
    } else {
      die 'Only scalar, array or hash can be specified in anyof';
    }
  }
  my $type = [$scalar, $array, $hash];
  weaken $type->[0] if $scalar_weaken;
  weaken $type->[1] if $array_weaken;
  weaken $type->[2] if $hash_weaken;
  return bless $type, JSON_TYPE_ANYOF_CLASS;
}

sub json_type_null_or_anyof {
  foreach (@_) {
    die 'Scalar cannot be specified in null_or_anyof' if ref($_) eq '';
  }
  return json_type_anyof(JSON_TYPE_CAN_BE_NULL, @_);
}

sub json_type_arrayof {
  die 'Exactly one type must be specified in arrayof' if scalar @_ != 1;
  my $type = $_[0];
  if (ref($type) eq JSON_TYPE_WEAKEN_CLASS) {
    $type = ${$type};
    weaken $type;
  }
  return bless \$type, JSON_TYPE_ARRAYOF_CLASS;
}

sub json_type_hashof {
  die 'Exactly one type must be specified in hashof' if scalar @_ != 1;
  my $type = $_[0];
  if (ref($type) eq JSON_TYPE_WEAKEN_CLASS) {
    $type = ${$type};
    weaken $type;
  }
  return bless \$type, JSON_TYPE_HASHOF_CLASS;
}

sub json_type_weaken {
  die 'Exactly one type must be specified in weaken' if scalar @_ != 1;
  die 'Scalar cannot be specfied in weaken' if ref($_[0]) eq '';
  return bless \(my $type = $_[0]), JSON_TYPE_WEAKEN_CLASS;
}

1;
