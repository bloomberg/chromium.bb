package SQL::Statement;

#########################################################################
#
# This module is copyright (c), 2001,2005 by Jeff Zucker.
# This module is copyright (c), 2007-2017 by Jens Rehsack.
# All rights reserved.
#
# It may be freely distributed under the same terms as Perl itself.
#
# See below for help (search for SYNOPSIS)
#########################################################################

use strict;
use warnings FATAL => "all";

use 5.008;
use vars qw($VERSION $DEBUG);

use SQL::Parser                 ();
use SQL::Eval                   ();
use SQL::Statement::RAM         ();
use SQL::Statement::TermFactory ();
use SQL::Statement::Util        ();

use Carp qw(carp croak);
use Clone qw(clone);
use Errno;
use Scalar::Util qw(blessed looks_like_number);
use List::Util qw(first);
use Params::Util qw(_INSTANCE _STRING _ARRAY _ARRAY0 _HASH0 _HASH);

#use locale;

$VERSION = '1.412';

sub new
{
    my ( $class, $sql, $flags ) = @_;

    # IF USER DEFINED extend_csv IN SCRIPT
    # USE THE ANYDATA DIALECT RATHER THAN THE CSV DIALECT
    # WITH DBD::CSV

    if (   ( defined($main::extend_csv) && $main::extend_csv )
        || ( defined($main::extend_sql) && $main::extend_sql ) )
    {
        $flags = SQL::Parser->new('AnyData');
    }
    my $parser = $flags;
    my $self = bless( {}, $class );
    $flags->{PrintError}    = 1 unless defined $flags->{PrintError};
    $flags->{text_numbers}  = 1 unless defined $flags->{text_numbers};
    $flags->{alpha_compare} = 1 unless defined $flags->{alpha_compare};

    unless ( blessed($flags) )    # avoid copying stale data from earlier parsing sessions
    {
        %$self = ( %$self, %{ clone($flags) } );
    }
    else
    {
        $self->{$_} = $flags->{$_} for qw(RaiseError PrintError opts);
    }

    $self->{dlm} = '~';

    # Dean Arnold improvement to allow better subclassing
    # if (!ref($parser) or (ref($parser) and ref($parser) !~ /^SQL::Parser/)) {
    unless ( _INSTANCE( $parser, 'SQL::Parser' ) )
    {
        my $parser_dialect = $flags->{dialect} || 'AnyData';
        $parser_dialect = 'AnyData' if ( $parser_dialect =~ m/^(?:CSV|Excel)$/ );

        $parser = SQL::Parser->new( $parser_dialect, $flags );
    }
    $self->{termFactory}  = SQL::Statement::TermFactory->new($self);
    $self->{capabilities} = {};
    $self->prepare( $sql, $parser );
    return $self;
}

sub prepare
{
    my ( $self, $sql, $parser ) = @_;

    $self->{already_prepared}->{$sql} and return;

    # delete earlier preparations, they're overwritten after this prepare run
    $self->{already_prepared} = {};
    my $rv = $parser->parse($sql);
    if ($rv)
    {
        undef $self->{errstr};
        my $parser_struct = clone( $parser->{struct} );
        while ( my ( $k, $v ) = each( %{$parser_struct} ) )
        {
            $self->{$k} = $v;
        }
        undef $self->{where_terms};    # force rebuild when needed
        undef $self->{columns};
        undef $self->{splitted_all_cols};
        $self->{argnum} = 0;

        my $values    = $self->{values};
        my $param_num = -1;
        if ( $self->{limit_clause} )
        {
            $self->{limit_clause} = SQL::Statement::Limit->new( $self->{limit_clause} );
        }

        if ( defined( $self->{num_placeholders} ) )
        {
            for my $i ( 0 .. $self->{num_placeholders} - 1 )
            {
                $self->{params}->[$i] = SQL::Statement::Param->new($i);
            }
        }

        $self->{tables} = [ map { SQL::Statement::Table->new($_) } @{ $self->{table_names} } ];

        if ( $self->{where_clause} && !defined( $self->{where_terms} ) )
        {
            $self->{where_terms} = $self->{termFactory}->buildCondition( $self->{where_clause} );
            #if ( $self->{where_clause}->{combiners} )
            #{
            #    $self->{has_OR} = 1
            #      if ( first { -1 != index( $_, 'OR' ) } @{ $self->{where_clause}->{combiners} } );
            #}
        }

        ++$self->{already_prepared}->{$sql};
        return $self;
    }
    else
    {
        $self->{errstr} = $parser->errstr;
        ++$self->{already_prepared}->{$sql};
        return;
    }
}

sub execute
{
    my ( $self, $data, $params ) = @_;
    ( $self->{NUM_OF_ROWS}, $self->{NUM_OF_FIELDS}, $self->{data} ) = ( 0, 0, [] ) and return 'OEO'
      if ( $self->{no_execute} );
    $self->{procedure}->{data} = $data if ( $self->{procedure} );
    $self->{params} = $params;

    my ($command) = $self->command();
    return $self->do_err('No command found!') unless ($command);

    $self->{where_clause}
      and !defined( $self->{where_terms} )
      and $self->{where_terms} = $self->{termFactory}->buildCondition( $self->{where_clause} );

    ( $self->{NUM_OF_ROWS}, $self->{NUM_OF_FIELDS}, $self->{data} ) = $self->$command( $data, $params );

    $self->{NAME} =
      _ARRAY0( $self->{columns} ) ? [ map { delete $_->{term}->{fastpath}; $_->display_name() } @{ $self->{columns} } ] : [];

    # Force closing the tables
    $self->{tables} = [ map { SQL::Statement::Table->new($_->{name}) } @{ delete $self->{tables} } ];    # create keen defs

    undef $self->{where_terms};                                              # force rebuild when needed

    return unless ( defined( $self->{NUM_OF_ROWS} ) );
    return $self->{NUM_OF_ROWS} || '0E0';
}

sub CREATE ($$$)
{
    my ( $self, $data, $params ) = @_;
    my $names;

    # CREATE TABLE AS ...
    my $subquery = $self->{subquery};
    if ($subquery)
    {
        my $sth;

        # AS IMPORT
        if ( $subquery =~ m/^IMPORT/i )
        {
            $sth = $data->{Database}->prepare("SELECT * FROM $subquery")
              or return $self->do_err( $data->{Database}->errstr() );
            $sth->execute(@$params) or return $self->do_err( $sth->errstr() );
            $names = $sth->{NAME};
        }

        # AS SELECT
        else
        {
            $sth = $data->{Database}->prepare($subquery)
              or return $self->do_err( $data->{Database}->errstr() );
            $sth->execute() or return $self->do_err( $sth->errstr() );
            $names = $sth->{NAME};
        }
        $names = $sth->{NAME} unless defined $names;
        my $tbl_data = $sth->{sql_stmt}->{data};
        my $tbl_name = $self->{org_table_names}->[0] || $self->tables(0)->name;

        # my @tbl_cols = map {$_->name} $sth->{sql_stmt}->columns;
        #my @tbl_cols=map{$_->name} $sth->{sql_stmt}->columns if $sth->{sql_stmt};
        my @tbl_cols;

        #            @tbl_cols=@{ $sth->{NAME} } unless @tbl_cols;
        @tbl_cols = @{$names} unless (@tbl_cols);
        my $create_sql = "CREATE TABLE $tbl_name";
        $create_sql = "CREATE TEMP TABLE $tbl_name" if ( $self->{is_ram_table} );
        my @coldefs = map { "'$_' TEXT" } @tbl_cols;
        $create_sql .= '(' . join( ',', @coldefs ) . ')';
        $data->{Database}->do($create_sql) or die "Can't do <$create_sql>: " . $data->{Database}->errstr;
        my $colstr     = ('?,') x @tbl_cols;
        my $insert_sql = "INSERT INTO $tbl_name VALUES($colstr)";
        my $local_sth  = $data->{Database}->prepare($insert_sql);
        $local_sth->execute(@$_) for @$tbl_data;
        return ( 0, 0 );
    }
    my ( $eval, $foo ) = $self->open_tables( $data, 1, 1 );
    return unless ($eval);
    $eval->params($params);
    my ( $row, $table, $col ) = ( [], $eval->table( $self->tables(0)->name() ) );
    if ( _ARRAY( $table->col_names() ) )
    {
        return $self->do_err( "Table '" . $self->tables(0)->name() . "' already exists." );
    }
    foreach $col ( $self->columns() )
    {
        push( @{$row}, $col->name() );
    }
    $table->push_names( $data, $row );

    return ( 0, 0 );
}

sub CALL
{
    my ( $self, $data, $params ) = @_;

    # my $dbh = $data->{Database};
    # $self->{procedure}->{data} = $data;

    my $procTerm = $self->{termFactory}->buildCondition( $self->{procedure} );

    ( $self->{NUM_OF_ROWS}, $self->{NUM_OF_FIELDS}, $self->{data} ) = $procTerm->value($data);
}

my $enoentstr = "Cannot open .*\(" . Errno::ENOENT . "\)";
my $eabstrstr = "Abstract method .*::open_table called";
my $notblrx   = qr/(?:$enoentstr|$eabstrstr)/;

sub DROP ($$$)
{
    my ( $self, $data, $params ) = @_;
    my $eval;
    my @err;
    eval {
        local $SIG{__WARN__} = sub { push @err, @_ };
        ($eval) = $self->open_tables( $data, 0, 1 );
    };
    if (    $self->{ignore_missing_table}
        and ( $@ or @err or $self->{errstr} )
        and grep { $_ =~ $notblrx } ( @err, $@, $self->{errstr} ) )
    {
        return ( -1, 0 );
    }

    return if $self->{errstr};
    return $self->do_err( $@ || $err[0] ) if ( $@ || @err );

    #    return undef unless $eval;
    return ( -1, 0 ) unless $eval;

    #    $eval->params($params);
    my ($table) = $eval->table( $self->tables(0)->name() );
    $table->drop($data);

    #use mylibs; zwarn $self->{sql_stmt};
    return ( -1, 0 );
}

sub INSERT ($$$)
{
    my ( $self, $data, $params ) = @_;

    my ( $eval, $all_cols ) = $self->open_tables( $data, 0, 1 );
    return unless ($eval);

    $params and $eval->params($params);
    $self->verify_columns( $data, $eval, $all_cols ) if ( scalar( $self->columns() ) );
    return if ( $self->{errstr} );

    my ($table) = $eval->table( $self->tables(0)->name() );
    $table->seek( $data, 0, 2 ) unless ( $table->capability('insert_new_row') );

    my ( $val, $col, $i, $k );
    my ($cNum) = scalar( $self->columns() );
    my $param_num = 0;

    $cNum
      or return $self->do_err("Bad col names in INSERT");

    my $maxCol = $#$all_cols;

    # INSERT INTO $table (row, ...) VALUES (value, ...), (...)
    for ( $k = 0; $k < scalar( @{ $self->{values} } ); ++$k )
    {
        my ($array) = [];
        for ( $i = 0; $i < $cNum; $i++ )
        {
            $col = $self->columns($i);
            $val = $self->row_values( $k, $i );
            if ( defined( _INSTANCE( $val, 'SQL::Statement::Param' ) ) )
            {
                $val = $eval->param( $val->idx() );
            }
            elsif ( defined( _INSTANCE( $val, 'SQL::Statement::Term' ) ) )
            {
                $val = $val->value($eval);
            }
            elsif ( $val and $val->{type} eq 'placeholder' )
            {
                $val = $eval->param( $param_num++ );
            }
            elsif ( defined( _HASH($val) ) )
            {
                $val = $self->{termFactory}->buildCondition($val);
                $val = $val->value($eval);
            }
            else
            {
                return $self->do_err('Internal error: Unexpected column type');
            }
            $array->[ $table->column_num( $col->name() ) ] = $val;
        }

        # Extend row to put values in ALL fields
        $#$array < $maxCol and $array->[$maxCol] = undef;

        $table->capability('insert_new_row')
          ? $table->insert_new_row( $data, $array )
          : $table->push_row( $data, $array );
    }

    return ( $k, 0 );
}

sub DELETE ($$$)
{
    my ( $self, $data, $params ) = @_;
    my ( $eval, $all_cols ) = $self->open_tables( $data, 0, 1 );
    return unless $eval;
    $eval->params($params);
    $self->verify_columns( $data, $eval, $all_cols );
    return if ( $self->{errstr} );
    my $tname    = $self->tables(0)->name();
    my ($table)  = $eval->table($tname);
    my $affected = 0;
    my ( @rows, $array );

    while ( $array = $table->fetch_row($data) )
    {
        if ( $self->eval_where( $eval, $tname, $array ) )
        {
            ++$affected;
            if ( $table->capability('rowwise_delete') and $table->capability('inplace_delete') )
            {
                if ( $table->capability('delete_one_row') )
                {
                    $table->delete_one_row( $data, $array );
                }
                elsif ( $table->capability('delete_current_row') )
                {
                    $table->delete_current_row( $data, $array );
                }
            }
            elsif ( $table->capability('rowwise_delete') )
            {
                push( @rows, $array );
            }

            next;
        }

        push( @rows, $array ) unless ( $table->capability('rowwise_delete') );
    }

    if ($affected)
    {
        if ( $table->capability('rowwise_delete') )
        {    # @rows is empty in case of inplace_delete capability
            foreach my $array (@rows)
            {
                $table->delete_one_row( $data, $array );
            }
        }
        else
        {
            # rewrite table without deleted elements
            $table->seek( $data, 0, 0 );
            foreach my $array (@rows)
            {
                $table->push_row( $data, $array );
            }
            $table->truncate($data);
        }
    }

    return ( $affected, 0 );
}

sub UPDATE ($$$)
{
    my ( $self, $data, $params ) = @_;

    my ( $eval, $all_cols ) = $self->open_tables( $data, 0, 1 );
    return unless $eval;

    my $valnum = $self->{num_val_placeholders};
    my @val_params = splice( @{$params}, 0, $valnum ) if ($valnum);
    $self->{params} ||= $params;
    $eval->params($params);
    $self->verify_columns( $data, $eval, $all_cols );
    return if ( $self->{errstr} );

    my $tname    = $self->tables(0)->name();
    my ($table)  = $eval->table($tname);
    my $affected = 0;
    my @rows;

    while ( my $array = $table->fetch_row($data) )
    {
        my $originalValues;
        if ( $self->eval_where( $eval, $tname, $array ) )
        {
            my $valpos = 0;
            if ( $table->capability('update_specific_row') )
            {
                $originalValues = clone($array);
            }

            for ( my $i = 0; $i < $self->columns(); $i++ )
            {
                my $val = $self->row_values( 0, $i );
                if ( defined( _INSTANCE( $val, 'SQL::Statement::Param' ) ) )
                {
                    $val = $val_params[ $valpos++ ];
                }
                elsif ( defined( _INSTANCE( $val, 'SQL::Statement::Term' ) ) )
                {
                    $val = $val->value($eval);
                }
                elsif ( $val and $val->{type} eq 'placeholder' )
                {
                    $val = $val_params[ $valpos++ ];
                }
                elsif ( defined( _HASH($val) ) )
                {
                    $val = $self->{termFactory}->buildCondition($val);
                    $val = $val->value($eval);
                }
                else
                {
                    return $self->do_err('Internal error: Unexpected column type');
                }

                my $col = $self->columns($i);
                $array->[ $table->column_num( $col->name() ) ] = $val;
            }

            ++$affected;
            if ( $table->capability('rowwise_update') and $table->capability('inplace_update') )
            {
                # Martin Fabiani <martin@fabiani.net>:
                # the following block is the most important enhancement to SQL::Statement::UPDATE
                if ( $table->capability('update_specific_row') )
                {
                    $table->update_specific_row( $data, $array, $originalValues );
                }
                elsif ( $table->capability('update_one_row') )
                {
                    # NOTE: this prevents from updating index keys
                    $table->update_one_row( $data, $array );
                }
                elsif ( $table->capability('update_current_row') )
                {
                    $table->update_current_row( $data, $array );
                }
            }
            elsif ( $table->capability('rowwise_update') )
            {
                push( @rows, $array ) unless ( $table->capability('update_specific_row') );
                push( @rows, [ $array, $originalValues ] )
                  if ( $table->capability('update_specific_row') );
            }
        }

        push( @rows, $array ) unless ( $table->capability('rowwise_update') );
    }

    if ($affected)
    {
        if ( $table->capability('rowwise_update') )
        {    # @rows is empty in case of inplace_update capability
            foreach my $array (@rows)
            {
                if ( $table->capability('update_specific_row') )
                {
                    $table->update_specific_row( $data, $array->[0], $array->[1] );
                }
                elsif ( $table->capability('update_one_row') )
                {
                    $table->update_one_row( $data, $array );
                }
            }
        }
        else
        {
            # rewrite table with updated elements
            $table->seek( $data, 0, 0 );
            foreach my $array (@rows)
            {
                $table->push_row( $data, $array );
            }
            $table->truncate($data);
        }
    }

    return ( $affected, 0 );
}

sub find_join_columns
{
    my ( $self, @all_cols ) = @_;
    my $display_combine = 'NAMED';
    $display_combine = 'NATURAL' if ( -1 != index( $self->{join}->{type},   'NATURAL' ) );
    $display_combine = 'USING'   if ( -1 != index( $self->{join}->{clause}, 'USING' ) );
    my @display_cols;
    my @keycols = ();
    @keycols = @{ $self->{join}->{keycols} }
      if $self->{join}->{keycols};
    foreach (@keycols) { $_ =~ s/\./$self->{dlm}/ }
    my %is_key_col;
    %is_key_col = map { $_ => 1 } @keycols;

    # IF NAMED COLUMNS, USE NAMED COLUMNS
    #
    if ( $display_combine eq 'NAMED' )
    {
        @display_cols = $self->columns();

        #
        #	DAA
        #	need to get to $self's table objects to get the name
        #
        #        @display_cols = map {$_->table . $self->{dlm} . $_->name} @display_cols;
        #        @display_cols = map {$_->table->{NAME} . $self->{dlm} . $_->name} @display_cols;

        my @tbls   = $self->tables();
        my %tables = ();

        $tables{ $_->name() } = $_ foreach (@tbls);

        foreach ( 0 .. $#display_cols )
        {
            $display_cols[$_] = (
                  $display_cols[$_]->table()
                ? $tables{ $display_cols[$_]->table() }->name()
                : ''
              )
              . $self->{dlm}
              . $display_cols[$_]->name();
        }
    }

    # IF ASTERISKED COLUMNS AND NOT NATURAL OR USING
    # USE ALL COLUMNS, IN ORDER OF NAMING OF TABLES
    #
    elsif ( $display_combine eq 'NONE' )
    {
        @display_cols = @all_cols;
    }

    # IF NATURAL, COMBINE ALL SHARED COLUMNS
    # IF USING, COMBINE ALL KEY COLUMNS
    #
    else
    {
        my %is_natural;
        for my $full_col (@all_cols)
        {
            my ( $table, $col ) = $full_col =~ m/^([^$self->{dlm}]+)$self->{dlm}(.+)$/;
            next if ( ( $display_combine eq 'NATURAL' ) and $is_natural{$col} );
            next if ( ( $display_combine eq 'USING' ) && $is_natural{$col} && $is_key_col{$col} );
            push( @display_cols, $full_col );
            $is_natural{$col}++;
        }
    }
    my @shared = ();
    my %is_shared;
    if ( $self->{join}->{type} =~ m/NATURAL/ )
    {
        for my $full_col (@all_cols)
        {
            my ( $table, $col ) = $full_col =~ m/^([^$self->{dlm}]+)$self->{dlm}(.+)$/;
            push( @shared, $col ) if ( $is_shared{$col}++ );    # using side-effect of post-inc
        }
    }
    else
    {
        @shared = @keycols;
    }
    $self->{join}->{shared_cols}  = \@shared;
    $self->{join}->{display_cols} = \@display_cols;
}

sub JOIN
{
    my ( $self, $data, $params ) = @_;

    my ( $eval, $all_cols ) = $self->open_tables( $data, 0, 0 );
    return undef unless $eval;
    $eval->params($params);
    $self->verify_columns( $data, $eval, $all_cols );
    return if ( $self->{errstr} );
    if (    $self->{join}->{keycols}
        and $self->{join}->{table_order}
        and ( scalar( @{ $self->{join}->{table_order} } ) == 0 ) )
    {
        $self->{join}->{table_order} = $self->order_joins( $self->{join}->{keycols} );
        $self->{join}->{table_order} = $self->{table_names}
          unless ( defined( $self->{join}->{table_order} ) );
    }
    my @tables = $self->tables;

    # GET THE LIST OF QUALIFIED COLUMN NAMES FOR DISPLAY
    # *IN ORDER BY NAMING OF TABLES*
    #
    my @all_cols;
    for my $table (@tables)
    {
        my @cols = @{ $eval->table( $table->{name} )->col_names };
        for my $col (@cols)
        {
            push( @all_cols, $table->{name} . $self->{dlm} . $col );
        }
    }
    $self->find_join_columns(@all_cols);

    # JOIN THE TABLES
    # *IN ORDER *BY JOINS*
    #
    @tables = @{ $self->{join}->{table_order} } if ( $self->{join}->{table_order} );
    my ( $tableA, $tableB ) = splice( @tables, 0, 2 );
    $tableA = $tableA->{name} if ( ref($tableA) );
    $tableB = $tableB->{name} if ( ref($tableB) );
    my ( $tableAobj, $tableBobj ) = ( $eval->table($tableA), $eval->table($tableB) );
    $tableAobj->{NAME} ||= $tableA;
    $tableBobj->{NAME} ||= $tableB;
    $self->join_2_tables( $data, $params, $tableAobj, $tableBobj );

    for my $next_table (@tables)
    {
        $tableAobj = $self->{join}->{table};
        $tableBobj = $eval->table($next_table);
        $tableBobj->{NAME} ||= $next_table;
        $self->join_2_tables( $data, $params, $tableAobj, $tableBobj );
        $self->{cur_table} = $next_table;
    }
    return $self->SELECT( $data, $params );
}

sub join_2_tables
{
    my ( $self, $data, $params, $tableAobj, $tableBobj ) = @_;
    my $share_type = 'IMPLICIT';
    $share_type = 'NATURAL' if ( -1 != index( $self->{join}->{type},   'NATURAL' ) );
    $share_type = 'USING'   if ( -1 != index( $self->{join}->{clause}, 'USING' ) );
    $share_type = 'ON'      if ( -1 != index( $self->{join}->{clause}, 'ON' ) );
    $share_type = 'USING'
      if ( ( $share_type eq 'ON' ) && ( scalar( @{ $self->{join}->{keycols} } ) == 1 ) );
    my $join_type = 'INNER';
    $join_type = 'LEFT'  if ( -1 != index( $self->{join}->{type}, 'LEFT' ) );
    $join_type = 'RIGHT' if ( -1 != index( $self->{join}->{type}, 'RIGHT' ) );
    $join_type = 'FULL'  if ( -1 != index( $self->{join}->{type}, 'FULL' ) );

    my $right_join = $join_type eq 'RIGHT';
    if ($right_join)
    {
        my $tmpTbl = $tableAobj;
        $tableAobj = $tableBobj;
        $tableBobj = $tmpTbl;
    }

    my $tableA = $tableAobj->{NAME};
    ( 0 != index( $tableA, '"' ) ) and $tableA = lc $tableA;
    my $tableB = $tableBobj->{NAME};
    ( 0 != index( $tableB, '"' ) ) and $tableB = lc $tableB;
    my @colsA = @{ $tableAobj->col_names() };
    my @colsB = @{ $tableBobj->col_names() };
    my ( %isunqualA, %isunqualB, @shared_cols );
    $isunqualB{ $colsB[$_] } = 1 for ( 0 .. $#colsB );
    my @tmpshared = @{ $self->{join}->{shared_cols} };

    if ( $share_type eq 'ON' )
    {
        $right_join and @tmpshared = reverse @tmpshared;
    }
    elsif ( $share_type eq 'USING' )
    {
        foreach my $c (@tmpshared)
        {
            substr( $c, 0, index( $c, $self->{dlm} ) + 1 ) = '';
            push( @shared_cols, $tableA . $self->{dlm} . $c );
            push( @shared_cols, $tableB . $self->{dlm} . $c );
        }
    }
    elsif ( $share_type eq 'NATURAL' )
    {
        for my $c (@colsA)
        {
            if ( $tableA eq $self->{dlm} . 'tmp' )
            {
                substr( $c, 0, index( $c, $self->{dlm} ) + 1 ) = '';
            }
            if ( $isunqualB{$c} )
            {
                push( @shared_cols, $tableA . $self->{dlm} . $c );
                push( @shared_cols, $tableB . $self->{dlm} . $c );
            }
        }
    }

    my %whichqual;
    if ( $share_type eq 'ON' || $share_type eq 'IMPLICIT' )
    {
        foreach my $colb (@colsB)
        {
            $colb = $whichqual{$colb} = $tableB . $self->{dlm} . $colb;
        }
    }
    else
    {
        @colsB = map { $tableB . $self->{dlm} . $_ } @colsB;
    }

    my @all_cols = map { $tableA . $self->{dlm} . $_ } @colsA;
    @all_cols = $right_join ? ( @colsB, @all_cols ) : ( @all_cols, @colsB );
    {
        my $str = $self->{dlm} . "tmp" . $self->{dlm};
        foreach (@all_cols)
        {
            my $pos = index( $_, $str );
            $pos >= 0 and substr( $_, $pos, length($str) ) = '';
        }
    }
    if ( $tableA eq $self->{dlm} . 'tmp' )
    {
        foreach my $colA (@colsA)
        {
            my $c = substr( $colA, index( $colA, $self->{dlm} ) + 1 );
            $isunqualA{$c} = $colA;
        }
        #%isunqualA =
        #  map { my ($c) = $_ =~ m/^(?:[^$self->{dlm}]+)$self->{dlm}(.+)$/; $c => $_ } @colsA;
    }
    else
    {
        foreach my $cola (@colsA)
        {
            $cola = $isunqualA{$cola} = $tableA . $self->{dlm} . $cola;
        }
    }

    my ( %col_numsA, %col_numsB );
    $col_numsA{ $colsA[$_] } = $_ for ( 0 .. $#colsA );
    $col_numsB{ $colsB[$_] } = $_ for ( 0 .. $#colsB );

    if ( $share_type eq 'ON' || $share_type eq 'IMPLICIT' )
    {
        %whichqual = ( %whichqual, %isunqualA );

        while (@tmpshared)
        {
            my ( $k1, $k2 ) = splice( @tmpshared, 0, 2 );

            # if both keys are in one table, bail out - FIXME: errmsg?
            next if ( $isunqualA{$k1} && $isunqualA{$k2} );
            next if ( $isunqualB{$k1} && $isunqualB{$k2} );

            defined( $whichqual{$k1} ) and $k1 = $whichqual{$k1};
            defined( $whichqual{$k2} ) and $k2 = $whichqual{$k2};

            if ( defined( $col_numsA{$k1} ) && defined( $col_numsB{$k2} ) )
            {
                push( @shared_cols, $k1, $k2 );
            }
            elsif ( defined( $col_numsA{$k2} ) && defined( $col_numsB{$k1} ) )
            {
                push( @shared_cols, $k2, $k1 );
            }
        }
    }

    my %is_shared;
    for my $c (@shared_cols)
    {
        $is_shared{$c} = 1;
        defined( $col_numsA{$c} )
          or defined( $col_numsB{$c} )
          or return $self->do_err("Can't find shared columns!");
    }
    my ( $posA, $posB ) = ( [], [] );
    for my $f (@shared_cols)
    {
        defined( $col_numsA{$f} ) and push( @{$posA}, $col_numsA{$f} );
        defined( $col_numsB{$f} ) and push( @{$posB}, $col_numsB{$f} );
    }

    my $is_inner_join = $join_type eq "INNER";
    #use mylibs; zwarn $self->{join};
    # CYCLE THROUGH TABLE B, CREATING A HASH OF ITS VALUES
    #
    my $hashB = {};
  TBLBFETCH: while ( my $array = $tableBobj->fetch_row($data) )
    {
        my @key_vals = @$array[@$posB];
        if ($is_inner_join)
        {
            defined($_) or next TBLBFETCH for (@key_vals);
        }
        my $hashkey = join( ' ', @key_vals );
        push( @{ $hashB->{$hashkey} }, $array );
    }

    # CYCLE THROUGH TABLE A
    #
    my $blankRow;
    my $joined_table = [];
    my %visited;
  TBLAFETCH: while ( my $arrayA = $tableAobj->fetch_row($data) )    # use tbl1st & tbl2nd
    {
        my @key_vals = @$arrayA[@$posA];
        if ($is_inner_join)
        {
            defined($_) or next TBLAFETCH for (@key_vals);
        }
        my $hashkey = join( ' ', @key_vals );
        my $rowsB = $hashB->{$hashkey};
        if ( !defined($rowsB) && ( $join_type ne 'INNER' ) )
        {
            defined($blankRow) or $blankRow = [ (undef) x scalar(@colsB) ];
            $rowsB = [$blankRow];
        }

        if ( $join_type ne 'UNION' )
        {
            for my $arrayB ( @{$rowsB} )
            {
                my $newRow = $right_join ? [ @{$arrayB}, @{$arrayA} ] : [ @{$arrayA}, @{$arrayB} ];

                push( @$joined_table, $newRow );
            }
        }

        ++$visited{$hashkey};
    }

    # ADD THE LEFTOVER B ROWS IF NEEDED
    #
    if ( $join_type eq 'FULL' || $join_type eq 'UNION' )
    {
        my $st_is_NaturalOrUsing = ( -1 != index( $self->{join}->{type}, 'NATURAL' ) )
          || ( -1 != index( $self->{join}->{clause}, 'USING' ) );
        while ( my ( $k, $v ) = each %{$hashB} )
        {
            next if ( $visited{$k} );
            for my $rowB (@$v)
            {
                my ( @arrayA, @tmpB, $rowhash );
                @{$rowhash}{@colsB} = @{$rowB};
                for my $c (@all_cols)
                {
                    my ( $table, $col ) = split( $self->{dlm}, $c, 2 );
                    push( @arrayA, undef )          if ( $table eq $tableA );
                    push( @tmpB,   $rowhash->{$c} ) if ( $table eq $tableB );
                }
                @arrayA[@$posA] = @tmpB[@$posB] if ($st_is_NaturalOrUsing);
                my $newRow = [ @arrayA, @tmpB ];
                push( @{$joined_table}, $newRow );
            }
        }
    }

    undef $hashB;
    undef $tableAobj;
    undef $tableBobj;

    $self->{join}->{table} =
      SQL::Statement::TempTable->new( $self->{dlm} . 'tmp', \@all_cols, $self->{join}->{display_cols}, $joined_table );

    return;
}

sub run_functions
{
    my ( $self, $data, $params ) = @_;
    my ( $eval, $all_cols ) = $self->open_tables( $data, 0, 0 );
    my @row = ();
    for my $col ( $self->columns() )
    {
        my $val = $col->value($eval);    # FIXME approve
        push( @row, $val );
    }
    return ( 1, scalar @row, [ \@row ] );
}

sub SELECT($$)
{
    my ( $self, $data, $params ) = @_;

    $self->{params} ||= $params;
    defined( _ARRAY( $self->{table_names} ) ) or return $self->run_functions( $data, $params );

    my ( $eval, $all_cols, $tableName, $table );
    if ( defined( $self->{join} ) )
    {
        defined $self->{join}->{table} or return $self->JOIN( $data, $params );
        $tableName = $self->{dlm} . 'tmp';
        $table     = $self->{join}->{table};
    }
    else
    {
        ( $eval, $all_cols ) = $self->open_tables( $data, 0, 0 );
        return unless $eval;
        $eval->params($params);
        $self->verify_columns( $data, $eval, $all_cols );
        return if ( $self->{errstr} );
        $tableName = $self->tables(0)->name();
        $table     = $eval->table($tableName);
    }

    my $rows = [];

    # In a loop, build the list of columns to retrieve; this will be
    # used both for fetching data and ordering.
    my ( $cList, $col, $tbl, $ar, $i, $c );
    my $numFields = 0;
    my %columns;
    my @names;
    my %funcs = ();

    #
    #	DAA
    #
    #	lets just disable this and see where it leads...
    #
    #    if ($self->{join}) {
    #          @names = @{ $table->col_names };
    #          for my $col(@names) {
    #             $columns{$tableName}->{"$col"} = $numFields++;
    #             push(@$cList, $table->column_num($col));
    #          }
    #    }
    #    else {
    foreach my $column ( $self->columns() )
    {
        if ( _INSTANCE( $column, 'SQL::Statement::Param' ) )
        {
            my $val = $eval->param( $column->idx() );
            if ( -1 != ( my $idx = index( $val, '.' ) ) )
            {
                $col = substr( $val, 0, $idx );
                $tbl = substr( $val, $idx + 1 );
            }
            else
            {
                $col = $val;
                $tbl = $tableName;
            }
            $tbl ||= '';
            $columns{$tbl}->{$col} = $numFields++;
        }
        else
        {
            ( $col, $tbl ) = ( $column->name(), $column->table() );
            $tbl ||= '';
            $columns{$tbl}->{ $column->display_name() } = $columns{$tbl}->{$col} = $numFields++;
        }

        #
        # handle functions in select list
        #
        #	DAA
        #
        #	check for a join temp table; if so, check if we can locate
        #	the column in its delimited set
        #
        my $cnum =
          ( ( $tableName eq ( $self->{dlm} . 'tmp' ) ) && ( $tbl ne '' ) )
          ? $table->column_num( $tbl . $self->{dlm} . $col )
          : $table->column_num($col);

        if ( !defined $cnum || $column->{function} )
        {
            $funcs{$col} = $column->{function};
            $cnum = $col;
        }
        push( @$cList, $cnum );

        # push(@$cList, $table->column_num($col));
        push( @names, $col );
    }

    #    }
    $cList = [] unless ( defined($cList) );
    if ( $self->{join} )
    {
        foreach (@names) { $_ =~ s/^[^$self->{dlm}]+$self->{dlm}//; }
    }
    $self->{NAME} = \@names;
    # $self->verify_order_cols($table);
    my @order_by      = $self->order();
    my @extraSortCols = ();

    if (@order_by)
    {
        my $nFields = $numFields;

        # It is possible that the user gave an ORDER BY clause with columns
        # that are not part of $cList yet. These columns will need to be
        # present in the array of arrays for sorting, but will be stripped
        # off later.
        my $i = -1;
        foreach my $column (@order_by)
        {
            ++$i;
            ( $col, $tbl ) = ( $column->column(), $column->table() );
            my $pos;
            $tbl ||= $self->colname2table($col);
            $tbl ||= '';
            if ( $self->{join} )
            {
                $pos = $table->column_num( $tbl . $self->{dlm} . $col );
                defined($pos)
                  or $pos = $table->column_num( $tbl . '_' . $col );
            }
            next if ( exists( $columns{$tbl}->{$col} ) );
            $pos = $table->column_num($col) unless ( defined($pos) );
            push( @extraSortCols, $pos );
            $columns{$tbl}->{$col} = $nFields++;
        }
    }

    my $e = $self->{join} ? $table : $eval;

    # begin count for limiting if there's a limit clause and no order clause
    #
    my $limit_count = 0 if ( $self->limit() and !$self->order() );
    my $limit       = $self->limit();
    my $row_count   = 0;
    my $offset      = $self->offset() || 0;
    while ( my $array = $table->fetch_row($data) )
    {
        if ( $self->eval_where( $e, $tableName, $array, \%funcs ) )
        {
            next if ( defined($limit_count) and ( $row_count++ < $offset ) );

            my @row = map { $_->value($e) } $self->columns();
            push( @{$rows}, \@row );

            # We quit here if there's a limit clause without order clause
            # and the limit has been reached
            defined($limit_count)
              and ( ++$limit_count >= $limit )
              and return ( $limit, $numFields, $rows );
        }
    }

    if ( $self->distinct() )
    {
        my %seen;
        @{$rows} = map {
            $seen{ join( "\0", ( map { defined($_) ? $_ : '' } @{$_} ) ) }++
              ? ()
              : $_
        } @{$rows};
    }

    if ( $self->{has_set_functions} )
    {
        my $aggreg;
        if ( $self->{group_by} )
        {
            my @keycols = @{ $self->{colpos} }{ @{ $self->{group_by} } };
            $aggreg = SQL::Statement::Group->new( $self, $rows, \@keycols );
        }
        else
        {
            $aggreg = SQL::Statement::Aggregate->new( $self, $rows );
        }
        $rows = $aggreg->calc();
        # FIXME re-order if order_by
    }

    if (@order_by)
    {
        use sort 'stable';
        my @sortCols = map {
            my ( $col, $tbl ) = ( $_->column(), $_->table() );
            $self->{join} and $table->is_shared($col) and $tbl = 'shared';
            $tbl ||= $self->colname2table($col) || '';
            ( $columns{$tbl}->{$col}, $_->desc() )
        } @order_by;

        $i = scalar(@sortCols);
        do
        {
            my $desc   = $sortCols[ --$i ];
            my $colNum = $sortCols[ --$i ];
            @{$rows} = sort {
                my $result;
                $result = _anycmp( $a->[$colNum], $b->[$colNum] );
                $desc and $result = -$result;
                $result;
            } @{$rows};
        } while ( $i > 0 );
        use sort 'defaults';    # for perl < 5.10.0
    }

    if ( defined( $self->limit() ) )
    {
        my $offset = $self->offset() || 0;
        my $limit  = $self->limit()  || 0;
        @{$rows} = splice( @{$rows}, $offset, $limit );
    }

    # Rip off columns that have been added for @extraSortCols only
    if (@extraSortCols)
    {
        foreach my $row ( @{$rows} )
        {
            splice( @{$row}, $numFields, scalar(@extraSortCols) );
        }
    }

    ( scalar( @{$rows} ), $numFields, $rows );
}

sub _anycmp($$;$)
{
    my ( $a, $b, $case_fold ) = @_;

    if ( !defined($a) || !defined($b) )
    {
        return defined($a) - defined($b);
    }
    elsif ( looks_like_number($a) && looks_like_number($b) )
    {
        return $a <=> $b;
    }
    else
    {
        return $case_fold ? lc($a) cmp lc($b) || $a cmp $b : $a cmp $b;
    }
}

sub eval_where
{
    my ( $self, $eval, $tname, $rowary ) = @_;
    return 1 unless ( defined( $self->{where_terms} ) );
    $self->{argnum} = 0;

    return $self->{where_terms}->value($eval);
}

sub fetch_row
{
    my ($self) = @_;
    $self->{data} ||= [];
    my $row = shift @{ $self->{data} };
    return unless $row and scalar @$row;
    return $row;
}

no warnings 'once';
*fetch = \&fetch_row;

use warnings;

sub fetch_rows
{
    my $self = $_[0];
    my $rows = $self->{data} || [];
    $self->{data} = [];
    return $rows;
}

sub open_table ($$$$$) { croak "Abstract method " . ref( $_[0] ) . "::open_table called" }

sub open_tables
{
    my ( $self, $data, $createMode, $lockMode ) = @_;
    my @c;
    my $t      = {};
    my @tables = $self->tables();
    my $count  = -1;
    for my $tbl (@tables)
    {
        ++$count;
        my $name = $tbl->name();
        if ( $name =~ m/^(.+)\.([^\.]+)$/ )
        {
            my $schema = $1;    # ignored
            $name = $tbl->{name} = $2;
        }

        if ( defined( $self->{table_func} ) && defined( $self->{table_func}->{ uc $name } ) )
        {
            my $u_func = $self->{table_func}->{ uc $name };
            $t->{$name} = $self->get_user_func_table( $name, $u_func );
        }
        elsif (defined( $data->{Database}->{sql_ram_tables} )
            && defined( $data->{Database}->{sql_ram_tables}->{$name} )
            && $data->{Database}->{sql_ram_tables}->{$name} )
        {
            $t->{$name} = $data->{Database}->{sql_ram_tables}->{$name};
            $t->{$name}->seek( $data, 0, 0 );
            $t->{$name}->init_table( $data, $name, $createMode, $lockMode )
              if ( $t->{$name}->can('init_table') );
        }
        elsif ( $self->{is_ram_table} )
        {
            $t->{$name} = $data->{Database}->{sql_ram_tables}->{$name} =
              SQL::Statement::RAM::Table->new( $name, [], [] );
        }
        else
        {
            undef $@;
            eval {
                my $open_name = $self->{org_table_names}->[$count];
                $t->{$name} = $self->open_table( $data, $open_name, $createMode, $lockMode );
            };
            my $err = $t->{$name}->{errstr};
            return $self->do_err($err) if ($err);
            return $self->do_err($@)   if ($@);
        }

        my @cnames;
        my $table_cols = $t->{$name}->{org_col_names};
        $table_cols = $t->{$name}->{col_names} unless $table_cols;
        for my $c (@$table_cols)
        {
            my $newc = $c =~ m/^"/ ? $c : lc($c);
            push( @cnames, $newc );
            $self->{ORG_NAME}->{$newc} = $c;
        }

        #
        # set the col_num => col_obj hash for the table
        #
        my $col_nums;
        my $i = 0;
        for (@cnames)
        {
            $col_nums->{$_} = $i++;
        }
        $t->{$name}->{col_nums}  = $col_nums;
        $t->{$name}->{col_names} = \@cnames;

        my $tcols = $t->{$name}->col_names();
        my @newcols;
        for (@$tcols)
        {
            next unless ( defined($_) );
            my $ncol = $_;
            $ncol = $name . '.' . $ncol unless ( $ncol =~ m/\./ );
            push( @newcols, $ncol );
        }
        @c = ( @c, @newcols );
    }

    $self->buildColumnObjects( $t, \@tables );
    return $self->do_err( $self->{errstr} ) if ( $self->{errstr} );

    ##################################################
    # Patch from Cosimo Streppone <cosimoATcpan.org>

    # my $all_cols = $self->{all_cols}
    #             || [ map {$_->{name} }@{$self->{columns}} ]
    #             || [];
    # @$all_cols = (@$all_cols,@c);
    # $self->{all_cols} = $all_cols;
    if ( !$self->{all_cols} )
    {
        my $all_cols = [];
        $all_cols = [ map { $_->{name} } @{ $self->{columns} } ];
        $all_cols ||= [];    # ?
        @$all_cols = ( @$all_cols, @c );
        $self->{all_cols} = $all_cols;
    }
    ##################################################

    return SQL::Eval->new( { 'tables' => $t } ), \@c;
}

sub getColumnObject($)
{
    my ( $self, $newcol, $t, $tables ) = @_;
    my @columns;

    if ( ( $newcol->{type} eq 'column' ) && ( -1 != index( $newcol->{value}, '*' ) ) )
    {
        my $tbl;
        my @tables;
        if ( $newcol->{value} =~ m/^(.+)\.\*$/ )
        {
            $tbl = $1;
            return $self->do_err("No table name given in '$newcol->{value}'")
              unless ( defined( _STRING($tbl) ) );
            @tables = ($tbl);
        }
        else
        {
            @tables = map { $_->name() } @{$tables};
        }

        my $join = defined( _HASH( $self->{join} ) )
          && ( ( -1 != index( $self->{join}->{type}, 'NATURAL' ) )
            || ( -1 != index( $self->{join}->{clause}, 'USING' ) ) );
        my %shared_cols;

        foreach my $table (@tables)
        {
            return $self->do_err("Can't find table '$table'") unless ( defined( $t->{$table} ) );
            my $tcols = $t->{$table}->{col_names};
            return $self->do_err("Couldn't find column names for table '$table'!")
              unless ( _ARRAY($tcols) );
            foreach my $colName ( @{$tcols} )
            {
                next if ( $join && $shared_cols{$colName}++ );
                my $expcol = [
                    $colName,    # column name
                    $table,      # table name
                    SQL::Statement::ColumnValue->new( $self, $table . '.' . $colName ),    # term
                    $colName,                                                              # display name
                    $colName,
                    $newcol,
                ];
                push( @columns, $expcol );
            }
        }
    }
    elsif ( ( 'CREATE' eq $self->command() ) || ( 'DROP' eq $self->command() ) )
    {
        return $self->do_err("Invalid column type '$newcol->{type}'")
          unless ( 'column' eq $newcol->{type} );
        my $expcol = [
            $newcol->{value},    # column name
            undef,               # table name
            undef,               # term
            $newcol->{value},    # display name
            $newcol->{value},    # original name
            $newcol,             # coldef
        ];
        push( @columns, $expcol );
    }
    else
    {
        my $col;
        if ( $newcol->{type} eq 'setfunc' )
        {
            my @cols = $self->getColumnObject( $newcol->{arg}, $t );
            if ( 1 == scalar(@cols) )
            {
                $col = $cols[0]->[2];
            }
            else
            {
                # FIXME add '\0' constants between items?
                my $colSep = $self->{termFactory}->buildCondition(
                    {
                        type  => 'string',
                        value => "\0",
                    }
                );
                @cols = map { $_->[2], $colSep } @cols;
                pop(@cols);
                $col = $self->{termFactory}->buildCondition(
                    {
                        type  => 'function',
                        name  => 'str_concat',
                        value => \@cols,
                    }
                );
            }
        }
        else
        {
            $col = $self->{termFactory}->buildCondition($newcol);
        }

        my $expcol = [
            $newcol->{name} || $newcol->{value},    # column name
            undef,                                  # table name
            $col,                                   # term
            $newcol->{alias} || $newcol->{fullorg}, # display name
            $newcol->{fullorg},                     # original name
            $newcol,                                # coldef
        ];
        push( @columns, $expcol );
    }

    return @columns;
}

sub buildColumnObjects($)
{
    my ( $self, $t, $tables ) = @_;

    defined( _ARRAY0( $self->{column_defs} ) ) or return;
    defined( _ARRAY0( $self->{columns} ) ) and return;

    $self->{columns} = [];

    my $coldefs = $self->{column_defs};

    for ( my $i = 0; $i < scalar( @{$coldefs} ); ++$i )
    {
        my $colentry = $coldefs->[$i];

        my @columns = $self->getColumnObject( $colentry, $t, $tables );
        return if ( $self->{errstr} );

        foreach my $col (@columns)
        {
            my $expcol = SQL::Statement::Util::Column->new( @{$col} );
            push( @{ $self->{columns} }, $expcol );
            $self->{column_aliases}->{ $col->[4] } ||= $col->[3];
            $self->{colpos}->{ $col->[3] } = scalar( @{ $self->{columns} } ) - 1;
        }
    }

    return;
}

sub verify_expand_column
{
    my ( $self, $c, $i, $usr_cols, $is_duplicate, $col_exists ) = @_;

    # XXX
    defined $self->{ALIASES}->{$c} and $c = $self->{ALIASES}->{$c};

    my ( $table, $col, $col_obj );
    if ( $c =~ m/(\S+)\.(\S+)/ )
    {
        $table = $1;
        $col   = $2;
    }
    elsif ( ++${$i} >= 0 )
    {
        $col_obj = $usr_cols->[ ${$i} ];
        ( $table, $col ) = ( $col_obj->{table}, $col_obj->{name} );
    }
    else
    {
        ( $table, $col ) = $self->full_qualified_column_name($c);
    }
    return unless ($col);

    my $is_column =
      ( defined( _INSTANCE( $col_obj, 'SQL::Statement::Util::Column' ) ) and ( $col_obj->{coldef}->{type} eq 'column' ) )
      ? 1
      : 0;

    unless ( $is_column and defined($table) )
    {
        ( $table, undef ) = $self->full_qualified_column_name($col);
    }

    if ( defined( _INSTANCE( $table, 'SQL::Statement::Table' ) ) )
    {
        $table = $table->name();
    }

    if ( $is_column and !$table )
    {
        return $self->do_err("Ambiguous column name '$c'") if ( $is_duplicate->{$c} );
        return $self->do_err("No such column '$col'");
        $col = $c;
    }
    elsif ($is_column)
    {
        my $is_user_def = 1 if ( $self->{opts}->{function_defs}->{$col} );
        return $self->do_err("No such column '$table.$col'")
          unless ( $col_exists->{"$table.$col"}
            or $col_exists->{ "\L$table." . $col }
            or $is_user_def );
    }

    return ( $table, $col ) if ( $is_column or ${$i} < 0 );
    return;
}

sub verify_columns
{
    my ( $self, $data, $eval, $all_cols ) = @_;

    #
    # NOTE FOR LATER:
    # perhaps cache column names and skip this after first table open
    #
    $all_cols ||= [];
    my @tmp_cols = @{$all_cols};
    my @usr_cols = $self->columns();
    return $self->do_err('No fetchable columns') if ( 0 == scalar(@usr_cols) );

    my ( $cnum, $fully_qualified_cols ) = ( 0, [] );
    my @tmpcols = map { $_->{name} } @usr_cols;
    my %col_exists = map { $_ => 1 } @tmp_cols;

    my ( %is_member, @duplicates, %is_duplicate );
    # $_ =~ s/[^.]*\.(.*)/$1/;
    foreach (@$all_cols)
    {
        substr( $_, 0, index( $_, '.' ) + 1 ) = '';
    }    # XXX we're modifying $all_cols from caller!
    @duplicates = grep( $is_member{$_}++, @$all_cols );
    %is_duplicate = map { $_ => 1 } @duplicates;
    if ( exists( $self->{join} ) && defined( _HASH( $self->{join} ) ) )
    {
        my $join = $self->{join};
        if ( -1 != index( uc $join->{type}, 'NATURAL' ) )
        {
            %is_duplicate = ();
        }

        # the following should be probably conditioned on an option,
        # but I don't know which --BW
        elsif ( 'USING' eq $join->{clause} )
        {
            my @keys = @{ $join->{keycols} };
            delete @is_duplicate{@keys};
        }
    }

    my %set_func_nofunc;
    if ( defined( $self->{has_set_functions} ) )
    {
        my @set_func_nofunc = grep { ( $_->{type} ne 'setfunc' ) } @{ $self->{column_defs} };
        %set_func_nofunc = map { ( $_->{alias} || $_->{fullorg} ) => 1 } @set_func_nofunc;
    }
    my ( $is_fully, $set_fully ) = ( {}, {} );
    my $i          = -1;
    my $num_tables = $self->tables();
    for my $c (@tmpcols)
    {
        my ( $table, $col ) = $self->verify_expand_column( $c, \$i, \@usr_cols, \%is_duplicate, \%col_exists );
        return if ( $self->{errstr} );
        next unless ( $table && $col );

        my $ftc = "$table.$col";
        next if ( $table and $col and $is_fully->{$ftc} );

        $self->{columns}->[$i]->{name}  = $col;
        $self->{columns}->[$i]->{table} = $table;

        if ( $table and $col )
        {
            push( @$fully_qualified_cols, $ftc );
            ++$is_fully->{$ftc};
            ++$set_fully->{$ftc} if ( $set_func_nofunc{$c} );
        }
    }

    if ( defined( $self->{has_set_functions} ) )
    {
        if ( defined( _ARRAY( $self->{group_by} ) ) )
        {
            foreach my $grpby ( @{ $self->{group_by} } )
            {
                $i = -2;
                my ( $table, $col ) = $self->verify_expand_column( $grpby, \$i, \@usr_cols, \%is_duplicate, \%col_exists );
                return if ( $self->{errstr} );
                $col ||= $grpby;
                ( $table, $col ) = $self->full_qualified_column_name($col)
                  if ( defined($col) && !defined($table) );
                next unless ( defined($table) && defined($col) );
                delete $set_fully->{"$table.$col"};
            }
        }

        if ( defined( _HASH($set_fully) ) )
        {
            return $self->do_err(
                sprintf(
                    "Column%s '%s' must appear in the GROUP BY clause or be used in an aggregate function",
                    scalar( keys( %{$set_fully} ) ) > 1 ? 's' : '',
                    join( "', '", keys( %{$set_fully} ) )
                )
            );
        }
    }

    if ( $self->{sort_spec_list} )
    {
        for my $n ( 0 .. scalar @{ $self->{sort_spec_list} } - 1 )
        {
            defined( _INSTANCE( $self->{sort_spec_list}->[$n], 'SQL::Statement::Order' ) ) and next;
            my ( $newcol, $direction ) = each %{ $self->{sort_spec_list}->[$n] };
            my $desc = $direction && ( $direction eq "DESC" );    # ($direction || "ASC") eq "DESC";

            # XXX parse order by like group by and select list
            $i = -2;
            my ( $table, $col ) = $self->verify_expand_column( $newcol, \$i, \@usr_cols, \%is_duplicate, \%col_exists );
            $self->{errstr} and return;
            ( $table, $col ) = $self->full_qualified_column_name($newcol)
              if ( defined($col) && !defined($table) );
            defined($table) and $col = $table . "." . $col;
            $self->{sort_spec_list}->[$n] = SQL::Statement::Order->new(
                col => SQL::Statement::Util::Column->new(
                    $col,                                         # column name
                    $table,                                       # table name
                    SQL::Statement::ColumnValue->new( $self, $col ),    # term
                    $newcol                                             # display name
                ),
                direction => $direction,
                desc      => $desc,
            );
        }
    }

    return $fully_qualified_cols;
}

sub distinct()
{
    my $q = _STRING( $_[0]->{set_quantifier} );
    return defined($q) && ( 'DISTINCT' eq $q );
}

sub column_names()
{
    my @cols = map { $_->name() } $_[0]->columns();
    return @cols;
}

sub command() { return $_[0]->{command} }

sub params(;$)
{
    if ( !$_[0]->{params} )
    {
        return wantarray ? () : 0;
    }
    return $_[0]->{params}->[ $_[1] ] if ( defined $_[1] );

    return wantarray ? @{ $_[0]->{params} } : scalar @{ $_[0]->{params} };
}

sub row_values(;$$)
{
    unless ( defined( _ARRAY( $_[0]->{values} ) ) )
    {
        return wantarray ? () : 0;
    }
    if ( defined( $_[1] ) )
    {
        return 0 unless ( defined( $_[0]->{values}->[ $_[1] ] ) );
        return $_[0]->{values}->[ $_[1] ]->[ $_[2] ] if ( defined $_[2] );

        return wantarray
          ? map { $_->{value} } @{ $_[0]->{values}->[ $_[1] ] }
          : scalar @{ $_[0]->{values}->[ $_[1] ] };
    }
    else
    {
        return wantarray
          ? map {
            [ map { $_->{value} } @{$_} ]
          } @{ $_[0]->{values} }
          : scalar( @{ $_[0]->{values} } );
    }
}

#
# $num_of_cols = $stmt->columns()       # number of columns
# @cols        = $stmt->columns()       # array of S::S::Column objects
# $col         = $stmt->columns($cnum)  # S::S::Column obj for col number $cnum
# $col         = $stmt->columns($cname) # S::S::Column obj for col named $cname
#
sub columns
{
    my ( $self, $col ) = @_;
    if ( !$self->{columns} )
    {
        return wantarray ? () : 0;
    }

    if ( defined $col and $col =~ m/^\d+$/ )
    {    # arg1 = a number
        return $self->{columns}->[$col];
    }
    elsif ( defined $col )
    {    # arg1 = string
        for my $c ( @{ $self->{columns} } )
        {
            return $c if ( $c->name() eq $col );
        }
    }

    return wantarray ? @{ $self->{columns} } : scalar @{ $self->{columns} };
}

sub colname2colnum
{
    if ( !$_[0]->{columns} ) { return undef; }
    for my $i ( 0 .. $#{ $_[0]->{columns} } )
    {
        return $i if ( $_[0]->{columns}->[$i]->name() eq $_[1] );
    }
    return undef;
}

sub colname2table($)
{
    my ( $self, $col_name ) = @_;
    return undef unless defined $col_name;

    my ( $tbl, $col );
    if ( $col_name =~ /^(.+)\.(.+)$/ )
    {
        ( $tbl, $col ) = ( $1, $2 );
    }
    else
    {
        $col = $col_name;
    }

    my $found_table;
    for my $full_col ( @{ $self->{all_cols} } )
    {
        my ( $stbl, $scol ) = $full_col =~ /^(.+)\.(.+)$/;
        next unless ( $scol || '' ) eq $col;
        next if ( defined($tbl) && ( $tbl ne $stbl ) );
        $found_table = $stbl;
        last;
    }
    return $found_table;
}

sub full_qualified_column_name($)
{
    my ( $self, $col_name ) = @_;
    return unless ( defined($col_name) );

    # XXX
    defined $self->{ALIASES}->{$col_name} and $col_name = $self->{ALIASES}->{$col_name};

    my ( $tbl, $col );
    unless ( ( $tbl, $col ) = $col_name =~ m/^((?:"[^"]+")|(?:[^.]+))\.(.*)$/ )
    {
        $col = $col_name;
    }

    unless ( defined( $self->{splitted_all_cols} ) )
    {
        my @rc;
        for my $full_col ( @{ $self->{all_cols} } )
        {
            if ( my ( $stbl, $scol ) = $full_col =~ m/^((?:"[^"]+")|(?:[^.]+))\.(.*)$/ )
            {
                push( @{ $self->{splitted_all_cols} }, [ $stbl, $scol ] );
                defined($tbl) and ( $tbl ne $stbl ) and next;
                ( $scol eq $col ) and @rc = ( $stbl, $scol );
            }
        }
        @rc and return @rc;
    }
    else
    {
        for my $splitted_col ( @{ $self->{splitted_all_cols} } )
        {
            defined($tbl) and ( $tbl ne $splitted_col->[0] ) and next;
            ( $splitted_col->[1] eq $col ) and return @$splitted_col;
        }
    }

    return ( $tbl, $col );
}

#sub verify_order_cols
#{
#    my ( $self, $table ) = @_;
#    return unless $self->{sort_spec_list};
#    my @ocols = $self->order();
#    my @tcols = @{ $table->col_names() };
#    my @n_ocols;
#
#    for my $colnum ( 0 .. $#ocols )
#    {
#        my $col = $self->order($colnum);
#
#        if ( !defined( $col->table() ) )
#        {
#            my $cname = $ocols[$colnum]->{col}->name();
#            my $tname = $self->colname2table($cname);
#            return $self->do_err("No such column '$cname'.") unless ($tname);
#            $self->{sort_spec_list}->[$colnum]->{col}->{table} = $tname;
#            push( @n_ocols, $tname );
#        }
#    }
#
#    return 1;
#}

sub limit ($)  { $_[0]->{limit_clause}->{limit}; }
sub offset ($) { $_[0]->{limit_clause}->{offset}; }

sub order
{
    return unless ( defined $_[0]->{sort_spec_list} );

    return
        defined( $_[1] ) && looks_like_number( $_[1] ) ? $_[0]->{sort_spec_list}->[ $_[1] ]
      : wantarray ? @{ $_[0]->{sort_spec_list} }
      :             scalar @{ $_[0]->{sort_spec_list} };
}

sub tables
{
    return
        defined( $_[1] ) && looks_like_number( $_[1] ) ? $_[0]->{tables}->[ $_[1] ]
      : wantarray ? @{ $_[0]->{tables} }
      :             scalar @{ $_[0]->{tables} };
}

sub order_joins
{
    my ( $self, $links ) = @_;
    my ( @new_keycols, @new_links );
    for (@$links)
    {
        my ( $tbl, $col ) = $self->full_qualified_column_name($_);
        push( @new_keycols, $tbl . $self->{dlm} . $col );
        push( @new_links,   $tbl );
    }
    $self->{join}->{keycols} = $links = \@new_keycols;
    # my @tmp = @new_keycols;
    # foreach (@tmp) { $_ =~ s/\./$self->{dlm}/g; }
    # $self->{join}->{keycols} = \@tmp;
    # @$links = \@new_keycols;
    my @all_tables;
    my %relations;
    my %is_table;

    while (@new_links)
    {
        my $t1 = shift(@new_links);
        my $t2 = shift(@new_links);
        return undef unless ( defined($t1) and defined($t2) );
        push @all_tables, $t1 unless ( $is_table{$t1}++ );
        push @all_tables, $t2 unless ( $is_table{$t2}++ );
        $relations{$t1}{$t2}++;
        $relations{$t2}{$t1}++;
    }
    my @tables     = @all_tables;
    my @order      = shift @tables;
    my %is_ordered = ( $order[0] => 1 );
    my %visited;
    while (@tables)
    {
        my $t    = shift @tables;
        my @rels = keys %{ $relations{$t} };
        for my $t2 (@rels)
        {
            next unless $is_ordered{$t2};
            push @order, $t;
            $is_ordered{$t}++;
            last;
        }
        if ( !$is_ordered{$t} )
        {
            push( @tables, $t ) if ( $visited{$t}++ < @all_tables );
        }
    }
    if ( @order < @all_tables )
    {
        my @missing;
        my %in_order = map { $_ => 1 } @order;
        foreach my $tbl (@all_tables)
        {
            next if ( $in_order{$tbl} );
            push( @missing, $tbl );
        }
        return $self->do_err( sprintf( 'Unconnected tables (%s) in equijoin statement!', join( ', ', @missing ) ) );
    }
    $self->{join}->{table_order} = \@order;
    return \@order;
}

sub do_err
{
    my $self    = shift;
    my $err     = shift;
    my $errtype = shift;
    my @c       = caller 6;

    #$err = "[" . $self->{original_string} . "]\n$err\n\n";
    #    $err = "$err\n\n";
    my $prog = $c[1];
    my $line = $c[2];
    $prog = defined($prog) ? " called from $prog" : '';
    $prog .= defined($line) ? " at $line" : '';
    $err = "\nExecution ERROR: $err$prog.\n\n";

    $self->{errstr} = $err;
    carp $err if $self->{PrintError};
    croak "$err" if $self->{RaiseError};
    return;
}

sub errstr() { return $_[0]->{errstr}; }

sub where_hash() { return $_[0]->{where_clause}; }

sub column_defs() { return $_[0]->{column_defs}; }

sub where()
{
    return undef unless $_[0]->{where_terms};
    return $_[0]->{where_terms};
}

sub get_user_func_table
{
    my ( $self, $name, $u_func ) = @_;
    my $term = $self->{termFactory}->buildCondition($u_func);

    my @data_aryref = @{ $term->value(undef) };
    my $col_names   = shift @data_aryref;

    # my $tempTable = SQL::Statement::TempTable->new(
    #     $name, $col_names, $col_names, $data_aryref
    # );
    my $tempTable = SQL::Statement::RAM::Table->new( $name, $col_names, \@data_aryref );
    $tempTable->{all_cols} ||= $col_names;
    return $tempTable;
}

sub capability($)
{
    my ( $self, $capname ) = @_;
    return $self->{capabilities}->{$capname} if ( defined( $self->{capabilities}->{$capname} ) );

    return;
}

sub DESTROY
{
    my $self = $_[0];

    undef $self->{NAME};
    undef $self->{ORG_NAME};
    undef $self->{all_cols};
    undef $self->{already_prepared};
    undef $self->{argnum};
    undef $self->{col_obj};
    undef $self->{column_names};
    undef $self->{columns};
    undef $self->{cur_table};
    undef $self->{data};
    undef $self->{group_by};
    #undef $self->{has_OR};
    undef $self->{join};
    undef $self->{limit_clause};
    undef $self->{num_placeholders};
    undef $self->{num_val_placeholders};
    undef $self->{org_table_names};
    undef $self->{params};
    undef $self->{opts};
    undef $self->{procedure};
    undef $self->{set_function};
    undef $self->{sort_spec_list};
    undef $self->{subquery};
    undef $self->{tables};
    undef $self->{table_names};
    undef $self->{table_func};
    undef $self->{where_clause};
    undef $self->{where_terms};
    undef $self->{values};
}

package SQL::Statement::Aggregate;

use Scalar::Util qw(looks_like_number);
use Params::Util qw(_HASH);
use Clone qw(clone);

sub new
{
    my ( $class, $owner, $rows ) = @_;
    my $self = {
        owner   => $owner,
        records => $rows,
    };
    return bless( $self, $class );
}

my $empty_agg = {
    uniq  => [],
    count => 0,
    sum   => undef,
    min   => undef,
    max   => undef,
};

sub do_calc()
{
    my $self = $_[0];

    foreach my $line ( 0 .. ( scalar( @{ $self->{records} } ) - 1 ) )
    {
        my $row    = $self->{records}->[$line];
        my $result = $self->getAffectedResult($row);

        foreach my $colidx ( 0 .. ( scalar( @{ $self->{owner}->{columns} } ) - 1 ) )
        {
            my $coldef = $self->{owner}->{columns}->[$colidx]->{coldef};
            my $colval = $row->[$colidx];

            if ( $coldef->{type} eq 'setfunc' )
            {
                if ( $coldef->{distinct} eq 'DISTINCT' )
                {
                    next if defined( $result->{uniq}->[$colidx]->{$colval} );
                    $result->{uniq}->[$colidx]->{$colval} = 1;
                }

                $result->{agg}->[$colidx] = clone($empty_agg)
                  unless ( defined( _HASH( $result->{agg}->[$colidx] ) ) );
                my $agg = $result->{agg}->[$colidx];

                ++$agg->{count};
                unless ( defined( $agg->{max} )
                    && ( SQL::Statement::_anycmp( $colval, $agg->{max} ) < 0 ) )
                {
                    $agg->{max} = $colval;
                }
                unless ( defined( $agg->{min} )
                    && ( SQL::Statement::_anycmp( $colval, $agg->{min} ) > 0 ) )
                {
                    $agg->{min} = $colval;
                }
                $agg->{sum} += $colval if ( looks_like_number($colval) );
            }
            else
            {
                $result->{pure}->[$colidx] = $colval
                  unless ( defined( $result->{pure}->[$colidx] ) );
            }
        }
    }
}

sub build_row    # (\%)
{
    my ( $self, $result ) = @_;
    my @row;

    foreach my $colidx ( 0 .. ( scalar( @{ $self->{owner}->{columns} } ) - 1 ) )
    {
        my $coldef = $self->{owner}->{columns}->[$colidx]->{coldef};

        if ( $coldef->{type} eq 'setfunc' )
        {
            if ( $coldef->{name} eq 'COUNT' )
            {
                push( @row, $result->{agg}->[$colidx]->{count} || 0 );
            }
            elsif ( $coldef->{name} eq 'MAX' )
            {
                push( @row, $result->{agg}->[$colidx]->{max} );
            }
            elsif ( $coldef->{name} eq 'MIN' )
            {
                push( @row, $result->{agg}->[$colidx]->{min} );
            }
            elsif ( $coldef->{name} eq 'SUM' )
            {
                push( @row, $result->{agg}->[$colidx]->{sum} );
            }
            elsif ( $coldef->{name} eq 'AVG' )
            {
                my $count = $result->{agg}->[$colidx]->{count};
                my $sum   = $result->{agg}->[$colidx]->{sum};
                my $avg   = $sum / $count if ( $count && $sum );
                push( @row, $avg );
            }
            else
            {
                return $self->{owner}->do_err("Invalid SET FUNCTION '$coldef->{name}'");
            }
        }
        else
        {
            push( @row, $result->{pure}->[$colidx] );
        }
    }

    return \@row;
}

sub calc()
{
    my $self = $_[0];

    $self->{final_row} = {};

    $self->do_calc();

    my $final_row = $self->build_row( $self->{final_row} );

    return [$final_row];
}

sub getAffectedResult    # (\@)
{
    return $_[0]->{final_row};
}

package SQL::Statement::Group;

use vars qw(@ISA);

use Params::Util qw(_HASH);

@ISA = qw(SQL::Statement::Aggregate);

sub new
{
    my ( $class, $owner, $rows, $keycols ) = @_;

    my $self = $class->SUPER::new( $owner, $rows );
    $self->{keycols} = $keycols;

    return $self;
}

sub calc()
{
    my $self = $_[0];
    my @final_table;

    $self->do_calc();

    if ( scalar( keys( %{ $self->{final_rows} } ) ) )
    {
        foreach my $key ( keys( %{ $self->{final_rows} } ) )
        {
            my $final_row = $self->build_row( $self->{final_rows}->{$key} );
            push( @final_table, $final_row );
        }
    }
    else
    {
        my $final_row = $self->build_row( {} );
        push( @final_table, $final_row );
    }

    return \@final_table;
}

sub getAffectedResult    # (\@)
{
    my ( $self, $row ) = @_;

    my $rowkey = join( "\0", @$row[ @{ $self->{keycols} } ] );

    $self->{final_rows}->{$rowkey} = {}
      unless ( defined( _HASH( $self->{final_rows}->{$rowkey} ) ) );

    return $self->{final_rows}->{$rowkey};
}

package SQL::Statement::TempTable;

use vars qw(@ISA);

BEGIN
{
    require SQL::Eval;

    @SQL::Statement::TempTable::ISA = qw(SQL::Eval::Table);
}

sub new
{
    my ( $class, $name, $col_names, $table_cols, $table ) = @_;
    my %col_nums;
    $col_nums{ $col_names->[$_] } = $_ for ( 0 .. scalar @$col_names - 1 );
    my @display_order = @col_nums{@$table_cols};
    my $self          = {
        col_names  => $col_names,
        table_cols => \@display_order,
        col_nums   => \%col_nums,
        table      => $table,
        NAME       => $name,
        rowpos     => 0,
        maxrow     => scalar @$table
    };
    return $class->SUPER::new($self);
}

sub is_shared($) { $_[0]->{is_shared}->{ $_[1] }; }
sub get_pos()    { $_[0]->{rowpos} }

sub column_num($)
{
    my ( $s, $col ) = @_;
    my $new_col = $s->{col_nums}->{$col};
    unless ( defined($new_col) )
    {
        my @tmp = split( '~', $col );
        return unless ( 2 == scalar(@tmp) );
        $new_col = lc( $tmp[0] ) . '~' . $tmp[1];
        $new_col = $s->{col_nums}->{$new_col};
    }
    return $new_col;
}

sub fetch_row()
{
    return $_[0]->{row} =
      ( $_[0]->{rowpos} >= $_[0]->{maxrow} )
      ? undef
      : $_[0]->{table}->[ $_[0]->{rowpos}++ ];
}

sub column($) { return $_[0]->{row}->[ $_[0]->{col_nums}->{ $_[1] } ]; }

package SQL::Statement::Order;

sub new ($$)
{
    my $proto = shift;
    my $self  = {@_};
    bless( $self, ( ref($proto) || $proto ) );
}
sub table ($)     { $_[0]->{col}->table(); }
sub column ($)    { $_[0]->{col}->display_name(); }
sub desc ($)      { $_[0]->{desc}; }
sub direction ($) { $_[0]->{direction}; }

package SQL::Statement::Limit;

sub new ($$)
{
    my ( $proto, $self ) = @_;
    bless( $self, ( ref($proto) || $proto ) );
}

#sub limit ($) { shift->{limit}; }
#sub offset ($) { shift->{offset}; }

package SQL::Statement::Param;

sub new
{
    my ( $class, $idx ) = @_;
    my $self = { 'idx' => $idx };
    return bless( $self, $class );
}

sub idx ($) { $_[0]->{idx}; }

package SQL::Statement::Table;

sub new
{
    my ( $class, $table_name ) = @_;

    if ( $table_name !~ m/"/ )
    {
        $table_name = lc $table_name;
    }

    my $self = {
        name => $table_name,
    };

    return bless( $self, $class );
}

sub name { $_[0]->{name} }

1;
__END__

=pod

=head1 NAME

SQL::Statement - SQL parsing and processing engine

=head1 SYNOPSIS

  # ... depends on what you want to do, see below

=head1 DESCRIPTION

The SQL::Statement module implements a pure Perl SQL parsing and execution
engine. While it by no means implements full ANSI standard, it does support
many features including column and table aliases, built-in and user-defined
functions, implicit and explicit joins, complex nested search conditions,
and other features.

SQL::Statement is a small embeddable Database Management System
(DBMS). This means that it provides all of the services of a simple
DBMS except that instead of a persistent storage mechanism, it has two
things: 1) an in-memory storage mechanism that allows you to prepare,
execute, and fetch from SQL statements using temporary tables and 2) a
set of software sockets where any author can plug in any storage
mechanism.

There are three main uses for SQL::Statement. One or another (hopefully not
all) may be irrelevant for your needs: 1) to access and manipulate data in
CSV, XML, and other formats 2) to build your own DBD for a new data source
3) to parse and examine the structure of SQL statements.

=head1 INSTALLATION

There are no prerequisites for using this as a standalone parser. If
you want to access persistent stored data, you either need to write a
subclass or use one of the DBI DBD drivers.  You can install this
module using CPAN.pm, CPANPLUS.pm, PPM, apt-get, or other packaging
tools or you can download the tar.gz file from CPAN and use the
standard perl mantra:

  perl Makefile.PL
  make
  make test
  make install

It works fine on all platforms it has been tested on. On Windows, you
can use ppm or with the mantra use nmake, dmake, or make depending on
which is available.

=head1 USAGE

=head2 How can I use SQL::Statement to access and modify data?

SQL::Statement provides the SQL engine for a number of existing DBI drivers
including L<DBD::CSV>, L<DBD::DBM>, L<DBD::AnyData>, L<DBD::Excel>,
L<DBD::Amazon>, and others.

These modules provide access to Comma Separated Values, Fixed Length, XML,
HTML and many other kinds of text files, to Excel Spreadsheets, to BerkeleyDB
and other DBM formats, and to non-traditional data sources like on-the-fly
Amazon searches.

If you are interested in accessing and manipulating persistent data, you may
not really want to use SQL::Statement directly, but use L<DBI> along with
one of the DBDs mentioned above instead. You will be using SQL::Statement, but
under the hood of the DBD. See L<http://dbi.perl.org> for help with DBI and
see L<SQL::Statement::Syntax> for a description of the SQL syntax that
SQL::Statement provides for these modules and see the documentation for
whichever DBD you are using for additional details.

=head2 How can I use it to parse and examine the structure of SQL statements?

SQL::Statement can be used stand-alone (without a subclass and without
DBI) to parse and examine the structure of SQL statements.  See
L<SQL::Statement::Structure> for details.

=head2 How can I use it to embed a SQL engine in a DBD or other module?

SQL::Statement is designed to be easily embedded in other modules and is
especially suited for developing new DBI drivers (DBDs).
See L<SQL::Statement::Embed>.

=head2 What SQL Syntax is supported?

SQL::Statement supports a small but powerful subset of SQL commands.
See L<SQL::Statement::Syntax>.

=head2 How can I extend the supported SQL syntax?

You can modify and extend the SQL syntax either by issuing SQL commands or
by subclassing SQL::Statement.  See L<SQL::Statement::Syntax>.

=head1 How can I participate in ongoing development?

SQL::Statement is a large module with many potential future directions.
You are invited to help plan, code, test, document, or kibbitz about these
directions. If you want to join the development team, or just hear more
about the development, write Jeff (<jzuckerATcpan.org>) or Jens
(<rehsackATcpan.org>) a note.

=head1 METHODS

The following methods can or must be overridden by derived classes.

=head2 capability

  $has_capability = $h->capability('capability_name');

Returns a true value if the specified capability is available.

Currently no capabilities are defined and this is a placeholder for
future use. It is envisioned it will be used like C<<
SQL::Eval::Table::capability >>.

=head2 open_table

The C<< open_table >> method must be overridden by derived classes to provide
the capability of opening data tables. This is a necessity.

Arguments given to open_table call:

=over 4

=item C<< $data >>

The database memo parameter. See L</execute>.

=item C<< $table >>

The name of the table to open as parsed from SQL statement.

=item C<< $createMode >>

A flag indicating the mode (C<< CREATE TABLE ... >>) the table should
be opened with. Set to a true value in create mode.

=item C<< $lockMode >>

A flag indicating whether the table should be opened for writing (any
other than C<< SELECT ... >>).  Set to a true value if the table is to
be opened for write access.

=back

The following methods are required to use SQL::Statement in a DBD (for
example).

=head2 new

Instantiates a new SQL::Statement object.

Arguments:

=over 4

=item C<< $sql >>

The SQL statement for later actions.

=item C<< $parser >>

An instance of a L<SQL::Parser> object or flags for it's instantiation.
If omitted, default flags are used.

=back

When the basic initialization is completed,
C<< $self->prepare($sql, $parser) >> is invoked.

=head2 prepare

Prepares SQL::Statement to execute a SQL statement.

Arguments:

=over 4

=item C<< $sql >>

The SQL statement to parse and prepare.

=item C<< $parser >>

Instance of a L<SQL::Parser> object to parse the provided SQL statement.

=back

=head2 execute

Executes a prepared statement.

Arguments:

=over 4

=item C<< $data >>

Memo field passed through to calls of the instantiated C<< $table >>
objects or C<< open_table >> calls. In C<< CREATE >> with subquery,
C<< $data->{Database} >> must be a DBI database handle object.

=item C<< $params >>

Bound params via DBI ...

=back

=head2 errstr

Gives the error string of the last error, if any.

=head2 fetch_row

Fetches the next row from the result data set (implies removing the fetched
row from the result data set).

=head2 fetch_rows

Fetches all (remaining) rows from the result data set.

=begin undocumented

=head2 _anycmp

=head2 buildColumnObjects

=head2 colname2colnum

=head2 colname2table

=head2 column_names

=head2 columns

=head2 command

=head2 distinct

=head2 do_err

=head2 eval_where

=head2 fetch

=head2 find_join_columns

=head2 full_qualified_column_name

=head2 getColumnObject

=head2 get_user_func_table

=head2 join_2_tables

=head2 limit

=head2 offset

=head2 open_tables

=head2 order

=head2 order_joins

=head2 params

=head2 row_values

=head2 run_functions

=head2 tables

=head2 verify_columns

=head2 verify_expand_column

=head2 verify_order_cols

=head2 where

=head2 where_hash

=head2 column_defs

=end undocumented

=head1 SUPPORT

You can find documentation for this module with the perldoc command.

    perldoc SQL::Statement

You can also look for information at:

=over 4

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=SQL-Statement>

=item * AnnoCPAN: Annotated CPAN documentation

L<http://annocpan.org/dist/SQL-Statement>

=item * CPAN Ratings

L<http://cpanratings.perl.org/s/SQL-Statement>

=item * CPAN Search

L<http://search.cpan.org/dist/SQL-Statement/>

=back

=head2 Where can I go for help?

For questions about installation or usage, please ask on the
dbi-users@perl.org mailing list (see http://dbi.perl.org) or post a
question on PerlMonks (L<http://www.perlmonks.org/>, where Jeff is
known as jZed).  Jens does not visit PerlMonks on a regular basis.

If you have a bug report, a patch or a suggestion, please open a new
report ticket at CPAN (but please check previous reports first in case
your issue has already been addressed). You can mail any of the module
maintainers, but you are more assured of an answer by posting to
the dbi-users list or reporting the issue in RT.

Report tickets should contain a detailed description of the bug or
enhancement request and at least an easily verifiable way of
reproducing the issue or fix. Patches are always welcome, too.

=head2 Where can I go for help with a concrete version?

Bugs and feature requests are accepted against the latest version
only. To get patches for earlier versions, you need to get an
agreement with a developer of your choice - who may or not report the
issue and a suggested fix upstream (depends on the license you have
chosen).

=head2 Business support and maintenance

For business support you can contact Jens via his CPAN email
address rehsackATcpan.org. Please keep in mind that business
support is neither available for free nor are you eligible to
receive any support based on the license distributed with this
package.

=head1 ACKNOWLEDGEMENTS

Jochen Wiedmann created the original module as an XS (C) extension in 1998.
Jeff Zucker took over the maintenance in 2001 and rewrote all of the C
portions in Perl and began extending the SQL support.  More recently Ilya
Sterin provided help with SQL::Parser, Tim Bunce provided both general and
specific support, Dan Wright and Dean Arnold have contributed extensively
to the code, and dozens of people from around the world have submitted
patches, bug reports, and suggestions.

In 2008 Jens Rehsack took over the maintenance of the extended module
from Jeff.  Together with H.Merijn Brand (who has taken DBD::CSV),
Detlef Wartke and Volker Schubbert (especially between 1.16 developer
versions until 1.22) and all submitters of bug reports via RT a lot of
issues have been fixed.

Thanks to all!

If you're interested in helping develop SQL::Statement or want to use it
with your own modules, feel free to contact Jeff or Jens.

=head1 BUGS AND LIMITATIONS

=over 4

=item *

Currently we treat NULL and '' as the same in AnyData/CSV mode -
eventually fix.

=item *

No nested C-style comments allowed as SQL99 says.

=item *

There are some issues regarding combining outer joins with where
clauses.

=item *

Aggregate functions cannot be used in where clause.

=item *

Some SQL commands/features are not supported (most of them cannot by
design), as C<LOCK TABLE>, using indices, sub-selects etc.

Currently the statement for missing features is: I plan to create a
SQL::Statement v2.00 based on a pure Backus-Naur-Form parser and a
fully object oriented command pattern based engine implementation.
When the time is available, I will do it. Until then bugs will be
fixed or other Perl modules under my maintainership will receive my
time. Features which can be added without deep design changes might be
applied earlier - especially when their addition allows studying
effective ways to implement the feature in upcoming 2.00.

=item *

Some people report that SQL::Statement is slower since the XS parts
were implemented in pure Perl. This might be true, but on the other
hand a large number of features have been added including support for
ANSI SQL 99.

For SQL::Statement 1.xx it's not planned to add new XS parts.

=item *

Wildcards are expanded to lower cased identifiers. This might confuse
some people, but it was easier to implement.

The warning in L<DBI> to never trust the case of returned column names
should be read more often. If you need to rely on identifiers, always
use C<sth-E<gt>{NAME_lc}> or C<sth-E<gt>{NAME_uc}> - never rely on
C<sth-E<gt>{NAME}>:

  $dbh->{FetchHashKeyName} = 'NAME_lc';
  $sth = $dbh->prepare("SELECT FOO, BAR, ID, NAME, BAZ FROM TABLE");
  $sth->execute;
  $hash_ref = $sth->fetchall_hashref('id');
  print "Name for id 42 is $hash_ref->{42}->{name}\n";

See L<DBI/FetchHashKeyName> for more information.

=item *

Unable to use the same table twice with different aliases. B<Workaround>:
Temporary tables: C<< CREATE TEMP TABLE t_foo AS SELECT * FROM foo >>.
Than both tables can be used independently.

=back

Patches to fix bugs/limitations (or a grant to do it) would be
very welcome. Please note, that any patches B<must> successfully pass
all the C<SQL::Statement>, L<DBD::File> and L<DBD::CSV> tests and must
be a general improvement.

=head1 AUTHOR AND COPYRIGHT

Jochen Wiedmann created the original module as an XS (C) extension in 1998.
Jeff Zucker took over the maintenance in 2001 and rewrote all of the C
portions in perl and began extending the SQL support. Since 2008, Jens
Rehsack is the maintainer.

Copyright (c) 2001,2005 by Jeff Zucker: jzuckerATcpan.org
Copyright (c) 2007-2017 by Jens Rehsack: rehsackATcpan.org

Portions Copyright (C) 1998 by Jochen Wiedmann: jwiedATcpan.org

All rights reserved.

You may distribute this module under the terms of either the GNU
General Public License or the Artistic License, as specified in
the Perl README file.

=cut
