package # hide from PAUSE
    DBIx::Class::Relationship::HasOne;

use strict;
use warnings;
use DBIx::Class::Carp;
use Try::Tiny;
use namespace::clean;

our %_pod_inherit_config =
  (
   class_map => { 'DBIx::Class::Relationship::HasOne' => 'DBIx::Class::Relationship' }
  );

sub might_have {
  shift->_has_one('LEFT' => @_);
}

sub has_one {
  shift->_has_one(undef() => @_);
}

sub _has_one {
  my ($class, $join_type, $rel, $f_class, $cond, $attrs) = @_;
  unless (ref $cond) {
    my $pri = $class->result_source_instance->_single_pri_col_or_die;

    my ($f_key,$guess,$f_rsrc);
    if (defined $cond && length $cond) {
      $f_key = $cond;
      $guess = "caller specified foreign key '$f_key'";
    }
    else {
      # at this point we need to load the foreigner, expensive or not
      $class->ensure_class_loaded($f_class);

      $f_rsrc = try {
        my $r = $f_class->result_source_instance;
        die "There got to be some columns by now... (exception caught and rewritten by catch below)"
          unless $r->columns;
        $r;
      }
      catch {
        $class->throw_exception(
          "Foreign class '$f_class' does not seem to be a Result class "
        . "(or it simply did not load entirely due to a circular relation chain)"
        );
      };

      if ($f_rsrc->has_column($rel)) {
        $f_key = $rel;
        $guess = "using given relationship name '$rel' as foreign key column name";
      }
      else {
        $f_key = $f_rsrc->_single_pri_col_or_die;
        $guess = "using primary key of foreign class for foreign key";
      }
    }

# FIXME - this check needs to be moved to schema-composition time...
#    # only perform checks if the far side was not preloaded above *AND*
#    # appears to have been loaded by something else (has a rsrc_instance)
#    if (! $f_rsrc and $f_rsrc = try { $f_class->result_source_instance }) {
#      $class->throw_exception(
#        "No such column '$f_key' on foreign class ${f_class} ($guess)"
#      ) if !$f_rsrc->has_column($f_key);
#    }

    $cond = { "foreign.${f_key}" => "self.${pri}" };
  }
  $class->_validate_has_one_condition($cond);

  my $default_cascade = ref $cond eq 'CODE' ? 0 : 1;

  $class->add_relationship($rel, $f_class,
   $cond,
   { accessor => 'single',
     cascade_update => $default_cascade,
     cascade_delete => $default_cascade,
     is_depends_on => 0,
     ($join_type ? ('join_type' => $join_type) : ()),
     %{$attrs || {}} });
  1;
}

sub _validate_has_one_condition {
  my ($class, $cond )  = @_;

  return if $ENV{DBIC_DONT_VALIDATE_RELS};
  return unless 'HASH' eq ref $cond;
  foreach my $foreign_id ( keys %$cond ) {
    my $self_id = $cond->{$foreign_id};

    # we can ignore a bad $self_id because add_relationship handles this
    # exception
    return unless $self_id =~ /^self\.(.*)$/;

    my $key = $1;
    $class->throw_exception("Defining rel on ${class} that includes '$key' but no such column defined here yet")
        unless $class->has_column($key);
    my $column_info = $class->column_info($key);
    if ( $column_info->{is_nullable} ) {
      carp(qq'"might_have/has_one" must not be on columns with is_nullable set to true ($class/$key). This might indicate an incorrect use of those relationship helpers instead of belongs_to.');
    }
  }
}

1;
