package # hide from PAUSE
    DBIx::Class::Relationship::CascadeActions;

use strict;
use warnings;
use DBIx::Class::Carp;
use namespace::clean;

our %_pod_inherit_config =
  (
   class_map => { 'DBIx::Class::Relationship::CascadeActions' => 'DBIx::Class::Relationship' }
  );

sub delete {
  my ($self, @rest) = @_;
  return $self->next::method(@rest) unless ref $self;
    # I'm just ignoring this for class deletes because hell, the db should
    # be handling this anyway. Assuming we have joins we probably actually
    # *could* do them, but I'd rather not.

  my $source = $self->result_source;
  my %rels = map { $_ => $source->relationship_info($_) } $source->relationships;
  my @cascade = grep { $rels{$_}{attrs}{cascade_delete} } keys %rels;

  if (@cascade) {
    my $guard = $source->schema->txn_scope_guard;

    my $ret = $self->next::method(@rest);

    foreach my $rel (@cascade) {
      if( my $rel_rs = eval{ $self->search_related($rel) } ) {
        $rel_rs->delete_all;
      } else {
        carp "Skipping cascade delete on relationship '$rel' - related resultsource '$rels{$rel}{class}' is not registered with this schema";
        next;
      }
    }

    $guard->commit;
    return $ret;
  }

  $self->next::method(@rest);
}

sub update {
  my ($self, @rest) = @_;
  return $self->next::method(@rest) unless ref $self;
    # Because update cascades on a class *really* don't make sense!

  my $source = $self->result_source;
  my %rels = map { $_ => $source->relationship_info($_) } $source->relationships;
  my @cascade = grep { $rels{$_}{attrs}{cascade_update} } keys %rels;

  if (@cascade) {
    my $guard = $source->schema->txn_scope_guard;

    my $ret = $self->next::method(@rest);

    foreach my $rel (@cascade) {
      next if (
        $rels{$rel}{attrs}{accessor}
          &&
        $rels{$rel}{attrs}{accessor} eq 'single'
          &&
        !exists($self->{_relationship_data}{$rel})
      );
      $_->update for grep defined, $self->$rel;
    }

    $guard->commit;
    return $ret;
  }

  $self->next::method(@rest);
}

1;
