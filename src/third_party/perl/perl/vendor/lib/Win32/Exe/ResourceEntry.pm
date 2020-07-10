# Copyright 2004 by Audrey Tang <cpan@audreyt.org>

package Win32::Exe::ResourceEntry;

use strict;
use base 'Win32::Exe::Base';
use constant FORMAT => (
    Data        => 'V',
    E_RVA       => 'V',
);
use constant HIGH_BIT => 0x80_00_00_00;
use Win32::Exe::ResourceData;

sub high_bit {
    my ($self) = @_;
    return +HIGH_BIT;
}

sub path {
    my ($self) = @_;
    return $self->parent->path;
}

sub PathName {
    my ($self) = @_;
    return join('/', '', $self->path, $self->Name);
}

sub VirtualAddress {
    my ($self) = @_;
    $self->E_RVA & ~($self->high_bit);
}

sub SetVirtualAddress {
    my ($self, $data) = @_;
    $self->SetE_RVA($data | $self->IsDirectory);
}

sub IsDirectory {
    my ($self) = @_;
    $self->E_RVA & ($self->high_bit);
}

sub initialize {
    my ($self) = @_;
    my $section = $self->first_parent('Resources');
    my $data = $section->substr($self->VirtualAddress, 12);
    my $res_data = Win32::Exe::ResourceData->new(\$data, { parent => $self });
    $res_data->initialize;
    $self->{res_data} = $res_data;
}

sub Data {
    my ($self) = @_;
    return $self->{res_data}->Data;
}

sub CodePage {
    my ($self) = @_;
    return $self->{res_data}->CodePage;
}

sub object {
    my ($self) = @_;
    return $self->{res_data}->object;
}

1;
