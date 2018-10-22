# Copyright 2004 by Audrey Tang <cpan@audreyt.org>

package Win32::Exe::IconFile;

use strict;
use base 'Win32::Exe::Base';
use constant FORMAT => (
    Magic		=> 'a2',
    Type		=> 'v',
    Count		=> 'v',
    'Resource::Icon'	=> [ 'a16', '{$Count}', 1 ],
    Data		=> 'a*',
);
use constant DEFAULT_ARGS => (
    Magic   => "\0\0",
    Type    => 1,
    Count   => 0,
    Data    => '',
);
use constant DISPATCH_FIELD => 'Magic';
use constant DISPATCH_TABLE => (
    "\0\0"  => '',
    "MZ"    => '__BASE__',
    '*'	    => sub { die "Invalid icon file header: $_[1]" },
);

sub icons {
    my $self = shift;
    $self->members(@_);
}

sub set_icons {
    my ($self, $icons) = @_;
    $self->SetCount(scalar @$icons);
    $self->set_members('Resource::Icon' => $icons);
    $self->refresh;

    foreach my $idx (0 .. $#{$icons}) {
	$self->icons->[$idx]->SetImageOffset(length($self->dump));
	$self->SetData( $self->Data . $icons->[$idx]->Data );
    }

    $self->refresh;
}

sub dump_iconfile {
    my $self = shift;
    my @icons = $self->icons;
    my $obj = $self->require_class('IconFile')->new;
    $obj->set_icons(\@icons);
    return $obj->dump;
}

sub write_iconfile {
    my ($self, $filename) = @_;
    $self->write_file($filename, $self->dump_iconfile);
}

1;
