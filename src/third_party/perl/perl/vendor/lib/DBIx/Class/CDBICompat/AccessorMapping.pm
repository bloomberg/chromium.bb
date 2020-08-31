package # hide from PAUSE Indexer
  DBIx::Class::CDBICompat::AccessorMapping;

use strict;
use warnings;

use Scalar::Util 'blessed';
use namespace::clean;

sub mk_group_accessors {
    my ($class, $group, @cols) = @_;

    foreach my $col (@cols) {
        my($accessor, $col) = ref $col eq 'ARRAY' ? @$col : (undef, $col);

        my($ro_meth, $wo_meth);
        if (defined blessed $col and $col->isa('Class::DBI::Column')) {
            $ro_meth = $col->accessor;
            $wo_meth = $col->mutator;
        }
        elsif (defined $accessor and ($accessor ne $col)) {
            $ro_meth = $wo_meth = $accessor;
        }
        else {
            $ro_meth = $class->accessor_name_for($col);
            $wo_meth = $class->mutator_name_for($col);
        }

        # warn "class: $class / col: $col / ro: $ro_meth / wo: $wo_meth\n";
        if ($ro_meth eq $wo_meth or # they're the same
            $wo_meth eq $col)     # or only the accessor is custom
        {
            $class->next::method($group => [ $ro_meth => $col ]);
        }
        else {
            $class->mk_group_ro_accessors($group => [ $ro_meth => $col ]);
            $class->mk_group_wo_accessors($group => [ $wo_meth => $col ]);
        }
    }
}


sub accessor_name_for {
    my ($class, $column) = @_;
    if ($class->can('accessor_name')) {
        return $class->accessor_name($column)
    }

    return $column;
}

sub mutator_name_for {
    my ($class, $column) = @_;
    if ($class->can('mutator_name')) {
        return $class->mutator_name($column)
    }

    return $column;
}


sub new {
    my ($class, $attrs, @rest) = @_;
    $class->throw_exception( "create needs a hashref" ) unless ref $attrs eq 'HASH';
    foreach my $col ($class->columns) {
        my $acc = $class->accessor_name_for($col);
        $attrs->{$col} = delete $attrs->{$acc} if exists $attrs->{$acc};

        my $mut = $class->mutator_name_for($col);
        $attrs->{$col} = delete $attrs->{$mut} if exists $attrs->{$mut};
    }
    return $class->next::method($attrs, @rest);
}

1;
