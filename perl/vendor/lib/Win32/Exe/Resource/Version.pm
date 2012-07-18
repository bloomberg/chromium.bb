# Copyright 2004 by Audrey Tang <cpan@audreyt.org>

package Win32::Exe::Resource::Version;

use strict;
use base 'Win32::Exe::Resource';
use constant FORMAT => (
    Data        => 'a*',
);
use constant FIXED_INFO => [qw(
    Signature StrucVersion FileVersionMS FileVersionLS
    ProductVersionMS ProductVersionLS FileFlagsMask FileFlags
    FileOS FileType FileSubtype FileDateMS FileDateLS
)];
use constant STRING_INFO => [qw(
    Comments CompanyName FileDescription FileVersion InternalName
    LegalCopyright LegalTrademarks OriginalFilename PrivateBuild
    ProductName ProductVersion SpecialBuild FileNumber ProductNumber
)];
use constant FI_TO_ID => {
    map { (FIXED_INFO->[$_] => $_) } (0 .. $#{+FIXED_INFO})
};
use constant LC_TO_SI => {
    (map { (lc($_) => $_) } @{+STRING_INFO}, keys %{+FI_TO_ID}),
    (map { (lc($_) => $_) } map { /^(.+)MS$/ ? $1 : () } keys %{+FI_TO_ID}),
};

sub fi_to_id {
    my ($self, $name) = @_;
    return(+FI_TO_ID->{$name});
}

sub lc_to_si {
    my ($self, $name) = @_;
    return(+LC_TO_SI->{lc($name)} || $name);
}

sub info {
    my ($self) = @_;
    return $self->{info};
}

sub set_info {
    my ($self, $info) = @_;
    $self->{info} = $info;
}

sub initialize {
    my ($self) = @_;
    $self->set_info($self->decode_info($self->Data));
    die 'Invalid structure' unless $self->check;
}

sub refresh {
    my ($self) = @_;
    $self->SetData($self->encode_info($self->info));
    my $rsrc = $self->first_parent('Resources');
    $rsrc->remove("/#RT_VERSION");
    $rsrc->insert("/#RT_VERSION/#1/#0" => $self);
    $rsrc->refresh;
    $self->initialize;
}

sub encode_info {
    my ($self, $info) = @_;

    my $key = shift(@$info);
    $key = $self->encode_ucs2("$key\0");

    my $val = shift(@$info);
    my ($type, $vallen);

    if (ref $val) {
    $type   = 0;                   # binary
    $val    = pack('V*', @$val);
    $vallen = length($val);
    }
    elsif (length $val) {
    $type   = 1;                          # text;
    $val    = $self->encode_ucs2("$val\0");
    $vallen = length($val) / 2;
    }
    else {
    $type   = 1;
    $vallen = 0;
    }

    my @sub_objects;
    foreach my $sub_info (@$info) {
    my $obj = $self->encode_info($sub_info);
    push(@sub_objects, $obj);
    }

    my $buf = pack('v3', 0, $vallen, $type) . $key;
    $buf .= $self->pad($buf, 4);
    $buf .= $val;

    foreach my $sub_object (@sub_objects) {
    $buf .= $self->pad($buf, 4);
    $buf .= $sub_object;
    }

    substr($buf, 0, 2, pack('v', length($buf)));
    return $buf;
}

sub decode_info {
    my $self  = shift;
    my $level = $_[1] || 1;

    my ($len, $vallen, $type) = unpack('v3', $_[0]);
    die 'No record length' unless $len;
    die 'Long length' if $len > length($_[0]);

    my $buf = substr($_[0], 0, $len);
    substr($_[0], 0, $self->align($len, 4)) = '';

    my $endkey = index($buf, "\0\0", 6);
    while ($endkey > 0 and ($endkey % 2)) {
    $endkey = index($buf, "\0\0", $endkey + 1);
    }

    die 'Invalid endkey' if $endkey < 6 or $endkey > $len - $vallen;;

    my $key    = substr($buf, 6, $endkey - 6);
    my $u8_key = $self->decode_ucs2($key);

    my @res = ($u8_key);
    $endkey = $self->align($endkey + 2, 4);
    substr($buf, 0, $endkey, '');

    if ($vallen) {
    $vallen *= 2 if $level == 4;    # only for strings
    my $val = substr($buf, 0, $vallen);
    if ($type) {
        $val = $self->decode_ucs2($val);
        $val =~ s/\0\z//;
    }
    else {
        $val = [ unpack('V*', $val) ];
    }
    push(@res, $val);
    $vallen = $self->align($vallen, 4);

    substr($buf, 0, $vallen) = '';
    }
    else {
    push(@res, '');
    }

    while (length $buf) {
    push(@res, $self->decode_info($buf, $level + 1));
    }

    return \@res;
}

sub empty_info {
    [ 'VS_VERSION_INFO', [ 0xFEEF04BD, 1 << 16, (0) x 11 ] ];
}

sub check_info {
    my ($self, $info) = @_;
    return 0 unless $info->[0] eq 'VS_VERSION_INFO';
    return 0 unless ref($info->[1]);
    return 0 unless $info->[1][0] == 0xFEEF04BD;
    return 0 unless $self->check_sub_info($info);
    return 1;
}

sub check_sub_info {
    my ($self, $info) = @_;
    return unless UNIVERSAL::isa($info, 'ARRAY');
    return if @$info < 2;
    return unless defined($info->[0]) and defined($info->[1]);
    return unless !ref($info->[0]) and length($info->[0]);
    return unless !ref($info->[1]) or UNIVERSAL::isa($info->[1], 'ARRAY');
    foreach my $idx (2 .. @$info - 1) {
    return 0 unless $self->check_sub_info($info->[$idx]);
    }
    return 1;
}

sub get {
    my ($self, $name) = @_;
    $name =~ s!\\!/!g;
    $name = $self->lc_to_si($name);
    my $info = $self->info;

    if ($name eq '/') {
    return undef unless ref $info->[1];
    return $info->[1];
    }

    my $fixed = $self->fi_to_id($name);
    if (defined $fixed) {
    my $struct = $info->[1];
    return undef unless $struct && ref($struct);
    return $struct->[$fixed];
    }

    $fixed = $self->fi_to_id($name.'MS');
    if (defined $fixed) {
    my $struct = $info->[1];
    return undef unless $struct && ref($struct);
    my $ms = $struct->[$fixed];
    my $ls = $struct->[ $self->fi_to_id($name.'LS') ];
    return join(',', $self->split_dword($ms), $self->split_dword($ls));
    }

    my $s;
    if ($name =~ s!^/!!) {
    $s = $info;
    while ($name =~ s!^([^/]+)/!!) {
        $s = $self->find_info($s, $1) or return undef;
    }
    }
    else {
    $s = $self->find_info($info, 'StringFileInfo') or return undef;
    if (my $cur_trans = $self->{cur_trans}) {
        $s = $self->find_info($s, $cur_trans, 1) or return undef;
    }
    else {
        $s = $s->[2] or return undef;
        $self->{cur_trans} = $s->[0];
    }
    }

    $s = $self->find_info($s, $name) or return undef;
    return $s->[1];
}

sub set {
    my ($self, $name, $value) = @_;
    $name =~ s!\\!/!g;
    $name = $self->lc_to_si($name);
    my $info = $self->info;

    if ($name eq '/') {
    if (!defined $value) {
        $info->[1] = '';
    }
    elsif (UNIVERSAL::isa($value, 'ARRAY') and @$value == 13) {
        $info->[1] = $value;
    }
    else {
        die 'Invalid array assigned';
    }
    }

    my $fixed = $self->fi_to_id($name);
    if (defined $fixed) {
    $value = oct($value) if $value =~ /^0/;
    $info->[1][$fixed] = $value;
    return;
    }

    $fixed = $self->fi_to_id($name.'MS');
    if (defined $fixed) {
    my @value = split(/[,.]/, $value, -1);
    if (@value == 4) {
        $value[0] = ($value[0] << 16) | $value[1];
        $value[1] = ($value[2] << 16) | $value[3];
        splice(@value, 2);
    }

    die 'Invalid MS/LS value' if @value != 2;
    $info->[1][$fixed] = $value[0] || 0;
    $info->[1][$self->fi_to_id($name.'LS')] = $value[1] || 0;
    return;
    }

    my $container = $info;

    if ($name =~ s!^/!!) {
    while ($name =~ s!^([^/]+)/!!) {
        my $n = $1;
        my $s = $self->find_info($container, $n);
        unless ($s) {
        $s = [ $n => '' ];
        push(@$container, $s);
        }
        $container = $s;
    }
    }
    else {
    my $s = $self->find_info($container, 'StringFileInfo');
    unless ($s) {
        $s = [ StringFileInfo => '' ];
        push(@$container, $s);
    }
    $container = $s;

    my $cur_trans = $self->{cur_trans};
    unless ($cur_trans) {
        if (@$container > 2) {
        $cur_trans = $container->[2][0];
        }
        else {
        $cur_trans = '000004B0';    # Language Neutral && CP 1200 = Unicode
        }
        $self->{cur_trans} = $cur_trans;
    }

    $s = $self->find_info($container, $cur_trans, 1);
    unless ($s) {
        $s = [ $cur_trans => '' ];
        push(@$container, $s);
    }
    $container = $s;
    }

    my ($kv, $kv_index) = $self->find_info($container, $name);
    unless ($kv) {
    push(@$container, [ $name => $value ]) if defined $value;
    return;
    }

    if (defined $value) {
    $kv->[1] = $value;
    }
    else {
    splice(@$container, $kv_index, 1);
    }
}

sub check {
    my $self = shift;
    return $self->check_info($self->info);
}

sub find_info {
    my ($self, $info, $name, $ignore) = @_;
    my $index;

    if ($name =~ /^#(\d+)$/) {
    $index = $1 - 1 + 2;
    $index = undef if $index < 2 || $index >= @$info;
    }
    else {
    for (2 .. @$info - 1) {
        my $e = $info->[$_];
        if ($e->[0] eq $name or $ignore && lc($e->[0]) eq lc($name)) {
        $index = $_;
        last;
        }
    }
    }
    if ($index) {
    return $info->[$index] unless wantarray;
    return ($info->[$index], $index);
    }

    return undef unless wantarray;
    return (undef, undef);
}

sub split_dword {
    my ($self, $dword) = @_;
    return ($dword >> 16), ($dword & 0xFFFF);
}

1;
