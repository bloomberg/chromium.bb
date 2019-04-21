# NOTE: Derived from blib\lib\Crypt\CAST5_PP.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package Crypt::CAST5_PP;

#line 223 "blib\lib\Crypt\CAST5_PP.pm (autosplit into blib\lib\auto\Crypt\CAST5_PP\decrypt.al)"
sub decrypt {
  use strict;
  use integer;
  my ($cast5, $block) = @_;
  croak "Block size must be 8" if length($block) != 8;

  my $decrypt = $cast5->{decrypt};
  unless ($decrypt) {
    my $key = $cast5->{key} or croak "Call init() first";
    my $rounds = $cast5->{rounds};
    my $f = 'sub{my($r,$l,$i)=unpack"N2",$_[0];';

    my ($l, $r) = qw( $r $l );
    my ($op1, $op2, $op3) = qw( - + ^ );
    foreach (1 .. $rounds%3) { ($op1, $op2, $op3) = ($op2, $op3, $op1) }
    foreach my $round (1 .. $rounds) {
      my $km = $key->[$rounds-$round];
      my $kr = $key->[$rounds-$round+16];

      my $rot = "";
      if ($kr) {
        my $mask = ~(~0<<$kr) & 0xffffffff;
        my $kr2 = 32-$kr;
        $rot = "\$i=\$i<<$kr|\$i>>$kr2&$mask;"
      }

      $f .= "\$i=$km$op1$r;$rot$l^=((\$s1[\$i>>24&255]$op2\$s2[\$i>>16&255])$op3\$s3[\$i>>8&255])$op1\$s4[\$i&255];";
      ($l, $r) = ($r, $l);
      ($op1, $op2, $op3) = ($op3, $op1, $op2);
    }

    $f .= 'pack"N2",$l&0xffffffff,$r&0xffffffff}';
    $cast5->{decrypt} = $decrypt = eval $f;
  }

  return $decrypt->($block);
} # decrypt

# end CAST5_PP.pm
1;
# end of Crypt::CAST5_PP::decrypt
