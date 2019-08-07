# NOTE: Derived from blib\lib\Crypt\CAST5_PP.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package Crypt::CAST5_PP;

#line 187 "blib\lib\Crypt\CAST5_PP.pm (autosplit into blib\lib\auto\Crypt\CAST5_PP\encrypt.al)"
sub encrypt {
  use strict;
  use integer;
  my ($cast5, $block) = @_;
  croak "Block size must be 8" if length($block) != 8;

  my $encrypt = $cast5->{encrypt};
  unless ($encrypt) {
    my $key = $cast5->{key} or croak "Call init() first";
    my $f = 'sub{my($l,$r,$i)=unpack"N2",$_[0];';

    my ($l, $r) = qw( $l $r );
    my ($op1, $op2, $op3) = qw( + ^ - );
    foreach my $round (0 .. $cast5->{rounds}-1) {
      my $km = $key->[$round];
      my $kr = $key->[$round+16];

      my $rot = "";
      if ($kr) {
        my $mask = ~(~0<<$kr) & 0xffffffff;
        my $kr2 = 32-$kr;
        $rot = "\$i=\$i<<$kr|\$i>>$kr2&$mask;"
      }

      $f .= "\$i=$km$op1$r;$rot$l^=((\$s1[\$i>>24&255]$op2\$s2[\$i>>16&255])$op3\$s3[\$i>>8&255])$op1\$s4[\$i&255];";
      ($l, $r) = ($r, $l);
      ($op1, $op2, $op3) = ($op2, $op3, $op1);
    }

    $f .= 'pack"N2",$r&0xffffffff,$l&0xffffffff}';
    $cast5->{encrypt} = $encrypt = eval $f;
  }

  return $encrypt->($block);
} # encrypt

# end of Crypt::CAST5_PP::encrypt
1;
