use strict; use warnings;
package IO::All::MLDBM;

use IO::All::DBM -base;

field _serializer => 'Data::Dumper';

sub mldbm {
    my $self = shift;
    bless $self, __PACKAGE__;
    my ($serializer) = grep { /^(Storable|Data::Dumper|FreezeThaw)$/ } @_;
    $self->_serializer($serializer) if defined $serializer;
    my @dbm_list = grep { not /^(Storable|Data::Dumper|FreezeThaw)$/ } @_;
    $self->_dbm_list([@dbm_list]);
    return $self;
}

sub tie_dbm {
    my $self = shift;
    my $filename = $self->name;
    my $dbm_class = $self->_dbm_class;
    my $serializer = $self->_serializer;
    eval "use MLDBM qw($dbm_class $serializer)";
    $self->throw("Can't open '$filename' as MLDBM:\n$@") if $@;
    my $hash;
    my $db = tie %$hash, 'MLDBM', $filename, $self->mode, $self->perms,
        @{$self->_dbm_extra}
      or $self->throw("Can't open '$filename' as MLDBM file:\n$!");
    $self->add_utf8_dbm_filter($db)
      if $self->_has_utf8;
    $self->tied_file($hash);
}

1;
