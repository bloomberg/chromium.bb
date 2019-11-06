#!/usr/bin/perl

package Math::Base::Convert::CalcPP;

use strict;
use vars qw($VERSION);

$VERSION = do { my @r = (q$Revision: 0.03 $ =~ /\d+/g); sprintf "%d."."%02d" x $#r, @r };

# test number < 2^32 is NOT power of 2
#
sub isnotp2 {
  my $ref = ref $_[0];
  shift if ref $_[0] || $_[0] =~ /\D/;	# class?
  $_[0] & $_[0] -1;
}

# add a long n*32 bit number toa number < 65536
# add 'n' to array digits and propagate carry, return carry
#
sub addbaseno {
  my($ap,$n) = @_;
  foreach (@$ap) {
    $_ += $n;
    return 0 unless $_ > 0xffffffff;
    $n = 1;
    $_ -= 4294967296;
  }
  1;	# carry is one on exit, else would have taken return 0 branch
}

# multiply a register of indeterminate length by a number < 65535
#
# ap    pointer to multiplicand array
# multiplier  
#
sub multiply {
  my($ap,$m) = @_;
# $m is always 2..65535
#  $m &= 0xffff;                # max value 65535       already done by VETTING
#
# perl uses doubles for arithmetic, $m << 65536 will fit
  my $carry = 0;
  foreach ( @$ap) {
    $_ *= $m;
    $_ += $carry;
    if ($_ > 0xffffffff) {
      $carry = int($_ / 4294967296);
      $_ %= 4294967296;
    } else {
      $carry = 0;
    }
  }
  push @$ap, $carry if $carry;
}

sub dividebybase {
  my($np,$divisor) = @_;
  my @dividend = @$np;		# 3% improvement
  while ($#dividend) {		# 3% improvement
    last if $dividend[0];
    shift @dividend;
  }    
  my $remainder = 0;
  my @quotient;
  while (@dividend) {
    my $work = ($dividend[0] += ($remainder * 4294967296));     
    push @quotient, int($work / $divisor);
    $remainder = $work % $divisor;
    shift @dividend;
  }
  return (\@quotient,$remainder);
}

# simple versions of conversion, works for N < ~2^49 or 10^16
#
#sub frombase {
#  my($hsh,$base,$str) = @_;
#  my $number = 0;
#  for( $str =~ /./g ) {
#    $number *= $base;
#    $number += $hsh->{$_};
#  }
#  return $number;
#}

#sub tobase {
#sub to_base
#  my($bp,$base,$num) = @_;
#  my $base   = shift;
#  return $bp->[0] if $num == 0;
#  my $str = '';
#  while( $num > 0 ) {
#    $str = $bp->[$num % $base] . $str;
#    $num = int( $num / $base );
#  }
#  return $str;
#}

# convert a number from its base to 32*N bit representation
#
sub useFROMbaseto32wide {
  my $bc = shift;
  my($ary,$hsh,$base,$str) = @{$bc}{qw(from fhsh fbase nstr)};
# check if decimal and interger from within perl's 32bit double representation
# cutoff is 999,999,999,999,999 -- a bit less than 2^50
#
#	convert directly to base 2^32 arrays
#
  my @result = (0);

  if ($base == 10 && length($str) < 16) {
#  unless ($str > 999999999999999) {	# maximum 32 bit double float integer representation
    $result[0] = $str % 4294967296;
    my $quotient = int($str / 4294967296);
    $result[1] = $quotient if $quotient;
    $bc->{b32str} = \@result;
  }
  else {
    for ($str =~ /./g) {
      multiply(\@result,$base);
      push @result, 1 if addbaseno(\@result,$hsh->{$_});	# propagate carry
    }
#    my @rv = reverse @result;
    $bc->{b32str} = \@result;
  }
  $bc;
}

#my %used = map {$_,0}(0..255);

# convert 32*N bit representation to any base < 65536
#

sub use32wideTObase {
  my $bc = shift;
  my($ary,$base,$rquot) = @{$bc}{qw(to tbase b32str)};
  my @quotient = reverse(@$rquot);
  my $quotient = \@quotient;
  my @answer;
  my $remainder;
  do {
    ($quotient,$remainder) = dividebybase($quotient,$base);
# these commented out print statements are for convert.t DO NOT REMOVE!
#$used{$remainder} = 1;
#print $remainder;
#print "    *" if $remainder > 86;
#print "\n";
    unshift @answer, $ary->[$remainder];
  } while grep {$_} @$quotient;

#foreach (sort {$b <=> $a} keys %used) {
#print " $_,\n" if $used{$_} && $_ > 85;
#print "\t$_\t=> \n" if !$used{$_} && $_ < 86;
#}
  join '', @answer;
}

1;

__END__

=head1 NAME

Math::Base::Convert::CalcPP - standard methods used by Math::Base::Convert

=head1 DESCRIPTION

This module contains the standard methods used by B<Math::Base::Convert> to
convert from one base number to another base number.

=over 4

=item * $carry = addbaseno($reg32ptr,$int)

This function adds an integer < 65536 to a long n*32 bit register and
returns the carry.

=item * multiply($reg32ptr,$int)

This function multiplies a long n*32 bit register by an integer < 65536

=item * ($qptr,$remainder) = dividebybase($reg32ptr,$int)

this function divides a long n*32 bit register by an integer < 65536 and
returns a pointer to a long n*32 bit quotient and an integer remainder.

=item * $bc->useFROMbaseto32wide

This method converts FROM an input base string to a long n*32 bit register using
an algorithim like:

	$longnum = 0;
	for $char ( $in_str =~ /./g ) {
	  $longnum *= $base;
	  $longnum += $value{$char)
	}
	return $number;

=item * $output = $bc->use32wideTObase

This method converts a long n*32 bit register TO a base number using an
algorithim like:

    $output = '';
    while( $longnum > 0 ) {
      $output = ( $longnum % $base ) . $output;
      $num = int( $longnum / $base );
    }
    return $output;

=back

=head1 AUTHOR
   
Michael Robinton, michael@bizsystems.com

=head1 COPYRIGHT

Copyright 2012-15, Michael Robinton

This program is free software; you may redistribute it and/or modify it
under the same terms as Perl itself.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 
=cut

1;
