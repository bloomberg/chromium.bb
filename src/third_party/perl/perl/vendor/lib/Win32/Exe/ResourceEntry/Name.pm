# Copyright 2004 by Audrey Tang <cpan@audreyt.org>

package Win32::Exe::ResourceEntry::Name;

use strict;
use base 'Win32::Exe::ResourceEntry';
use constant SUBFORMAT => (
    N_RVA	    => 'V',
);

sub NameAddress {
    my ($self) = @_;
    $self->N_RVA & ~($self->high_bit);
}

sub SetNameAddress {
    my ($self, $data) = @_;
    $self->SetN_RVA($data | $self->IsDirectory);
}

sub IsEscaped {
    my ($self) = @_;
    $self->N_RVA & ($self->high_bit);
}

sub Name {
    my ($self) = @_;
    my $section = $self->first_parent('Resources');
    my $addr = $self->NameAddress;
    my $size = unpack('v', $section->substr($addr, 2));
    my $ustr = $section->substr($addr + 2, $size * 2);
    my $name = $self->decode_ucs2($ustr);
    $name =~ s{([%#/])}{sprintf('%%%02X', ord($1))}eg;
    return $name;
}

sub SetName {
    die "XXX unimplemented";
}

1;
