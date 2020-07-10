package DBD::ADO::TypeInfo;

use strict;
use warnings;

use DBI();
use DBD::ADO::Const();

$DBD::ADO::TypeInfo::VERSION = '2.83';

my $Enums = DBD::ADO::Const->Enums;
my $Dt = $Enums->{DataTypeEnum};

# -----------------------------------------------------------------------------
$DBD::ADO::TypeInfo::Fields =
# -----------------------------------------------------------------------------
{
  TYPE_NAME          =>  0
, DATA_TYPE          =>  1
, COLUMN_SIZE        =>  2
, LITERAL_PREFIX     =>  3
, LITERAL_SUFFIX     =>  4
, CREATE_PARAMS      =>  5
, NULLABLE           =>  6
, CASE_SENSITIVE     =>  7
, SEARCHABLE         =>  8
, UNSIGNED_ATTRIBUTE =>  9
, FIXED_PREC_SCALE   => 10
, AUTO_UNIQUE_VALUE  => 11
, LOCAL_TYPE_NAME    => 12
, MINIMUM_SCALE      => 13
, MAXIMUM_SCALE      => 14
, SQL_DATA_TYPE      => 15
, SQL_DATETIME_SUB   => 16
# NUM_PREC_RADIX     => 17
# INTERVAL_PRECISION => 18
};
# -----------------------------------------------------------------------------
$DBD::ADO::TypeInfo::dbi2ado =
# -----------------------------------------------------------------------------
{
  DBI::SQL_GUID()            => $Dt->{adGUID}            # -11
, DBI::SQL_WLONGVARCHAR()    => $Dt->{adLongVarWChar}    # -10
, DBI::SQL_WVARCHAR()        => $Dt->{adVarWChar}        #  -9
, DBI::SQL_WCHAR()           => $Dt->{adWChar}           #  -8
# DBI::SQL_BIT()                                         #  -7
, DBI::SQL_TINYINT()         => $Dt->{adTinyInt}         #  -6
, -5                         => $Dt->{adBigInt}          # SQL_BIGINT
, DBI::SQL_LONGVARBINARY()   => $Dt->{adLongVarBinary}   #  -4
, DBI::SQL_VARBINARY()       => $Dt->{adVarBinary}       #  -3
, DBI::SQL_BINARY()          => $Dt->{adBinary}          #  -2
, DBI::SQL_LONGVARCHAR()     => $Dt->{adLongVarChar}     #  -1
# DBI::SQL_UNKNOWN_TYPE()    =>                          #   0
, DBI::SQL_CHAR()            => $Dt->{adChar}            #   1
, DBI::SQL_NUMERIC()         => $Dt->{adNumeric}         #   2
, DBI::SQL_DECIMAL()         => $Dt->{adDecimal}         #   3
, DBI::SQL_INTEGER()         => $Dt->{adInteger}         #   4
, DBI::SQL_SMALLINT()        => $Dt->{adSmallInt}        #   5
, DBI::SQL_FLOAT()           => $Dt->{adSingle}          #   6
# DBI::SQL_REAL()            =>                          #   7
, DBI::SQL_DOUBLE()          => $Dt->{adDouble}          #   8
, DBI::SQL_DATE()            => $Dt->{adDBDate}          #   9  # deprecated!
# DBI::SQL_INTERVAL()        =>                          #  10
, DBI::SQL_TIMESTAMP()       => $Dt->{adDBTimeStamp}     #  11  # deprecated!
, DBI::SQL_VARCHAR()         => $Dt->{adVarChar}         #  12
, DBI::SQL_BOOLEAN()         => $Dt->{adBoolean}         #  16
, DBI::SQL_UDT()             => $Dt->{adUserDefined}     #  17
# DBI::SQL_UDT_LOCATOR()     =>                          #  18
# DBI::SQL_ROW()             =>                          #  19
# DBI::SQL_REF()             =>                          #  20
, 25                         => $Dt->{adBigInt}          # SQL_BIGINT
, DBI::SQL_BLOB()            => $Dt->{adLongVarBinary}   #  30
# DBI::SQL_BLOB_LOCATOR()    =>                          #  31
, DBI::SQL_CLOB()            => $Dt->{adLongVarChar}     #  40
# DBI::SQL_CLOB_LOCATOR()    =>                          #  41
, DBI::SQL_ARRAY()           => $Dt->{adArray}           #  50
# DBI::SQL_ARRAY_LOCATOR()   =>                          #  51
# DBI::SQL_MULTISET()        =>                          #  55
# DBI::SQL_MULTISET_LOCATOR()=>                          #  56
, DBI::SQL_TYPE_DATE()       => $Dt->{adDBDate}          #  91
, DBI::SQL_TYPE_TIME()       => $Dt->{adDBTime}          #  92
, DBI::SQL_TYPE_TIMESTAMP()  => $Dt->{adDBTimeStamp}     #  93
# DBI::SQL_TYPE_TIME_WITH_TIMEZONE()                     #  94
# DBI::SQL_TYPE_TIMESTAMP_WITH_TIMEZONE()                #  95
# DBI::SQL_INTERVAL_YEAR()                               #  101
# DBI::SQL_INTERVAL_MONTH()                              #  102
# DBI::SQL_INTERVAL_DAY()                                #  103
# DBI::SQL_INTERVAL_HOUR()                               #  104
# DBI::SQL_INTERVAL_MINUTE()                             #  105
# DBI::SQL_INTERVAL_SECOND()                             #  106
# DBI::SQL_INTERVAL_YEAR_TO_MONTH()                      #  107
# DBI::SQL_INTERVAL_DAY_TO_HOUR()                        #  108
# DBI::SQL_INTERVAL_DAY_TO_MINUTE()                      #  109
# DBI::SQL_INTERVAL_DAY_TO_SECOND()                      #  110
# DBI::SQL_INTERVAL_HOUR_TO_MINUTE()                     #  111
# DBI::SQL_INTERVAL_HOUR_TO_SECOND()                     #  112
# DBI::SQL_INTERVAL_MINUTE_TO_SECOND()                   #  113
};
# -----------------------------------------------------------------------------
my $ado2dbi =
# -----------------------------------------------------------------------------
{
  $Dt->{adArray}            => DBI::SQL_ARRAY
, $Dt->{adBigInt}           => 25
, $Dt->{adBinary}           => DBI::SQL_BINARY
, $Dt->{adBoolean}          => DBI::SQL_BOOLEAN
, $Dt->{adBSTR}             => DBI::SQL_UNKNOWN_TYPE
, $Dt->{adChapter}          => DBI::SQL_UNKNOWN_TYPE
, $Dt->{adChar}             => DBI::SQL_CHAR
, $Dt->{adCurrency}         => DBI::SQL_NUMERIC
, $Dt->{adDate}             => DBI::SQL_TYPE_TIMESTAMP # XXX Not really!
, $Dt->{adDBDate}           => DBI::SQL_TYPE_DATE
, $Dt->{adDBTime}           => DBI::SQL_TYPE_TIME
, $Dt->{adDBTimeStamp}      => DBI::SQL_TYPE_TIMESTAMP
, $Dt->{adDecimal}          => DBI::SQL_DECIMAL
, $Dt->{adDouble}           => DBI::SQL_DOUBLE
, $Dt->{adEmpty}            => DBI::SQL_UNKNOWN_TYPE
, $Dt->{adError}            => DBI::SQL_UNKNOWN_TYPE
, $Dt->{adFileTime}         => DBI::SQL_TIMESTAMP
, $Dt->{adGUID}             => DBI::SQL_GUID
, $Dt->{adIDispatch}        => DBI::SQL_UNKNOWN_TYPE
, $Dt->{adInteger}          => DBI::SQL_INTEGER
, $Dt->{adIUnknown}         => DBI::SQL_UNKNOWN_TYPE
, $Dt->{adLongVarBinary}    => DBI::SQL_LONGVARBINARY
, $Dt->{adLongVarChar}      => DBI::SQL_LONGVARCHAR
, $Dt->{adLongVarWChar}     => DBI::SQL_WLONGVARCHAR
, $Dt->{adNumeric}          => DBI::SQL_NUMERIC
, $Dt->{adPropVariant}      => DBI::SQL_UNKNOWN_TYPE
, $Dt->{adSingle}           => DBI::SQL_FLOAT
, $Dt->{adSmallInt}         => DBI::SQL_SMALLINT
, $Dt->{adTinyInt}          => DBI::SQL_TINYINT
, $Dt->{adUnsignedBigInt}   => 25
, $Dt->{adUnsignedInt}      => DBI::SQL_INTEGER
, $Dt->{adUnsignedSmallInt} => DBI::SQL_SMALLINT
, $Dt->{adUnsignedTinyInt}  => DBI::SQL_TINYINT
, $Dt->{adUserDefined}      => DBI::SQL_UDT
, $Dt->{adVarBinary}        => DBI::SQL_VARBINARY
, $Dt->{adVarChar}          => DBI::SQL_VARCHAR
, $Dt->{adVariant}          => DBI::SQL_UNKNOWN_TYPE
, $Dt->{adVarNumeric}       => DBI::SQL_NUMERIC
, $Dt->{adVarWChar}         => DBI::SQL_WVARCHAR
, $Dt->{adWChar}            => DBI::SQL_WCHAR
};
# -----------------------------------------------------------------------------
my $ado2dbi3 =
# -----------------------------------------------------------------------------
{
  #     AdoType     IsLong IsFixed => SqlType
  $Dt->{adBinary   } => { 0 => { 0 => DBI::SQL_VARBINARY
                               , 1 => DBI::SQL_BINARY        }
                        , 1 => { 0 => DBI::SQL_LONGVARBINARY
                               , 1 => DBI::SQL_UNKNOWN_TYPE  }}
, $Dt->{adChar     } => { 0 => { 0 => DBI::SQL_VARCHAR
                               , 1 => DBI::SQL_CHAR          }
                        , 1 => { 0 => DBI::SQL_LONGVARCHAR
                               , 1 => DBI::SQL_UNKNOWN_TYPE  }}
, $Dt->{adWChar    } => { 0 => { 0 => DBI::SQL_WVARCHAR
                               , 1 => DBI::SQL_WCHAR         }
                        , 1 => { 0 => DBI::SQL_WLONGVARCHAR
                               , 1 => DBI::SQL_UNKNOWN_TYPE  }}
# $Dt->{adVarBinary} =>
# $Dt->{adVarChar  } =>
# $Dt->{adVarWChar } =>
};
# -----------------------------------------------------------------------------
sub ado2dbi  # Convert an ADO data type into an DBI/ODBC/SQL data type.
# -----------------------------------------------------------------------------
{
  my ( $AdoType, $IsFixed, $IsLong ) = @_;

  $IsFixed = 0 unless $IsFixed;
  $IsLong  = 0 unless $IsLong ;

#  return $dbh->set_err( -1,"ado2dbi: call without any attributes")
#    unless $AdoType;

  my $SqlType = 0;

  if ( $AdoType & $Dt->{adArray} ) {  # XXX: & vs. ==
    $SqlType = 50;  # XXX DBI::SQL_ARRAY();
  }
  elsif ( exists $ado2dbi3->{$AdoType}{$IsLong}{$IsFixed} ) {
    $SqlType = $ado2dbi3->{$AdoType}{$IsLong}{$IsFixed};
  }
  elsif ( exists $ado2dbi->{$AdoType} ) {
    $SqlType = $ado2dbi->{$AdoType};
  }
# print "==> $AdoType, $IsFixed, $IsLong => $SqlType\n";

  if ( wantarray ) {  # DATA_TYPE, SQL_DATA_TYPE, SQL_DATETIME_SUB
    my @a = ( $SqlType );

    if ( 90 < $SqlType && $SqlType < 100 ) {  # SQL_DATETIME
      push @a, 9, $SqlType - 90;
    }
    elsif ( 100 < $SqlType && $SqlType < 120 ) {  # SQL_INTERVAL
      push @a, 10, $SqlType - 100;
    }
    else {
      push @a, $SqlType, undef;
    }
    return @a;
  }
  return $SqlType;
}
# -----------------------------------------------------------------------------
sub determine_type_support
# -----------------------------------------------------------------------------
{
  my ( $dbh ) = @_;
  die 'dbh undefined' unless $dbh;

  $dbh->trace_msg("    -> ado_determine_type_support\n", 3 );

  # Attempt to convert data types from ODBC to ADO.
  my %local_types = (
    DBI::SQL_BINARY()        => [
      $Dt->{adBinary}
    , $Dt->{adVarBinary}
    ]
  , DBI::SQL_BIT()           => [ $Dt->{adBoolean}]
  , DBI::SQL_CHAR()          => [
      $Dt->{adChar}
    , $Dt->{adVarChar}
    , $Dt->{adWChar}
    , $Dt->{adVarWChar}
    ]
  , DBI::SQL_DATE()          => [
      $Dt->{adDBTimeStamp}
    , $Dt->{adDate}
    ]
  , DBI::SQL_DECIMAL()       => [ $Dt->{adNumeric} ]
  , DBI::SQL_DOUBLE()        => [ $Dt->{adDouble} ]
  , DBI::SQL_FLOAT()         => [ $Dt->{adSingle} ]
  , DBI::SQL_INTEGER()       => [ $Dt->{adInteger} ]
  , DBI::SQL_LONGVARBINARY() => [
      $Dt->{adLongVarBinary}
    , $Dt->{adVarBinary}
    , $Dt->{adBinary}
    ]
  , DBI::SQL_LONGVARCHAR()   => [
      $Dt->{adLongVarChar}
    , $Dt->{adVarChar}
    , $Dt->{adChar}
    , $Dt->{adLongVarWChar}
    , $Dt->{adVarWChar}
    , $Dt->{adWChar}
    ]
  , DBI::SQL_NUMERIC()       => [ $Dt->{adNumeric} ]
  , DBI::SQL_REAL()          => [ $Dt->{adSingle} ]
  , DBI::SQL_SMALLINT()      => [ $Dt->{adSmallInt} ]
  , DBI::SQL_TIMESTAMP()     => [
      $Dt->{adDBTime}
    , $Dt->{adDBTimeStamp}
    , $Dt->{adDate}
    ]
  , DBI::SQL_TINYINT()       => [ $Dt->{adUnsignedTinyInt} ]
  , DBI::SQL_VARBINARY()     => [
      $Dt->{adVarBinary}
    , $Dt->{adLongVarBinary}
    , $Dt->{adBinary}
    ]
  , DBI::SQL_VARCHAR()       => [
      $Dt->{adVarChar}
    , $Dt->{adChar}
    , $Dt->{adVarWChar}
    , $Dt->{adWChar}
    ]
  , DBI::SQL_WCHAR()         => [
      $Dt->{adWChar}
    , $Dt->{adVarWChar}
    , $Dt->{adLongVarWChar}
    ]
  , DBI::SQL_WVARCHAR()      => [
      $Dt->{adVarWChar}
    , $Dt->{adLongVarWChar}
    , $Dt->{adWChar}
    ]
  , DBI::SQL_WLONGVARCHAR()  => [
      $Dt->{adLongVarWChar}
    , $Dt->{adVarWChar}
    , $Dt->{adWChar}
    , $Dt->{adLongVarChar}
    , $Dt->{adVarChar}
    , $Dt->{adChar}
    ]
  );

  my @sql_types = (
    DBI::SQL_BINARY()
  , DBI::SQL_BIT()
  , DBI::SQL_CHAR()
  , DBI::SQL_DATE()
  , DBI::SQL_DECIMAL()
  , DBI::SQL_DOUBLE()
  , DBI::SQL_FLOAT()
  , DBI::SQL_INTEGER()
  , DBI::SQL_LONGVARBINARY()
  , DBI::SQL_LONGVARCHAR()
  , DBI::SQL_NUMERIC()
  , DBI::SQL_REAL()
  , DBI::SQL_SMALLINT()
  , DBI::SQL_TIMESTAMP()
  , DBI::SQL_TINYINT()
  , DBI::SQL_VARBINARY()
  , DBI::SQL_VARCHAR()
  , DBI::SQL_WCHAR()
  , DBI::SQL_WVARCHAR()
  , DBI::SQL_WLONGVARCHAR()
  );

  # Get the Provider Types attributes.
  my $QueryType = 'adSchemaProviderTypes';
  my $conn = $dbh->{ado_conn};
  my @sort_rows;
  my %ct;
  my $rs = $conn->OpenSchema( $Enums->{SchemaEnum}{$QueryType} );
  return if DBD::ADO::Failed( $dbh,"Can't OpenSchema ($QueryType)");

  my @ado_info = map { $_->Name } Win32::OLE::in( $rs->Fields );

  while ( !$rs->{EOF} ) {
    # Sort by row
    my $type_name = $rs->{TYPE_NAME}{Value};
    my $def;
    push ( @sort_rows, $def = join(' '
    , $rs->{DATA_TYPE     }{Value}
    , $rs->{BEST_MATCH    }{Value} || 0
    , $rs->{IS_LONG       }{Value} || 0
    , $rs->{IS_FIXEDLENGTH}{Value} || 0
    , $rs->{COLUMN_SIZE   }{Value}
    , $rs->{TYPE_NAME     }{Value}
    ));
    $dbh->trace_msg("    -- data type $type_name: $def\n", 5 );
    @{$ct{$type_name}} = map { $rs->{$_}{Value} || '' } @ado_info;
    $rs->MoveNext;
  }
  $rs->Close if $rs && $rs->State & $Enums->{ObjectStateEnum}{adStateOpen};
  $rs = undef;

  for my $t ( @sql_types ) {
    # Attempt to work with LONG text fields.
    # However for a LONG field, the order by ... isn't always the best pick.
    # Loop through the rows looking for something with a IS LONG mark.
    my $alt = join '|', @{$local_types{$t}};
    my $re;
    if    ( $t == DBI::SQL_LONGVARCHAR()   ) { $re = qr{^($alt)\s\d\s1\s0\s}  }
    elsif ( $t == DBI::SQL_LONGVARBINARY() ) { $re = qr{^($alt)\s\d\s1\s0\s}  }
    elsif ( $t == DBI::SQL_VARBINARY()     ) { $re = qr{^($alt)\s1\s\d\s0\s}  }
    elsif ( $t == DBI::SQL_VARCHAR()       ) { $re = qr{^($alt)\s[01]\s0\s0\s}}
    elsif ( $t == DBI::SQL_WVARCHAR()      ) { $re = qr{^($alt)\s[01]\s0\s0\s}}
    elsif ( $t == DBI::SQL_WLONGVARCHAR()  ) { $re = qr{^($alt)\s\d\s1\s0\s}  }
    elsif ( $t == DBI::SQL_CHAR()          ) { $re = qr{^($alt)\s\d\s0\s1\s}  }
    elsif ( $t == DBI::SQL_WCHAR()         ) { $re = qr{^($alt)\s\d\s0\s1\s}  }
    else                                     { $re = qr{^($alt)\s\d\s\d\s}    }

    for ( sort { $b cmp $a } grep { /$re/ } @sort_rows ) {
      my ( $cc ) = m/\d+\s+(\D\w?.*)$/;
      Carp::carp "$cc does not exist in hash\n" unless exists $ct{$cc};
      my @rec = @{$ct{$cc}};
      $dbh->trace_msg("    ** Changing type $rec[1] -> $t : @rec\n", 6 );
      $rec[1] = $t;
      push @{$dbh->{ado_all_types_supported}}, \@rec;
    }
  }
  $dbh->trace_msg("    <- ado_determine_type_support\n", 3 );
  return \@{$dbh->{ado_all_types_supported}};
}
# -----------------------------------------------------------------------------
sub type_info_all_1
# -----------------------------------------------------------------------------
{
  my ( $dbh ) = @_;
  my $QueryType = 'adSchemaProviderTypes';
  my $names = {
    TYPE_NAME          =>  0
  , DATA_TYPE          =>  1
  , COLUMN_SIZE        =>  2
  , LITERAL_PREFIX     =>  3
  , LITERAL_SUFFIX     =>  4
  , CREATE_PARAMS      =>  5
  , NULLABLE           =>  6
  , CASE_SENSITIVE     =>  7
  , SEARCHABLE         =>  8
  , UNSIGNED_ATTRIBUTE =>  9
  , FIXED_PREC_SCALE   => 10
  , AUTO_UNIQUE_VALUE  => 11
  , LOCAL_TYPE_NAME    => 12
  , MINIMUM_SCALE      => 13
  , MAXIMUM_SCALE      => 14
  };
  # If the type information is previously obtained, use it.
  unless ( $dbh->{ado_all_types_supported} ) {
    DBD::ADO::TypeInfo::determine_type_support( $dbh )
      or Carp::croak "Can't determine_type_support: $dbh->{errstr}";
  }
  my $ops = DBD::ADO::db::ado_open_schema( $dbh, $QueryType )
    or Carp::croak "Can't OpenSchema ($QueryType)";

  my $sth = DBI->connect('dbi:Sponge:','','', { RaiseError => 1 } )->prepare(
    $QueryType, { rows => [ @{$dbh->{ado_all_types_supported}} ]
  , NAME => [ @{$ops->{NAME}} ]
  });
  $ops->finish; $ops = undef;

  my @ti;
  while ( my $row = $sth->fetchrow_hashref ) {
    my $ti;
    # Only add items from the above names list.
    # When this list explans, the code 'should' still work.
    while ( my ( $k, $v ) = each %$names ) {
      $ti->[$v] = $row->{$k} || '';
    }
    push @ti, $ti;
  }
  return [ $names, @ti ];
}
# -----------------------------------------------------------------------------
sub type_info_all_2
# -----------------------------------------------------------------------------
{
  my ( $dbh ) = @_;
  my $QueryType = 'adSchemaProviderTypes';
  my $conn = $dbh->{ado_conn};
  my @Rows;
  my $rs = $conn->OpenSchema( $Enums->{SchemaEnum}{$QueryType} );
  return if DBD::ADO::Failed( $dbh,"Can't OpenSchema ($QueryType)");

  while ( !$rs->{EOF} ) {
    my $AdoType = $rs->{DATA_TYPE     }{Value};
    my $IsLong  = $rs->{IS_LONG       }{Value};
    my $IsFixed = $rs->{IS_FIXEDLENGTH}{Value};
    my @SqlType = DBD::ADO::TypeInfo::ado2dbi( $AdoType, $IsFixed, $IsLong );
    my $Fields  =
    [
      $rs->{TYPE_NAME         }{Value} #  0 TYPE_NAME
    , $SqlType[0]                      #  1 DATA_TYPE
    , $rs->{COLUMN_SIZE       }{Value} #  2 COLUMN_SIZE
    , $rs->{LITERAL_PREFIX    }{Value} #  3 LITERAL_PREFIX
    , $rs->{LITERAL_SUFFIX    }{Value} #  4 LITERAL_SUFFIX
    , $rs->{CREATE_PARAMS     }{Value} #  5 CREATE_PARAMS
    , $rs->{IS_NULLABLE       }{Value} #  6 NULLABLE
    , $rs->{CASE_SENSITIVE    }{Value} #  7 CASE_SENSITIVE
    , $rs->{SEARCHABLE        }{Value} #  8 SEARCHABLE
    , $rs->{UNSIGNED_ATTRIBUTE}{Value} #  9 UNSIGNED_ATTRIBUTE
    , $rs->{FIXED_PREC_SCALE  }{Value} # 10 FIXED_PREC_SCALE
    , $rs->{AUTO_UNIQUE_VALUE }{Value} # 11 AUTO_UNIQUE_VALUE
    , $rs->{LOCAL_TYPE_NAME   }{Value} # 12 LOCAL_TYPE_NAME
    , $rs->{MINIMUM_SCALE     }{Value} # 13 MINIMUM_SCALE
    , $rs->{MAXIMUM_SCALE     }{Value} # 14 MAXIMUM_SCALE
    , $SqlType[1]                      # 15 SQL_DATA_TYPE
    , $SqlType[2]                      # 16 SQL_DATETIME_SUB
    ];
    $Fields->[8]--;
    push @Rows, $Fields;
    $rs->MoveNext;
  }
  $rs->Close; undef $rs;

  # TODO: 2nd crit. for equal types
  return [ $DBD::ADO::TypeInfo::Fields, sort { $a->[1] <=> $b->[1] } @Rows ];
}
# -----------------------------------------------------------------------------
sub Find3
# -----------------------------------------------------------------------------
{
  my ( $dbh, $AdoType, $IsFixed, $IsLong ) = @_;

  unless ( $dbh->{ado_type_info_hash} ) {
    my $sth = $dbh->func('adSchemaProviderTypes','OpenSchema');
    while ( my $r = $sth->fetchrow_hashref ) {
      push @{$dbh->{ado_type_info_hash}{$r->{DATA_TYPE}}{$r->{IS_FIXEDLENGTH}}{$r->{IS_LONG}}}, $r;
    }
  }
  $dbh->{ado_type_info_hash}{$AdoType}{$IsFixed}{$IsLong} || [];
}
# -----------------------------------------------------------------------------
1;

=head1 NAME

DBD::ADO::TypeInfo - ADO TypeInfo

=head1 SYNOPSIS

  use DBD::ADO::TypeInfo();
  ...

=head1 DESCRIPTION

This module helps to handle DBI datatype information.
It provides mappings between DBI (SQL/CLI, ODBC) and ADO datatypes.

=head1 AUTHOR

Steffen Goeldner (sgoeldner@cpan.org)

=head1 COPYRIGHT

Copyright (c) 2002-2005 Steffen Goeldner. All rights reserved.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
