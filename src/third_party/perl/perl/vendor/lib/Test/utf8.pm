package Test::utf8;

use 5.007003;

use strict;
use warnings;

use base qw(Exporter);

use Encode;
use charnames ':full';

our $VERSION = "1.01";

our @EXPORT = qw(
  is_valid_string is_dodgy_utf8 is_sane_utf8
  is_within_ascii is_within_latin1 is_within_latin_1
  is_flagged_utf8 isnt_flagged_utf8
);

# A Regexp string to match valid UTF8 bytes
# this info comes from page 78 of "The Unicode Standard 4.0"
# published by the Unicode Consortium
our $valid_utf8_regexp = <<'REGEX' ;
        [\x{00}-\x{7f}]
      | [\x{c2}-\x{df}][\x{80}-\x{bf}]
      |         \x{e0} [\x{a0}-\x{bf}][\x{80}-\x{bf}]
      | [\x{e1}-\x{ec}][\x{80}-\x{bf}][\x{80}-\x{bf}]
      |         \x{ed} [\x{80}-\x{9f}][\x{80}-\x{bf}]
      | [\x{ee}-\x{ef}][\x{80}-\x{bf}][\x{80}-\x{bf}]
      |         \x{f0} [\x{90}-\x{bf}][\x{80}-\x{bf}]
      | [\x{f1}-\x{f3}][\x{80}-\x{bf}][\x{80}-\x{bf}][\x{80}-\x{bf}]
      |         \x{f4} [\x{80}-\x{8f}][\x{80}-\x{bf}][\x{80}-\x{bf}]
REGEX

=head1 NAME

Test::utf8 - handy utf8 tests

=head1 SYNOPSIS

  # check the string is good
  is_valid_string($string);   # check the string is valid
  is_sane_utf8($string);      # check not double encoded

  # check the string has certain attributes
  is_flagged_utf8($string1);   # has utf8 flag set
  is_within_ascii($string2);   # only has ascii chars in it
  isnt_within_ascii($string3); # has chars outside the ascii range
  is_within_latin_1($string4); # only has latin-1 chars in it
  isnt_within_ascii($string5); # has chars outside the latin-1 range

=head1 DESCRIPTION

This module is a collection of tests useful for dealing with utf8 strings in
Perl.

This module has two types of tests: The validity tests check if a string is
valid and not corrupt, whereas the characteristics tests will check that string
has a given set of characteristics.

=head2 Validity Tests

=over

=item is_valid_string($string, $testname)

Checks if the string is "valid", i.e. this passes and returns true unless
the internal utf8 flag hasn't been set on scalar that isn't made up of a valid
utf-8 byte sequence.

This should I<never> happen and, in theory, this test should always pass. Unless
you (or a module you use) goes monkeying around inside a scalar using Encode's
private functions or XS code you shouldn't ever end up in a situation where
you've got a corrupt scalar.  But if you do, and you do, then this function
should help you detect the problem.

To be clear, here's an example of the error case this can detect:

  my $mark = "Mark";
  my $leon = "L\x{e9}on";
  is_valid_string($mark);  # passes, not utf-8
  is_valid_string($leon);  # passes, not utf-8

  my $iloveny = "I \x{2665} NY";
  is_valid_string($iloveny);      # passes, proper utf-8

  my $acme = "L\x{c3}\x{a9}on";
  Encode::_utf8_on($acme);      # (please don't do things like this)
  is_valid_string($acme);       # passes, proper utf-8 byte sequence upgraded

  Encode::_utf8_on($leon);      # (this is why you don't do things like this)
  is_valid_string($leon);       # fails! the byte \x{e9} isn't valid utf-8

=cut

sub is_valid_string($;$)
{
  my $string = shift;
  my $name = shift || "valid string test";

  # check we're a utf8 string - if not, we pass.
  unless (Encode::is_utf8($string))
    { return _pass($name) }

  # work out at what byte (if any) we have an invalid byte sequence
  # and return the correct test result
  my $pos = _invalid_sequence_at_byte($string);
  if (_ok(!defined($pos), $name)) { return 1 }
  _diag("malformed byte sequence starting at byte $pos");
  return;
}

sub _invalid_sequence_at_byte($)
{
  my $string = shift;

  # examine the bytes that make up the string (not the chars)
  # by turning off the utf8 flag (no, use bytes doesn't
  # work, we're dealing with a regexp)
  Encode::_utf8_off($string);  ## no critic (ProtectPrivateSubs)

  # work out the index of the first non matching byte
  my $result = $string =~ m/^($valid_utf8_regexp)*/ogx;

  # if we matched all the string return the empty list
  my $pos = pos $string || 0;
  return if $pos == length($string);

  # otherwise return the position we found
  return $pos
}

=item is_sane_utf8($string, $name)

This test fails if the string contains something that looks like it
might be dodgy utf8, i.e. containing something that looks like the
multi-byte sequence for a latin-1 character but perl hasn't been
instructed to treat as such.  Strings that are not utf8 always
automatically pass.

Some examples may help:

  # This will pass as it's a normal latin-1 string
  is_sane_utf8("Hello L\x{e9}eon");

  # this will fail because the \x{c3}\x{a9} looks like the
  # utf8 byte sequence for e-acute
  my $string = "Hello L\x{c3}\x{a9}on";
  is_sane_utf8($string);

  # this will pass because the utf8 is correctly interpreted as utf8
  Encode::_utf8_on($string)
  is_sane_utf8($string);

Obviously this isn't a hundred percent reliable.  The edge case where
this will fail is where you have C<\x{c2}> (which is "LATIN CAPITAL
LETTER WITH CIRCUMFLEX") or C<\x{c3}> (which is "LATIN CAPITAL LETTER
WITH TILDE") followed by one of the latin-1 punctuation symbols.

  # a capital letter A with tilde surrounded by smart quotes
  # this will fail because it'll see the "\x{c2}\x{94}" and think
  # it's actually the utf8 sequence for the end smart quote
  is_sane_utf8("\x{93}\x{c2}\x{94}");

However, since this hardly comes up this test is reasonably reliable
in most cases.  Still, care should be applied in cases where dynamic
data is placed next to latin-1 punctuation to avoid false negatives.

There exists two situations to cause this test to fail; The string
contains utf8 byte sequences and the string hasn't been flagged as
utf8 (this normally means that you got it from an external source like
a C library; When Perl needs to store a string internally as utf8 it
does it's own encoding and flagging transparently) or a utf8 flagged
string contains byte sequences that when translated to characters
themselves look like a utf8 byte sequence.  The test diagnostics tells
you which is the case.

=cut

# build my regular expression out of the latin-1 bytes
# NOTE: This won't work if our locale is nonstandard will it?
my $re_bit = join "|", map { Encode::encode("utf8",chr($_)) } (127..255);

sub is_sane_utf8($;$)
{
  my $string = shift;
  my $name   = shift || "sane utf8";

  # regexp in scalar context with 'g', meaning this loop will run for
  # each match.  Should only have to run it once, but will redo if
  # the failing case turns out to be allowed in %allowed.
  while ($string =~ /($re_bit)/o)
  {
    # work out what the double encoded string was
    my $bytes = $1;

    my $index = $+[0] - length($bytes);
    my $codes = join '', map { sprintf '<%00x>', ord($_) } split //, $bytes;

    # what character does that represent?
    my $char = Encode::decode("utf8",$bytes);
    my $ord  = ord($char);
    my $hex  = sprintf '%00x', $ord;
    $char = charnames::viacode($ord);

    # print out diagnostic messages
    _fail($name);
    _diag(qq{Found dodgy chars "$codes" at char $index\n});
    if (Encode::is_utf8($string))
      { _diag("Chars in utf8 string look like utf8 byte sequence.") }
    else
      { _diag("String not flagged as utf8...was it meant to be?\n") }
    _diag("Probably originally a $char char - codepoint $ord (dec),"
         ." $hex (hex)\n");

    return 0;
  }

  # got this far, must have passed.
  _ok(1,$name);
  return 1;
}

# historic name of method; deprecated
sub is_dodgy_utf8 { goto &is_sane_utf8 }

=back

=head2 String Characteristic Tests

These routines allow you to check the range of characters in a string.
Note that these routines are blind to the actual encoding perl
internally uses to store the characters, they just check if the
string contains only characters that can be represented in the named
encoding:

=over

=item is_within_ascii

Tests that a string only contains characters that are in the ASCII
character set.

=cut

sub is_within_ascii($;$)
{
  my $string = shift;
  my $name   = shift || "within ascii";

  # look for anything that isn't ascii or pass
  $string =~ /([^\x{00}-\x{7f}])/ or return _pass($name);

  # explain why we failed
  my $dec = ord($1);
  my $hex = sprintf '%02x', $dec;

  _fail($name);
  _diag("Char $+[0] not ASCII (it's $dec dec / $hex hex)");

  return 0;
}

=item is_within_latin_1

Tests that a string only contains characters that are in latin-1.

=cut

sub is_within_latin_1($;$)
{
  my $string = shift;
  my $name   = shift || "within latin-1";

  # look for anything that isn't ascii or pass
  $string =~ /([^\x{00}-\x{ff}])/ or return _pass($name);

  # explain why we failed
  my $dec = ord($1);
  my $hex = sprintf '%x', $dec;

  _fail($name);
  _diag("Char $+[0] not Latin-1 (it's $dec dec / $hex hex)");

  return 0;
}

sub is_within_latin1 { goto &is_within_latin_1 }

=back

Simply check if a scalar is or isn't flagged as utf8 by perl's
internals:

=over

=item is_flagged_utf8($string, $name)

Passes if the string is flagged by perl's internals as utf8, fails if
it's not.

=cut

sub is_flagged_utf8
{
  my $string = shift;
  my $name = shift || "flagged as utf8";
  return _ok(Encode::is_utf8($string),$name);
}

=item isnt_flagged_utf8($string,$name)

The opposite of C<is_flagged_utf8>, passes if and only if the string
isn't flagged as utf8 by perl's internals.

Note: you can refer to this function as C<isn't_flagged_utf8> if you
really want to.

=cut

sub isnt_flagged_utf8($;$)
{
  my $string = shift;
  my $name   = shift || "not flagged as utf8";
  return _ok(!Encode::is_utf8($string), $name);
}

sub isn::t_flagged_utf8($;$)
{
  my $string = shift;
  my $name   = shift || "not flagged as utf8";
  return _ok(!Encode::is_utf8($string), $name);
}

=back

=head1 AUTHOR

Written by Mark Fowler B<mark@twoshortplanks.com>

=head1 COPYRIGHT

Copyright Mark Fowler 2004,2012.  All rights reserved.

This program is free software; you can redistribute it
and/or modify it under the same terms as Perl itself.

=head1 BUGS

None known.  Please report any to me via the CPAN RT system.  See
http://rt.cpan.org/ for more details.

=head1 SEE ALSO

L<Test::DoubleEncodedEntities> for testing for double encoded HTML
entities.

=cut

##########

# shortcuts for Test::Builder.

use Test::Builder;
my $tester = Test::Builder->new();

sub _ok
{
  local $Test::Builder::Level = $Test::Builder::Level + 1;
  return $tester->ok(@_)
}
sub _diag
{
  local $Test::Builder::Level = $Test::Builder::Level + 1;
  $tester->diag(@_);
  return;
}

sub _fail
{
  local $Test::Builder::Level = $Test::Builder::Level + 1;
  return _ok(0,@_)
}

sub _pass
{
  local $Test::Builder::Level = $Test::Builder::Level + 1;
  return _ok(1,@_)
}


1;


