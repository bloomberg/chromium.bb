# Copyright 2004 by Audrey Tang <cpan@audreyt.org>

package Win32::Exe::Section;

use strict;
use base 'Win32::Exe::Base';
use constant FORMAT => (
    Name        => 'Z8',
    VirtualSize     => 'V',
    VirtualAddress  => 'V',
    FileSize        => 'V',
    FileOffset      => 'V',
    RelocOffset     => 'V',
    LineNumOffset   => 'V',
    NumReloc        => 'v',
    NumLineNum      => 'v',
    Flags       => 'V',
);
use constant DISPATCH_FIELD => 'Name';
use constant DISPATCH_TABLE => (
    '.text' => 'Section::Code',
    '.debug'    => 'Section::Debug',
    '.data' => 'Section::Data',
    '.rdata'    => 'Section::Data',
    '.bss'  => 'Section::Data',
    '.edata'    => 'Section::Exports',
    '.idata'    => 'Section::Imports',
    '.rsrc' => 'Section::Resources',
);

use constant CONTAINS_CODE  => 0x20;
use constant CONTAINS_IDATA => 0x40;
use constant CONTAINS_UDATA => 0x80;

sub Data {
    my ($self) = @_;
    $self->{data} ||= do {
    my $v_size = $self->VirtualSize;
    my $f_size = $self->FileSize or return("\0" x $v_size);

    $f_size = $v_size if ($v_size && ( $v_size < $f_size));

    my $data = $self->parent->substr($self->FileOffset, $f_size);
    $data .= ("\x0" x ($v_size - length($data)));
    $data;
    }
}

sub SetData {
    my ($self, $data) = @_;
    my $pad_size = length($1) if $data =~ s/(\0*)\z//;

    my $exe = $self->parent;
    my $act_headersize = $exe->OptHeaderSize;
    my $exp_headersize = $exe->ExpectedOptHeaderSize;
    
    ($act_headersize && ( $act_headersize == $exp_headersize )) or die "Unsupported binary format: headersize $act_headersize ne $exp_headersize";
    
    my $index = $self->sibling_index;
    my $data_size = length($data);

    my $f_size  = $self->align($data_size, $exe->FileAlign);
    my $v_size  = $self->align($data_size, $exe->SectionAlign);
    my $f_extra = $f_size - $self->FileSize;
    my $v_extra = $v_size - $self->align($self->VirtualSize, $exe->SectionAlign);

    $self->pad_data($f_extra, $v_extra) if $f_extra;

    $self->SetVirtualSize($data_size + $pad_size);
    $data .= ("\0" x ($self->FileSize - $data_size));

    $exe->substr($self->FileOffset, length($data), $data);
    $self->update_size;
}

sub update_size {
    my ($self) = @_;

    my $exe = $self->parent;
    my $v_addr = $self->VirtualAddress;
    my $v_size = $self->VirtualSize;

    foreach my $dir ($exe->data_directories) {
    next unless $dir->VirtualAddress == $v_addr;
    $dir->SetSize($v_size);
    $dir->refresh;
    }
}

sub pad_data {
    my ($self, $f_extra, $v_extra) = @_;

    my $exe = $self->parent;
    my $offset = $self->FileOffset + $self->FileSize;

    $exe->update_debug_directory($offset, $f_extra);

    my $exe_size = $exe->size;
    if ($exe_size > $offset) {
    my $buf = $exe->substr($offset, ($exe_size - $offset));
    $exe->substr($offset + $f_extra, length($buf), $buf);
    }

    $exe->set_size($exe_size + $f_extra);
    if ($f_extra > 0) {
    $exe->SetData($exe->Data . ("\0" x $f_extra));
    }
    else {
    $exe->SetData(substr($exe->Data, 0, $f_extra));
    }

    my $index = $self->sibling_index;
    foreach my $section (@{$self->siblings}) {
    next if $section->sibling_index <= $index;
    $section->update_offset($f_extra, $v_extra);
    }

    $self->SetFileSize($self->FileSize + $f_extra);
    $exe->SetImageSize($exe->ImageSize + $v_extra);

    my $flags = $self->Flags;
    $exe->SetCodeSize($exe->CodeSize + $f_extra)   if $flags & CONTAINS_CODE;
    $exe->SetIDataSize($exe->IDataSize + $f_extra) if $flags & CONTAINS_IDATA;
    $exe->SetUDataSize($exe->UDataSize + $f_extra) if $flags & CONTAINS_UDATA;
}

sub update_offset {
    my ($self, $f_extra, $v_extra) = @_;
    return unless $f_extra > 0;

    my $exe = $self->parent;
    my $v_addr = $self->VirtualAddress;

    $self->SetVirtualAddress( $v_addr + $v_extra );
    $self->SetFileOffset( $self->FileOffset + $f_extra );

    foreach my $dir ($exe->data_directories) {
    next unless $dir->VirtualAddress == $v_addr;
    $dir->SetVirtualAddress($self->VirtualAddress);
    }
}

sub substr {
    my $self    = shift;
    my $data    = $self->Data;
    my $offset  = shift;
    my $length  = @_ ? shift(@_) : (length($data) - $offset);
    my $replace = shift;

    return substr($data, $offset, $length) if !defined $replace;

    substr($data, $offset, $length, $replace);
    $self->SetData($data);
}

1;
