# Copyright 2004 by Audrey Tang <cpan@audreyt.org>

package Win32::Exe::Section::Resources;

use strict;
use base 'Win32::Exe::Section';
use constant DELEGATE_SUBS => (
    'ResourceEntry' => [ 'high_bit' ],
    'ResourceEntry::Id' => [ 'rt_to_id', 'id_to_rt' ],
);

sub initialize {
    my $self = shift;
    $self->make_table(0);
    return $self;
}

sub table {
    my $self = shift;
    return $self->{table};
}

sub make_table {
    my ($self, $offset, @path) = @_;
    my $image = $self->substr($offset);
    my $table = $self->require_class('ResourceTable')->new(
    \$image, {
        parent  => $self,
        path    => \@path
    },
    );

    foreach my $entry ($table->members) {
    if ($entry->IsDirectory) {
        $self->make_table($entry->VirtualAddress, @path, $entry->Name);
    }
    else {
        $self->{table}{$entry->PathName} = $entry;
    }
    }
}

sub names {
    my ($self) = @_;
    my @rv = sort keys %{$self->{table}};
    wantarray ? @rv : \@rv;
}

sub resources {
    my ($self, $name) = @_;
    my @rv = map $self->{table}{$_}, $self->names;
    wantarray ? @rv : \@rv;
}

sub remove {
    my ($self, $name) = @_;
    delete $self->{table}{$_} for grep /^\Q$name\E/, $self->names;
}

sub insert {
    my ($self, $name, $res) = @_;
    $self->{table}{$name} = $res;
}

sub res {
    my ($self, $name) = @_;
    return $self->{table}{$name};
}

sub res_data {
    my ($self, $name) = @_;
    my $res = $self->res($name) or return;
    return $res->Data;
}

sub res_codepage {
    my ($self, $name) = @_;
    my $res = $self->res($name) or return;
    return $res->CodePage;
}

sub res_object {
    my ($self, $name) = @_;
    my $res = $self->res($name) or return;
    return $res->object;
}

sub res_image {
    my ($self, $name) = @_;
    my $res = $self->res($name) or return;
    my $object = $res->object or return $res->Data;
    return $object->dump;
}

sub first_object {
    my ($self, $type) = @_;
    foreach my $object (grep $_, map $_->object, $self->resources) {
    return $object if !$type or $object->is_type($type);
    }
    return undef;
}

sub objects {
    my ($self, $type) = @_;
    return grep { $type ? $_->is_type($type) : 1 }
       grep { $_ } map { $_->object } $self->resources;
}

sub refresh {
    my $self = shift;

    my $res_num = @{$self->resources} or return pack('V*', (0) x 4);
    my $entry_size = $self->entry_size(scalar $self->names);
    my $data_entry_size = 16 * $res_num;

    my %str_addr;
    my $str_image  = '';
    my $str_offset = $entry_size + $data_entry_size;

    foreach my $name ($self->names) {
    $name =~ s!^/!!;
    foreach my $chunk (split("/", $name, -1)) {
        $chunk =~ /^#/ and next;
        $chunk =~ s/%([0-9A-Fa-f]{2})/chr(hex($1))/eg;
        next if exists $str_addr{$chunk};

        die "String too long" if length($chunk) > 0xFFFF;

        my $addr = length($str_image);
        my $str = $self->encode_ucs2($chunk);
        $str_image .= pack('v', length($str) / 2) . $str;

        $str_addr{$chunk} = $addr + $str_offset;
    }
    }
    $str_image .= $self->pad($str_image, 8);

    my %data_entry_addr;
    my $data_entry_image = '';
    my $data_image       = '';
    my $data_offset      = $str_offset + length($str_image);

    foreach my $name ($self->names) {
    $data_entry_addr{$name} = $entry_size + length($data_entry_image);

    my $data_addr = $data_offset + length($data_image) + $self->VirtualAddress;
    $data_entry_image .= pack(
        'V4',
        $data_addr,
        length($self->res_data($name)),
        $self->res_codepage($name),
        0,
    );
    $data_image .= $self->res_data($name);
    $data_image .= $self->pad($data_image, 8);
    }

    my $entry_image = '';
    $self->make_entry(
    \$entry_image,
    '',
    [$self->names],
    \%str_addr,
    \%data_entry_addr,
    );

    length($entry_image) == $entry_size or die "Wrong size";

    $self->SetData(
    join('', $entry_image, $data_entry_image, $str_image, $data_image)
    );
}

sub entry_size {
    my ($self, $names) = @_;

    my %entries;
    foreach my $name (grep length, @$names) {
    $name =~ m!^/([^/]*)(.*)! or next;
    push(@{ $entries{$1} }, $2);
    }

    my $count = keys %entries or return 0;
    my $size = 8 * ($count + 2);
    $size += $self->entry_size($_) for values %entries;
    return $size;
}

sub make_entry {
    my ($self, $image_ref, $prefix, $names, $str_addr, $data_entry_addr) = @_;

    if (@$names == 1 and !length($names->[0])) {
    return $data_entry_addr->{$prefix};
    }

    my %entries;
    foreach my $name (@$names) {
    $name =~ m!^/([^/]*)(.*)! or next;
    my ($path, $name) = ($1, $2);
    my $type = ($path =~ /^#/) ? 'id' : 'name';
    push(@{ $entries{$type}{$path} }, $name);
    }

    my $addr = length($$image_ref);
    my $num_name = keys %{ $entries{name} };
    my $num_id   = keys %{ $entries{id} };
    $$image_ref .= pack('V3vv', 0, 0, 0, $num_name, $num_id);

    my $entry_offset = length($$image_ref);
    $$image_ref .= pack('V*', (0) x (($num_name + $num_id) * 2));

    foreach my $entry ($self->sort_entry(\%entries)) {
    my ($type, $name) = @$entry;
    my $id;
    if ($type eq 'id') {
        $id = $name;
        $id =~ s/^#//;
        $id = $self->rt_to_id($id);
    }
    else {
        (my $n = $name) =~ s/%([0-9A-Fa-f]{2})/chr(hex($1))/eg;
        $id = $str_addr->{$n} | $self->high_bit;
    }

    my $rva = $self->make_entry(
        $image_ref,
        "$prefix/$name",
        $entries{$type}{$name},
        $str_addr,
        $data_entry_addr,
    );

    substr($$image_ref, $entry_offset, 8) = pack('VV', $id, $rva);
    $entry_offset += 8;
    }

    return ($addr | $self->high_bit);
}

sub sort_entry {
    my ($self, $entries) = @_;

    my @names = map { $_->[1] } sort { $a->[0] cmp $b->[0] } map {
    my $name = lc($_);
    $name =~ s/%([0-9A-Fa-f]{2})/chr(hex($1))/eg;
    [ $name => $_ ];
    } keys %{ $entries->{name} };

    my @ids = map "#$_", sort {
    $self->rt_to_id($a) <=> $self->rt_to_id($b)
    } map substr($_, 1), keys %{ $entries->{id} };

    return(
    (map [ name => $_ ], @names),
    (map [ id   => $_ ], @ids),
    );
}

1;
