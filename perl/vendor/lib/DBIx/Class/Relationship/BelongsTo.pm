package # hide from PAUSE
    DBIx::Class::Relationship::BelongsTo;

# Documentation for these methods can be found in
# DBIx::Class::Relationship

use strict;
use warnings;
use Try::Tiny;
use namespace::clean;

our %_pod_inherit_config =
  (
   class_map => { 'DBIx::Class::Relationship::BelongsTo' => 'DBIx::Class::Relationship' }
  );

sub belongs_to {
  my ($class, $rel, $f_class, $cond, $attrs) = @_;

  # assume a foreign key constraint unless defined otherwise
  $attrs->{is_foreign_key_constraint} = 1
    if not exists $attrs->{is_foreign_key_constraint};
  $attrs->{undef_on_null_fk} = 1
    if not exists $attrs->{undef_on_null_fk};

  # no join condition or just a column name
  if (!ref $cond) {

    my ($f_key, $guess);
    if (defined $cond and length $cond) {
      $f_key = $cond;
      $guess = "caller specified foreign key '$f_key'";
    }
    else {
      $f_key = $rel;
      $guess = "using given relationship name '$rel' as foreign key column name";
    }

    $class->throw_exception(
      "No such column '$f_key' declared yet on ${class} ($guess)"
    )  unless $class->has_column($f_key);

    $class->ensure_class_loaded($f_class);
    my $f_rsrc = try {
      $f_class->result_source_instance;
    }
    catch {
      $class->throw_exception(
        "Foreign class '$f_class' does not seem to be a Result class "
      . "(or it simply did not load entirely due to a circular relation chain)"
      );
    };

    my $pri = $f_rsrc->_single_pri_col_or_die;

    $cond = { "foreign.${pri}" => "self.${f_key}" };

  }
  # explicit join condition
  else {
    if (ref $cond eq 'HASH') { # ARRAY is also valid
      my $cond_rel;
      # FIXME This loop is ridiculously incomplete and dangerous
      # staving off changes until implmentation of the swindon consensus
      for (keys %$cond) {
        if (m/\./) { # Explicit join condition
          $cond_rel = $cond;
          last;
        }
        $cond_rel->{"foreign.$_"} = "self.".$cond->{$_};
      }
      $cond = $cond_rel;
    }
  }

  my $acc_type = (
    ref $cond eq 'HASH'
      and
    keys %$cond == 1
      and
    (keys %$cond)[0] =~ /^foreign\./
      and
    $class->has_column($rel)
  ) ? 'filter' : 'single';

  my $fk_columns = ($acc_type eq 'single' and ref $cond eq 'HASH')
    ? { map { $_ =~ /^self\.(.+)/ ? ( $1 => 1 ) : () } (values %$cond ) }
    : undef
  ;

  $class->add_relationship($rel, $f_class,
    $cond,
    {
      is_depends_on => 1,
      accessor => $acc_type,
      $fk_columns ? ( fk_columns => $fk_columns ) : (),
      %{$attrs || {}}
    }
  );

  return 1;
}

1;
