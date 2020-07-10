# Copyright 2004 by Audrey Tang <cpan@audreyt.org>

package Win32::Exe::ResourceEntry::Id;

use strict;
use base 'Win32::Exe::ResourceEntry';
use constant SUBFORMAT => (
    Id          => 'V',
);
use constant RESOURCE_TYPES => [qw(
    _       CURSOR          BITMAP          ICON        MENU
    DIALOG  STRING          FONTDIR         FONT        ACCELERATOR
    RCDATA  MESSAGETABLE    GROUP_CURSOR    _           GROUP_ICON
    _       VERSION         DLGINCLUDE      _           PLUGPLAY
    VXD     ANICURSOR       ANIICON         HTML        MANIFEST
)];
use constant RT_TO_ID => {
    map { ('RT_'.RESOURCE_TYPES->[$_] => $_) }
    (0 .. $#{+RESOURCE_TYPES})
};
use constant ID_TO_RT => { reverse %{+RT_TO_ID} };

sub Name {
    my ($self) = @_;
    my $id = $_[0]->Id;
    $id = $self->id_to_rt($id) if $self->parent->depth < 1;
    return "#$id";
}

sub SetName {
    my ($self, $name) = @_;
    $name =~ s/^#//;
    $self->SetId( $self->rt_to_id($name) );
}

sub id_to_rt {
    my ($self, $id) = @_;
    return(+ID_TO_RT->{$id} || $id);
}

sub rt_to_id {
    my ($self, $rt) = @_;
    return(+RT_TO_ID->{$rt} || $rt);
}

1;
