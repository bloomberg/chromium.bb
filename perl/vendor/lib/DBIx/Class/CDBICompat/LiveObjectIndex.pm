package # hide from PAUSE
    DBIx::Class::CDBICompat::LiveObjectIndex;

use strict;
use warnings;

use Scalar::Util qw/weaken/;

use base qw/Class::Data::Inheritable/;

__PACKAGE__->mk_classdata('purge_object_index_every' => 1000);
__PACKAGE__->mk_classdata('live_object_index' => { });
__PACKAGE__->mk_classdata('live_object_init_count' => { });

# Caching is on by default, but a classic CDBI hack to turn it off is to
# set this variable false.
$Class::DBI::Weaken_Is_Available = 1
    unless defined $Class::DBI::Weaken_Is_Available;
__PACKAGE__->mk_classdata('__nocache' => 0);

sub nocache {
    my $class = shift;

    return $class->__nocache(@_) if @_;

    return 1 if $Class::DBI::Weaken_Is_Available == 0;
    return $class->__nocache;
}

# Ripped from Class::DBI 0.999, all credit due to Tony Bowden for this code,
# all blame due to me for whatever bugs I introduced porting it.

sub purge_dead_from_object_index {
  my $live = shift->live_object_index;
  delete @$live{ grep !defined $live->{$_}, keys %$live };
}

sub remove_from_object_index {
  my $self = shift;
  delete $self->live_object_index->{$self->ID};
}

sub clear_object_index {
  my $live = shift->live_object_index;
  delete @$live{ keys %$live };
}


# And now the fragments to tie it in to DBIx::Class::Table

sub insert {
  my ($self, @rest) = @_;
  $self->next::method(@rest);

  return $self if $self->nocache;

  # Because the insert will die() if it can't insert into the db (or should)
  # we can be sure the object *was* inserted if we got this far. In which
  # case, given primary keys are unique and ID only returns a
  # value if the object has all its primary keys, we can be sure there
  # isn't a real one in the object index already because such a record
  # cannot have existed without the insert failing.
  if (my $key = $self->ID) {
    my $live = $self->live_object_index;
    weaken($live->{$key} = $self);
    $self->purge_dead_from_object_index
      if ++$self->live_object_init_count->{count}
              % $self->purge_object_index_every == 0;
  }

  return $self;
}

sub inflate_result {
  my ($class, @rest) = @_;
  my $new = $class->next::method(@rest);

  return $new if $new->nocache;

  if (my $key = $new->ID) {
    #warn "Key $key";
    my $live = $class->live_object_index;
    return $live->{$key} if $live->{$key};
    weaken($live->{$key} = $new);
    $class->purge_dead_from_object_index
      if ++$class->live_object_init_count->{count}
              % $class->purge_object_index_every == 0;
  }
  return $new;
}

1;
