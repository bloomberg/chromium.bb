# Copyright (c) 2000-2005 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package Convert::ASN1;
{
  $Convert::ASN1::VERSION = '0.27';
}

use strict;
use warnings;

BEGIN {
  local $SIG{__DIE__};
  eval { require bytes and 'bytes'->import };
}

# These are the subs that do the decode, they are called with
# 0      1    2       3     4
# $optn, $op, $stash, $var, $buf
# The order must be the same as the op definitions above

my @decode = (
  sub { die "internal error\n" },
  \&_dec_boolean,
  \&_dec_integer,
  \&_dec_bitstring,
  \&_dec_string,
  \&_dec_null,
  \&_dec_object_id,
  \&_dec_real,
  \&_dec_sequence,
  \&_dec_explicit,
  \&_dec_set,
  \&_dec_time,
  \&_dec_time,
  \&_dec_utf8,
  undef, # ANY
  undef, # CHOICE
  \&_dec_object_id,
  \&_dec_bcd,
);

my @ctr;
@ctr[opBITSTR, opSTRING, opUTF8] = (\&_ctr_bitstring,\&_ctr_string,\&_ctr_string);


sub _decode {
  my ($optn, $ops, $stash, $pos, $end, $seqof, $larr) = @_;
  my $idx = 0;

  # we try not to copy the input buffer at any time
  foreach my $buf ($_[-1]) {
    OP:
    foreach my $op (@{$ops}) {
      my $var = $op->[cVAR];

      if (length $op->[cTAG]) {

	TAGLOOP: {
	  my($tag,$len,$npos,$indef) = _decode_tl($buf,$pos,$end,$larr)
	    or do {
	      next OP if $pos==$end and ($seqof || defined $op->[cEXT]);
	      die "decode error";
	    };

	  if ($tag eq $op->[cTAG]) {

	    &{$decode[$op->[cTYPE]]}(
	      $optn,
	      $op,
	      $stash,
	      # We send 1 if there is not var as if there is the decode
	      # should be getting undef. So if it does not get undef
	      # it knows it has no variable
	      ($seqof ? $seqof->[$idx++] : defined($var) ? $stash->{$var} : ref($stash) eq 'SCALAR' ? $$stash : 1),
	      $buf,$npos,$len, $larr
	    );

	    $pos = $npos+$len+$indef;

	    redo TAGLOOP if $seqof && $pos < $end;
	    next OP;
	  }

	  if ($tag eq ($op->[cTAG] | pack("C",ASN_CONSTRUCTOR))
	      and my $ctr = $ctr[$op->[cTYPE]]) 
	  {
	    _decode(
	      $optn,
	      [$op],
	      undef,
	      $npos,
	      $npos+$len,
	      (\my @ctrlist),
	      $larr,
	      $buf,
	    );

	    ($seqof ? $seqof->[$idx++] : defined($var) ? $stash->{$var} : ref($stash) eq 'SCALAR' ? $$stash : undef)
		= &{$ctr}(@ctrlist);
	    $pos = $npos+$len+$indef;

	    redo TAGLOOP if $seqof && $pos < $end;
	    next OP;

	  }

	  if ($seqof || defined $op->[cEXT]) {
	    next OP;
	  }

	  die "decode error " . unpack("H*",$tag) ."<=>" . unpack("H*",$op->[cTAG]), " ",$pos," ",$op->[cTYPE]," ",$op->[cVAR]||'';
        }
      }
      else { # opTag length is zero, so it must be an ANY, CHOICE or EXTENSIONS
	
	if ($op->[cTYPE] == opANY) {

	  ANYLOOP: {

	    my($tag,$len,$npos,$indef) = _decode_tl($buf,$pos,$end,$larr)
	      or do {
		next OP if $pos==$end and ($seqof || defined $op->[cEXT]);
		die "decode error";
	      };

	    $len += $npos - $pos + $indef;

            my $handler;
            if ($op->[cDEFINE]) {
              $handler = $optn->{oidtable} && $optn->{oidtable}{$stash->{$op->[cDEFINE]}};
              $handler ||= $optn->{handlers}{$op->[cVAR]}{$stash->{$op->[cDEFINE]}};
            }

	    ($seqof ? $seqof->[$idx++] : ref($stash) eq 'SCALAR' ? $$stash : $stash->{$var})
	      = $handler ? $handler->decode(substr($buf,$pos,$len)) : substr($buf,$pos,$len);

	    $pos += $len;

	    redo ANYLOOP if $seqof && $pos < $end;
	  }
	}
	elsif ($op->[cTYPE] == opCHOICE) {

	  CHOICELOOP: {
	    my($tag,$len,$npos,$indef) = _decode_tl($buf,$pos,$end,$larr)
	      or do {
		next OP if $pos==$end and ($seqof || defined $op->[cEXT]);
		die "decode error";
	      };
	    my $extensions;
	    foreach my $cop (@{$op->[cCHILD]}) {

	      if ($tag eq $cop->[cTAG]) {

		my $nstash = $seqof
			? ($seqof->[$idx++]={})
			: defined($var)
				? ($stash->{$var}={})
				: ref($stash) eq 'SCALAR'
					? ($$stash={}) : $stash;

		&{$decode[$cop->[cTYPE]]}(
		  $optn,
		  $cop,
		  $nstash,
		  ($cop->[cVAR] ? $nstash->{$cop->[cVAR]} : undef),
		  $buf,$npos,$len,$larr,
		);

		$pos = $npos+$len+$indef;

		redo CHOICELOOP if $seqof && $pos < $end;
		next OP;
	      }

	      if ($cop->[cTYPE] == opEXTENSIONS) {
		$extensions = 1;
		next;
	      }

	      unless (length $cop->[cTAG]) {
		eval {
		  _decode(
		    $optn,
		    [$cop],
		    (\my %tmp_stash),
		    $pos,
		    $npos+$len+$indef,
		    undef,
		    $larr,
		    $buf,
		  );

		  my $nstash = $seqof
			  ? ($seqof->[$idx++]={})
			  : defined($var)
				  ? ($stash->{$var}={})
				  : ref($stash) eq 'SCALAR'
					  ? ($$stash={}) : $stash;

		  @{$nstash}{keys %tmp_stash} = values %tmp_stash;

		} or next;

		$pos = $npos+$len+$indef;

		redo CHOICELOOP if $seqof && $pos < $end;
		next OP;
	      }

	      if ($tag eq ($cop->[cTAG] | pack("C",ASN_CONSTRUCTOR))
		  and my $ctr = $ctr[$cop->[cTYPE]]) 
	      {
		my $nstash = $seqof
			? ($seqof->[$idx++]={})
			: defined($var)
				? ($stash->{$var}={})
				: ref($stash) eq 'SCALAR'
					? ($$stash={}) : $stash;

		_decode(
		  $optn,
		  [$cop],
		  undef,
		  $npos,
		  $npos+$len,
		  (\my @ctrlist),
		  $larr,
		  $buf,
		);

		$nstash->{$cop->[cVAR]} = &{$ctr}(@ctrlist);
		$pos = $npos+$len+$indef;

		redo CHOICELOOP if $seqof && $pos < $end;
		next OP;
	      }
	    }

	    if ($pos < $end && $extensions) {
	      $pos = $npos+$len+$indef;

	      redo CHOICELOOP if $seqof && $pos < $end;
	      next OP;
	    }
	  }
	  die "decode error" unless $op->[cEXT];
	}
	elsif ($op->[cTYPE] == opEXTENSIONS) {
	    $pos = $end; # Skip over the rest
        }
	else {
	  die "this point should never be reached";
	}
      }
    }
  }
  die "decode error $pos $end" unless $pos == $end;
}


sub _dec_boolean {
# 0      1    2       3     4     5     6
# $optn, $op, $stash, $var, $buf, $pos, $len

  $_[3] = unpack("C",substr($_[4],$_[5],1)) ? 1 : 0;
  1;
}


sub _dec_integer {
# 0      1    2       3     4     5     6
# $optn, $op, $stash, $var, $buf, $pos, $len

  my $buf = substr($_[4],$_[5],$_[6]);
  my $tmp = unpack("C",$buf) & 0x80 ? pack("C",255) : pack("C",0);
  if ($_[6] > 4) {
      $_[3] = os2ip($buf, $_[0]->{decode_bigint} || 'Math::BigInt');
  } else {
      # N unpacks an unsigned value
      $_[3] = unpack("l",pack("l",unpack("N", $tmp x (4-$_[6]) . $buf)));
  }
  1;
}


sub _dec_bitstring {
# 0      1    2       3     4     5     6
# $optn, $op, $stash, $var, $buf, $pos, $len

  $_[3] = [ substr($_[4],$_[5]+1,$_[6]-1), ($_[6]-1)*8-unpack("C",substr($_[4],$_[5],1)) ];
  1;
}


sub _dec_string {
# 0      1    2       3     4     5     6
# $optn, $op, $stash, $var, $buf, $pos, $len

  $_[3] = substr($_[4],$_[5],$_[6]);
  1;
}


sub _dec_null {
# 0      1    2       3     4     5     6
# $optn, $op, $stash, $var, $buf, $pos, $len

  $_[3] = exists($_[0]->{decode_null}) ? $_[0]->{decode_null} : 1;
  1;
}


sub _dec_object_id {
# 0      1    2       3     4     5     6
# $optn, $op, $stash, $var, $buf, $pos, $len

  my @data = unpack("w*",substr($_[4],$_[5],$_[6]));
  if ($_[1]->[cTYPE] == opOBJID and @data > 1) {
    if ($data[0] < 40) {
      splice(@data, 0, 1, 0, $data[0]);
    }
    elsif ($data[0] < 80) {
      splice(@data, 0, 1, 1, $data[0] - 40);
    }
    else {
      splice(@data, 0, 1, 2, $data[0] - 80);
    }
  }
  $_[3] = join(".", @data);
  1;
}


my @_dec_real_base = (2,8,16);

sub _dec_real {
# 0      1    2       3     4     5     6
# $optn, $op, $stash, $var, $buf, $pos, $len

  $_[3] = 0.0, return unless $_[6];

  my $first = unpack("C",substr($_[4],$_[5],1));
  if ($first & 0x80) {
    # A real number

    require POSIX;

    my $exp;
    my $expLen = $first & 0x3;
    my $estart = $_[5]+1;

    if($expLen == 3) {
      $estart++;
      $expLen = unpack("C",substr($_[4],$_[5]+1,1));
    }
    else {
      $expLen++;
    }
    _dec_integer(undef, undef, undef, $exp, $_[4],$estart,$expLen);

    my $mant = 0.0;
    for (reverse unpack("C*",substr($_[4],$estart+$expLen,$_[6]-1-$expLen))) {
      $exp +=8, $mant = (($mant+$_) / 256) ;
    }

    $mant *= 1 << (($first >> 2) & 0x3);
    $mant = - $mant if $first & 0x40;

    $_[3] = $mant * POSIX::pow($_dec_real_base[($first >> 4) & 0x3], $exp);
    return;
  }
  elsif($first & 0x40) {
    $_[3] =   POSIX::HUGE_VAL(),return if $first == 0x40;
    $_[3] = - POSIX::HUGE_VAL(),return if $first == 0x41;
  }
  elsif(substr($_[4],$_[5],$_[6]) =~ /^.([-+]?)0*(\d+(?:\.\d+(?:[Ee][-+]?\d+)?)?)$/s) {
    $_[3] = eval "$1$2";
    return;
  }

  die "REAL decode error\n";
}


sub _dec_explicit {
# 0      1    2       3     4     5     6     7
# $optn, $op, $stash, $var, $buf, $pos, $len, $larr

  local $_[1][cCHILD][0][cVAR] = $_[1][cVAR] unless $_[1][cCHILD][0][cVAR];

  _decode(
    $_[0], #optn
    $_[1]->[cCHILD],   #ops
    $_[2], #stash
    $_[5], #pos
    $_[5]+$_[6], #end
    undef, #loop
    $_[7],
    $_[4], #buf
  );
  1;
}
sub _dec_sequence {
# 0      1    2       3     4     5     6     7
# $optn, $op, $stash, $var, $buf, $pos, $len, $larr

  if (defined( my $ch = $_[1]->[cCHILD])) {
    _decode(
      $_[0], #optn
      $ch,   #ops
      (defined($_[3]) || $_[1]->[cLOOP]) ? $_[2] : ($_[3]= {}), #stash
      $_[5], #pos
      $_[5]+$_[6], #end
      $_[1]->[cLOOP] && ($_[3]=[]), #loop
      $_[7],
      $_[4], #buf
    );
  }
  else {
    $_[3] = substr($_[4],$_[5],$_[6]);
  }
  1;
}


sub _dec_set {
# 0      1    2       3     4     5     6     7
# $optn, $op, $stash, $var, $buf, $pos, $len, $larr

  # decode SET OF the same as SEQUENCE OF
  my $ch = $_[1]->[cCHILD];
  goto &_dec_sequence if $_[1]->[cLOOP] or !defined($ch);

  my ($optn, $pos, $larr) = @_[0,5,7];
  my $stash = defined($_[3]) ? $_[2] : ($_[3]={});
  my $end = $pos + $_[6];
  my @done;
  my $extensions;

  while ($pos < $end) {
    my($tag,$len,$npos,$indef) = _decode_tl($_[4],$pos,$end,$larr)
      or die "decode error";

    my ($idx, $any, $done) = (-1);

SET_OP:
    foreach my $op (@$ch) {
      $idx++;
      if (length($op->[cTAG])) {
	if ($tag eq $op->[cTAG]) {
	  my $var = $op->[cVAR];
	  &{$decode[$op->[cTYPE]]}(
	    $optn,
	    $op,
	    $stash,
	    # We send 1 if there is not var as if there is the decode
	    # should be getting undef. So if it does not get undef
	    # it knows it has no variable
	    (defined($var) ? $stash->{$var} : 1),
	    $_[4],$npos,$len,$larr,
	  );
	  $done = $idx;
	  last SET_OP;
	}
	if ($tag eq ($op->[cTAG] | pack("C",ASN_CONSTRUCTOR))
	    and my $ctr = $ctr[$op->[cTYPE]]) 
	{
	  _decode(
	    $optn,
	    [$op],
	    undef,
	    $npos,
	    $npos+$len,
	    (\my @ctrlist),
	    $larr,
	    $_[4],
	  );

	  $stash->{$op->[cVAR]} = &{$ctr}(@ctrlist)
	    if defined $op->[cVAR];
	  $done = $idx;
	  last SET_OP;
	}
	next SET_OP;
      }
      elsif ($op->[cTYPE] == opANY) {
	$any = $idx;
      }
      elsif ($op->[cTYPE] == opCHOICE) {
	my $var = $op->[cVAR];
	foreach my $cop (@{$op->[cCHILD]}) {
	  if ($tag eq $cop->[cTAG]) {
	    my $nstash = defined($var) ? ($stash->{$var}={}) : $stash;

	    &{$decode[$cop->[cTYPE]]}(
	      $optn,
	      $cop,
	      $nstash,
	      $nstash->{$cop->[cVAR]},
	      $_[4],$npos,$len,$larr,
	    );
	    $done = $idx;
	    last SET_OP;
	  }
	  if ($tag eq ($cop->[cTAG] | pack("C",ASN_CONSTRUCTOR))
	      and my $ctr = $ctr[$cop->[cTYPE]]) 
	  {
	    my $nstash = defined($var) ? ($stash->{$var}={}) : $stash;

	    _decode(
	      $optn,
	      [$cop],
	      undef,
	      $npos,
	      $npos+$len,
	      (\my @ctrlist),
	      $larr,
	      $_[4],
	    );

	    $nstash->{$cop->[cVAR]} = &{$ctr}(@ctrlist);
	    $done = $idx;
	    last SET_OP;
	  }
	}
      }
      elsif ($op->[cTYPE] == opEXTENSIONS) {
	  $extensions = $idx;
      }
      else {
	die "internal error";
      }
    }

    if (!defined($done) and defined($any)) {
      my $var = $ch->[$any][cVAR];
      $stash->{$var} = substr($_[4],$pos,$len+$npos-$pos) if defined $var;
      $done = $any;
    }

    if( !defined($done) && defined($extensions) ) {
      $done = $extensions;
    }

    die "decode error" if !defined($done) or $done[$done]++;

    $pos = $npos + $len + $indef;
  }

  die "decode error" unless $end == $pos;

  foreach my $idx (0..$#{$ch}) {
    die "decode error" unless $done[$idx] or $ch->[$idx][cEXT] or $ch->[$idx][cTYPE] == opEXTENSIONS;
  }

  1;
}


my %_dec_time_opt = ( unixtime => 0, withzone => 1, raw => 2);

sub _dec_time {
# 0      1    2       3     4     5     6
# $optn, $op, $stash, $var, $buf, $pos, $len

  my $mode = $_dec_time_opt{$_[0]->{'decode_time'} || ''} || 0;

  if ($mode == 2 or $_[6] == 0) {
    $_[3] = substr($_[4],$_[5],$_[6]);
    return;
  }

  my @bits = (substr($_[4],$_[5],$_[6])
     =~ /^((?:\d\d)?\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)((?:\.\d{1,3})?)(([-+])(\d\d)(\d\d)|Z)/)
     or die "bad time format";

  if ($bits[0] < 100) {
    $bits[0] += 100 if $bits[0] < 50;
  }
  else {
    $bits[0] -= 1900;
  }
  $bits[1] -= 1;
  require Time::Local;
  my $time = Time::Local::timegm(@bits[5,4,3,2,1,0]);
  $time += $bits[6] if length $bits[6];
  my $offset = 0;
  if ($bits[7] ne 'Z') {
    $offset = $bits[9] * 3600 + $bits[10] * 60;
    $offset = -$offset if $bits[8] eq '-';
    $time -= $offset;
  }
  $_[3] = $mode ? [$time,$offset] : $time;
}


sub _dec_utf8 {
# 0      1    2       3     4     5     6
# $optn, $op, $stash, $var, $buf, $pos, $len

  BEGIN {
    unless (CHECK_UTF8) {
      local $SIG{__DIE__};
      eval { require bytes } and 'bytes'->unimport;
      eval { require utf8  } and 'utf8'->import;
    }
  }

  if (CHECK_UTF8) {
    $_[3] = Encode::decode('utf8', substr($_[4],$_[5],$_[6]));
  }
  else {
    $_[3] = (substr($_[4],$_[5],$_[6]) =~ /(.*)/s)[0];
  }

  1;
}


sub _decode_tl {
  my($pos,$end,$larr) = @_[1,2,3];

  return if $pos >= $end;

  my $indef = 0;

  my $tag = substr($_[0], $pos++, 1);

  if((unpack("C",$tag) & 0x1f) == 0x1f) {
    my $b;
    my $n=1;
    do {
      return if $pos >= $end;
      $tag .= substr($_[0],$pos++,1);
      $b = ord substr($tag,-1);
    } while($b & 0x80);
  }
  return if $pos >= $end;

  my $len = ord substr($_[0],$pos++,1);

  if($len & 0x80) {
    $len &= 0x7f;

    if ($len) {
      return if $pos+$len > $end ;

      my $padding = $len < 4 ? "\0" x (4 - $len) : "";
      ($len,$pos) = (unpack("N", $padding . substr($_[0],$pos,$len)), $pos+$len);
    }
    else {
      unless (exists $larr->{$pos}) {
        _scan_indef($_[0],$pos,$end,$larr) or return;
      }
      $indef = 2;
      $len = $larr->{$pos};
    }
  }

  return if $pos+$len+$indef > $end;

  # return the tag, the length of the data, the position of the data
  # and the number of extra bytes for indefinate encoding

  ($tag, $len, $pos, $indef);
}

sub _scan_indef {
  my($pos,$end,$larr) = @_[1,2,3];
  my @depth = ( $pos );

  while(@depth) {
    return if $pos+2 > $end;

    if (substr($_[0],$pos,2) eq "\0\0") {
      my $end = $pos;
      my $stref = shift @depth;
      # replace pos with length = end - pos
      $larr->{$stref} = $end - $stref;
      $pos += 2;
      next;
    }

    my $tag = substr($_[0], $pos++, 1);

    if((unpack("C",$tag) & 0x1f) == 0x1f) {
      my $b;
      do {
	$tag .= substr($_[0],$pos++,1);
	$b = ord substr($tag,-1);
      } while($b & 0x80);
    }
    return if $pos >= $end;

    my $len = ord substr($_[0],$pos++,1);

    if($len & 0x80) {
      if ($len &= 0x7f) {
	return if $pos+$len > $end ;

	my $padding = $len < 4 ? "\0" x (4 - $len) : "";
	$pos += $len + unpack("N", $padding . substr($_[0],$pos,$len));
      }
      else {
        # reserve another list element
        unshift @depth, $pos;
      }
    }
    else {
      $pos += $len;
    }
  }

  1;
}

sub _ctr_string { join '', @_ }

sub _ctr_bitstring {
  [ join('', map { $_->[0] } @_), $_[-1]->[1] ]
}

sub _dec_bcd {
# 0      1    2       3     4     5     6
# $optn, $op, $stash, $var, $buf, $pos, $len

  ($_[3] = unpack("H*", substr($_[4],$_[5],$_[6]))) =~ s/[fF]$//;
  1;
}
1;

