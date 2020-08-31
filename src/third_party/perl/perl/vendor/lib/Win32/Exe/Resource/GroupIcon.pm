# Copyright 2004, 2010 by Audrey Tang <cpan@audreyt.org>

package Win32::Exe::Resource::GroupIcon;

use strict;
use base 'Win32::Exe::Resource';
use constant FORMAT => (
    Magic		=> 'a2',
    Type		=> 'v',
    Count		=> 'v',
    'Resource::Icon'    => [ 'a14', '{$Count}', 1 ],
);
use constant DEFAULT_ARGS => (
    Magic   => "\0\0",
    Type    => 1,
    Count   => 0,
);
use constant DELEGATE_SUBS => (
    'IconFile'	=> [ 'dump_iconfile', 'write_iconfile' ],
);

sub icons {
    my $self = shift;
    $self->members(@_);
}

sub set_icons {
    my ($self, $icons) = @_;

    $self->SetCount(scalar @$icons);
    $self->set_members('Resource::Icon' => $icons);

    my $rsrc = $self->first_parent('Resources') or return;
	
	# get the existing resource icon ids
	
	my %existids = ();
	for my $groupicon ($rsrc->objects('GroupIcon')) {	
		for my $icon ( $groupicon->icons ) {
			my $id = $icon->Id;
			$existids{$id} = 1;
		}
    }

	my $nextid = 0;
    foreach my $idx (0 .. $#{$icons}) {
		$nextid ++;
		while(exists($existids{$nextid})) { $nextid ++; }
		my $icon = $self->icons->[$idx];
		$icon->SetId($nextid);
		$rsrc->insert($self->icon_name($icon->Id), $icons->[$idx]);
    }
}

sub substr {
    my ($self, $id) = @_;
    my $section = $self->first_parent('Resources');
    return $section->res_data($self->icon_name($id));
}

sub icon_name {
    my ($self, $id) = @_;
    my @icon_name = split("/", $self->PathName, -1);
    $icon_name[1] = "#RT_ICON";
    $icon_name[2] = "#$id";
    return join("/", @icon_name);
}

1;
