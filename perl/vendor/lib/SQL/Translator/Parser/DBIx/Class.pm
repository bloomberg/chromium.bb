package SQL::Translator::Parser::DBIx::Class;

# AUTHOR: Jess Robinson

# Some mistakes the fault of Matt S Trout

# Others the fault of Ash Berlin

use strict;
use warnings;
our ($DEBUG, $VERSION, @EXPORT_OK);
$VERSION = '1.10';
$DEBUG = 0 unless defined $DEBUG;

use Exporter;
use SQL::Translator::Utils qw(debug normalize_name);
use DBIx::Class::Carp qw/^SQL::Translator|^DBIx::Class|^Try::Tiny/;
use DBIx::Class::Exception;
use Scalar::Util 'blessed';
use Try::Tiny;
use namespace::clean;

use base qw(Exporter);

@EXPORT_OK = qw(parse);

# -------------------------------------------------------------------
# parse($tr, $data)
#
# setting parser_args => { add_fk_index => 0 } will prevent
# the auto-generation of an index for each FK.
#
# Note that $data, in the case of this parser, is not useful.
# We're working with DBIx::Class Schemas, not data streams.
# -------------------------------------------------------------------
sub parse {
    my ($tr, $data)   = @_;
    my $args          = $tr->parser_args;

    my $dbicschema = $data || $args->{dbic_schema};

    for (qw(DBIx::Class::Schema DBIx::Schema package)) {
      if (defined (my $s = delete $args->{$_} )) {
        carp_unique("Supplying a schema via  ... parser_args => { '$_' => \$schema } is deprecated. Please use parser_args => { dbic_schema => \$schema } instead");

        # move it from the deprecated to the proper $args slot
        unless ($dbicschema) {
          $args->{dbic_schema} = $dbicschema = $s;
        }
      }
    }

    DBIx::Class::Exception->throw('No DBIx::Class::Schema') unless ($dbicschema);

    if (!ref $dbicschema) {
      eval "require $dbicschema"
        or DBIx::Class::Exception->throw("Can't load $dbicschema: $@");
    }

    if (
      ref $args->{dbic_schema}
        and
      $args->{dbic_schema}->storage
    ) {
      # we have a storage-holding $schema instance in $args
      # we need to dissociate it from that $storage
      # otherwise SQLT insanity may ensue due to how some
      # serializing producers treat $args (crazy crazy shit)
      local $args->{dbic_schema}{storage};
      $args->{dbic_schema} = $args->{dbic_schema}->clone;
    }

    my $schema      = $tr->schema;
    my $table_no    = 0;

    $schema->name( ref($dbicschema) . " v" . ($dbicschema->schema_version || '1.x'))
      unless ($schema->name);

    my @monikers = sort $dbicschema->sources;
    if (my $limit_sources = $args->{'sources'}) {
        my $ref = ref $limit_sources || '';
        $dbicschema->throw_exception ("'sources' parameter must be an array or hash ref")
          unless( $ref eq 'ARRAY' || ref eq 'HASH' );

        # limit monikers to those specified in
        my $sources;
        if ($ref eq 'ARRAY') {
            $sources->{$_} = 1 for (@$limit_sources);
        } else {
            $sources = $limit_sources;
        }
        @monikers = grep { $sources->{$_} } @monikers;
    }


    my(%table_monikers, %view_monikers);
    for my $moniker (@monikers){
      my $source = $dbicschema->source($moniker);
       if ( $source->isa('DBIx::Class::ResultSource::Table') ) {
         $table_monikers{$moniker}++;
      } elsif( $source->isa('DBIx::Class::ResultSource::View') ){
          next if $source->is_virtual;
         $view_monikers{$moniker}++;
      }
    }

    my %tables;
    foreach my $moniker (sort keys %table_monikers)
    {
        my $source = $dbicschema->source($moniker);
        my $table_name = $source->name;

        # FIXME - this isn't the right way to do it, but sqlt does not
        # support quoting properly to be signaled about this
        $table_name = $$table_name if ref $table_name eq 'SCALAR';

        # It's possible to have multiple DBIC sources using the same table
        next if $tables{$table_name};

        $tables{$table_name}{source} = $source;
        my $table = $tables{$table_name}{object} = SQL::Translator::Schema::Table->new(
                                       name => $table_name,
                                       type => 'TABLE',
                                       );
        foreach my $col ($source->columns)
        {
            # assuming column_info in dbic is the same as DBI (?)
            # data_type is a number, column_type is text?
            my %colinfo = (
              name => $col,
              size => 0,
              is_auto_increment => 0,
              is_foreign_key => 0,
              is_nullable => 0,
              %{$source->column_info($col)}
            );
            if ($colinfo{is_nullable}) {
              $colinfo{default} = '' unless exists $colinfo{default};
            }
            my $f = $table->add_field(%colinfo)
              || $dbicschema->throw_exception ($table->error);
        }

        my @primary = $source->primary_columns;

        $table->primary_key(@primary) if @primary;

        my %unique_constraints = $source->unique_constraints;
        foreach my $uniq (sort keys %unique_constraints) {
            if (!$source->_compare_relationship_keys($unique_constraints{$uniq}, \@primary)) {
                $table->add_constraint(
                            type             => 'unique',
                            name             => $uniq,
                            fields           => $unique_constraints{$uniq}
                );
            }
        }

        my @rels = $source->relationships();

        my %created_FK_rels;

        # global add_fk_index set in parser_args
        my $add_fk_index = (exists $args->{add_fk_index} && ! $args->{add_fk_index}) ? 0 : 1;

        REL:
        foreach my $rel (sort @rels) {

            my $rel_info = $source->relationship_info($rel);

            # Ignore any rel cond that isn't a straight hash
            next unless ref $rel_info->{cond} eq 'HASH';

            my $relsource = try { $source->related_source($rel) };
            unless ($relsource) {
              carp "Ignoring relationship '$rel' on '$moniker' - related resultsource '$rel_info->{class}' is not registered with this schema\n";
              next;
            };

            # related sources might be excluded via a {sources} filter or might be views
            next unless exists $table_monikers{$relsource->source_name};

            my $rel_table = $relsource->name;

            # FIXME - this isn't the right way to do it, but sqlt does not
            # support quoting properly to be signaled about this
            $rel_table = $$rel_table if ref $rel_table eq 'SCALAR';

            # Force the order of @cond to match the order of ->add_columns
            my $idx;
            my %other_columns_idx = map {'foreign.'.$_ => ++$idx } $relsource->columns;

            for ( keys %{$rel_info->{cond}} ) {
              unless (exists $other_columns_idx{$_}) {
                carp "Ignoring relationship '$rel' on '$moniker' - related resultsource '@{[ $relsource->source_name ]}' does not contain one of the specified columns: '$_'\n";
                next REL;
              }
            }

            my @cond = sort { $other_columns_idx{$a} <=> $other_columns_idx{$b} } keys(%{$rel_info->{cond}});

            # Get the key information, mapping off the foreign/self markers
            my @refkeys = map {/^\w+\.(\w+)$/} @cond;
            my @keys = map {$rel_info->{cond}->{$_} =~ /^\w+\.(\w+)$/} @cond;

            # determine if this relationship is a self.fk => foreign.pk (i.e. belongs_to)
            my $fk_constraint;

            #first it can be specified explicitly
            if ( exists $rel_info->{attrs}{is_foreign_key_constraint} ) {
                $fk_constraint = $rel_info->{attrs}{is_foreign_key_constraint};
            }
            # it can not be multi
            elsif ( $rel_info->{attrs}{accessor}
                    && $rel_info->{attrs}{accessor} eq 'multi' ) {
                $fk_constraint = 0;
            }
            # if indeed single, check if all self.columns are our primary keys.
            # this is supposed to indicate a has_one/might_have...
            # where's the introspection!!?? :)
            else {
                $fk_constraint = not $source->_compare_relationship_keys(\@keys, \@primary);
            }

            my ($otherrelname, $otherrelationship) = %{ $source->reverse_relationship_info($rel) };

            my $cascade;
            for my $c (qw/delete update/) {
                if (exists $rel_info->{attrs}{"on_$c"}) {
                    if ($fk_constraint) {
                        $cascade->{$c} = $rel_info->{attrs}{"on_$c"};
                    }
                    elsif ( $rel_info->{attrs}{"on_$c"} ) {
                        carp "SQLT attribute 'on_$c' was supplied for relationship '$moniker/$rel', which does not appear to be a foreign constraint. "
                            . "If you are sure that SQLT must generate a constraint for this relationship, add 'is_foreign_key_constraint => 1' to the attributes.\n";
                    }
                }
                elsif (defined $otherrelationship and $otherrelationship->{attrs}{$c eq 'update' ? 'cascade_copy' : 'cascade_delete'}) {
                    $cascade->{$c} = 'CASCADE';
                }
            }

            if($rel_table) {
                # Constraints are added only if applicable
                next unless $fk_constraint;

                # Make sure we don't create the same foreign key constraint twice
                my $key_test = join("\x00", sort @keys);
                next if $created_FK_rels{$rel_table}->{$key_test};

                if (scalar(@keys)) {
                  $created_FK_rels{$rel_table}->{$key_test} = 1;

                  my $is_deferrable = $rel_info->{attrs}{is_deferrable};

                  # calculate dependencies: do not consider deferrable constraints and
                  # self-references for dependency calculations
                  if (! $is_deferrable and $rel_table ne $table_name) {
                    $tables{$table_name}{foreign_table_deps}{$rel_table}++;
                  }

                  # trim schema before generating constraint/index names
                  (my $table_abbrev = $table_name) =~ s/ ^ [^\.]+ \. //x;

                  $table->add_constraint(
                    type             => 'foreign_key',
                    name             => join('_', $table_abbrev, 'fk', @keys),
                    fields           => \@keys,
                    reference_fields => \@refkeys,
                    reference_table  => $rel_table,
                    on_delete        => uc ($cascade->{delete} || ''),
                    on_update        => uc ($cascade->{update} || ''),
                    (defined $is_deferrable ? ( deferrable => $is_deferrable ) : ()),
                  );

                  # global parser_args add_fk_index param can be overridden on the rel def
                  my $add_fk_index_rel = (exists $rel_info->{attrs}{add_fk_index}) ? $rel_info->{attrs}{add_fk_index} : $add_fk_index;

                  # Check that we do not create an index identical to the PK index
                  # (some RDBMS croak on this, and it generally doesn't make much sense)
                  # NOTE: we do not sort the key columns because the order of
                  # columns is important for indexes and two indexes with the
                  # same cols but different order are allowed and sometimes
                  # needed
                  next if join("\x00", @keys) eq join("\x00", @primary);

                  if ($add_fk_index_rel) {
                      (my $idx_name = $table_name) =~ s/ ^ [^\.]+ \. //x;
                      my $index = $table->add_index(
                          name   => join('_', $table_abbrev, 'idx', @keys),
                          fields => \@keys,
                          type   => 'NORMAL',
                      );
                  }
              }
            }
        }

    }

    # attach the tables to the schema in dependency order
    my $dependencies = {
      map { $_ => _resolve_deps ($_, \%tables) } (keys %tables)
    };

    for my $table (sort
      {
        keys %{$dependencies->{$a} || {} } <=> keys %{ $dependencies->{$b} || {} }
          ||
        $a cmp $b
      }
      (keys %tables)
    ) {
      $schema->add_table ($tables{$table}{object});
      $tables{$table}{source} -> _invoke_sqlt_deploy_hook( $tables{$table}{object} );

      # the hook might have already removed the table
      if ($schema->get_table($table) && $table =~ /^ \s* \( \s* SELECT \s+/ix) {
        carp <<'EOW';

Custom SQL through ->name(\'( SELECT ...') is DEPRECATED, for more details see
"Arbitrary SQL through a custom ResultSource" in DBIx::Class::Manual::Cookbook
or http://search.cpan.org/dist/DBIx-Class/lib/DBIx/Class/Manual/Cookbook.pod

EOW

        # remove the table as there is no way someone might want to
        # actually deploy this
        $schema->drop_table ($table);
      }
    }

    my %views;
    my @views = map { $dbicschema->source($_) } keys %view_monikers;

    my $view_dependencies = {
        map {
            $_ => _resolve_deps( $dbicschema->source($_), \%view_monikers )
          } ( keys %view_monikers )
    };

    my @view_sources =
      sort {
        keys %{ $view_dependencies->{ $a->source_name }   || {} } <=>
          keys %{ $view_dependencies->{ $b->source_name } || {} }
          || $a->source_name cmp $b->source_name
      }
      map { $dbicschema->source($_) }
      keys %view_monikers;

    foreach my $source (@view_sources)
    {
        my $view_name = $source->name;

        # FIXME - this isn't the right way to do it, but sqlt does not
        # support quoting properly to be signaled about this
        $view_name = $$view_name if ref $view_name eq 'SCALAR';

        # Skip custom query sources
        next if ref $view_name;

        # Its possible to have multiple DBIC source using same table
        next if $views{$view_name}++;

        $dbicschema->throw_exception ("view $view_name is missing a view_definition")
            unless $source->view_definition;

        my $view = $schema->add_view (
          name => $view_name,
          fields => [ $source->columns ],
          $source->view_definition ? ( 'sql' => $source->view_definition ) : ()
        ) || $dbicschema->throw_exception ($schema->error);

        $source->_invoke_sqlt_deploy_hook($view);
    }


    if ($dbicschema->can('sqlt_deploy_hook')) {
      $dbicschema->sqlt_deploy_hook($schema);
    }

    return 1;
}

#
# Quick and dirty dependency graph calculator
#
sub _resolve_deps {
    my ( $question, $answers, $seen ) = @_;
    my $ret = {};
    $seen ||= {};
    my @deps;

    # copy and bump all deps by one (so we can reconstruct the chain)
    my %seen = map { $_ => $seen->{$_} + 1 } ( keys %$seen );
    if ( blessed($question)
        && $question->isa('DBIx::Class::ResultSource::View') )
    {
        $seen{ $question->result_class } = 1;
        @deps = keys %{ $question->{deploy_depends_on} };
    }
    else {
        $seen{$question} = 1;
        @deps = keys %{ $answers->{$question}{foreign_table_deps} };
    }

    for my $dep (@deps) {
        if ( $seen->{$dep} ) {
            return {};
        }
        my $next_dep;

        if ( blessed($question)
            && $question->isa('DBIx::Class::ResultSource::View') )
        {
            no warnings 'uninitialized';
            my ($next_dep_source_name) =
              grep {
                $question->schema->source($_)->result_class eq $dep
                  && !( $question->schema->source($_)
                    ->isa('DBIx::Class::ResultSource::Table') )
              } @{ [ $question->schema->sources ] };
            return {} unless $next_dep_source_name;
            $next_dep = $question->schema->source($next_dep_source_name);
        }
        else {
            $next_dep = $dep;
        }
        my $subdeps = _resolve_deps( $next_dep, $answers, \%seen );
        $ret->{$_} += $subdeps->{$_} for ( keys %$subdeps );
        ++$ret->{$dep};
    }
    return $ret;
}

1;

=head1 NAME

SQL::Translator::Parser::DBIx::Class - Create a SQL::Translator schema
from a DBIx::Class::Schema instance

=head1 SYNOPSIS

 ## Via DBIx::Class
 use MyApp::Schema;
 my $schema = MyApp::Schema->connect("dbi:SQLite:something.db");
 $schema->create_ddl_dir();
 ## or
 $schema->deploy();

 ## Standalone
 use MyApp::Schema;
 use SQL::Translator;

 my $schema = MyApp::Schema->connect;
 my $trans  = SQL::Translator->new (
      parser      => 'SQL::Translator::Parser::DBIx::Class',
      parser_args => {
          dbic_schema => $schema,
          add_fk_index => 0,
          sources => [qw/
            Artist
            CD
          /],
      },
      producer    => 'SQLite',
     ) or die SQL::Translator->error;
 my $out = $trans->translate() or die $trans->error;

=head1 DESCRIPTION

This class requires L<SQL::Translator> installed to work.

C<SQL::Translator::Parser::DBIx::Class> reads a DBIx::Class schema,
interrogates the columns, and stuffs it all in an $sqlt_schema object.

Its primary use is in deploying database layouts described as a set
of L<DBIx::Class> classes, to a database. To do this, see
L<DBIx::Class::Schema/deploy>.

This can also be achieved by having DBIx::Class export the schema as a
set of SQL files ready for import into your database, or passed to
other machines that need to have your application installed but don't
have SQL::Translator installed. To do this see
L<DBIx::Class::Schema/create_ddl_dir>.

=head1 PARSER OPTIONS

=head2 dbic_schema

The DBIx::Class schema (either an instance or a class name) to be parsed.
This argument is in fact optional - instead one can supply it later at
translation time as an argument to L<SQL::Translator/translate>. In
other words both of the following invocations are valid and will produce
conceptually identical output:

  my $yaml = SQL::Translator->new(
    parser => 'SQL::Translator::Parser::DBIx::Class',
    parser_args => {
      dbic_schema => $schema,
    },
    producer => 'SQL::Translator::Producer::YAML',
  )->translate;

  my $yaml = SQL::Translator->new(
    parser => 'SQL::Translator::Parser::DBIx::Class',
    producer => 'SQL::Translator::Producer::YAML',
  )->translate(data => $schema);

=head2 add_fk_index

Create an index for each foreign key.
Enabled by default, as having indexed foreign key columns is normally the
sensible thing to do.

=head2 sources

=over 4

=item Arguments: \@class_names

=back

Limit the amount of parsed sources by supplying an explicit list of source names.

=head1 SEE ALSO

L<SQL::Translator>, L<DBIx::Class::Schema>

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
