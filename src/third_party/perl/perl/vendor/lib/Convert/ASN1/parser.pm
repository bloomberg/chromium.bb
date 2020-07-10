#$yysccsid = "@(#)yaccpar 1.8 (Berkeley) 01/20/91 (Perl 2.0 12/31/92)";
# 24 "parser.y"
;# Copyright (c) 2000-2005 Graham Barr <gbarr@pobox.com>. All rights reserved.
;# This program is free software; you can redistribute it and/or
;# modify it under the same terms as Perl itself.

package Convert::ASN1::parser;
{
  $Convert::ASN1::parser::VERSION = '0.27';
}

use strict;
use Convert::ASN1 qw(:all);
use vars qw(
  $asn $yychar $yyerrflag $yynerrs $yyn @yyss
  $yyssp $yystate @yyvs $yyvsp $yylval $yys $yym $yyval
);

BEGIN { Convert::ASN1->_internal_syms }

my $yydebug=0;
my %yystate;

my %base_type = (
  BOOLEAN	    => [ asn_encode_tag(ASN_BOOLEAN),		opBOOLEAN ],
  INTEGER	    => [ asn_encode_tag(ASN_INTEGER),		opINTEGER ],
  BIT_STRING	    => [ asn_encode_tag(ASN_BIT_STR),		opBITSTR  ],
  OCTET_STRING	    => [ asn_encode_tag(ASN_OCTET_STR),		opSTRING  ],
  STRING	    => [ asn_encode_tag(ASN_OCTET_STR),		opSTRING  ],
  NULL 		    => [ asn_encode_tag(ASN_NULL),		opNULL    ],
  OBJECT_IDENTIFIER => [ asn_encode_tag(ASN_OBJECT_ID),		opOBJID   ],
  REAL		    => [ asn_encode_tag(ASN_REAL),		opREAL    ],
  ENUMERATED	    => [ asn_encode_tag(ASN_ENUMERATED),	opINTEGER ],
  ENUM		    => [ asn_encode_tag(ASN_ENUMERATED),	opINTEGER ],
  'RELATIVE-OID'    => [ asn_encode_tag(ASN_RELATIVE_OID),	opROID	  ],

  SEQUENCE	    => [ asn_encode_tag(ASN_SEQUENCE | ASN_CONSTRUCTOR), opSEQUENCE ],
  EXPLICIT	    => [ asn_encode_tag(ASN_SEQUENCE | ASN_CONSTRUCTOR), opEXPLICIT ],
  SET               => [ asn_encode_tag(ASN_SET      | ASN_CONSTRUCTOR), opSET ],

  ObjectDescriptor  => [ asn_encode_tag(ASN_UNIVERSAL |  7), opSTRING ],
  UTF8String        => [ asn_encode_tag(ASN_UNIVERSAL | 12), opUTF8 ],
  NumericString     => [ asn_encode_tag(ASN_UNIVERSAL | 18), opSTRING ],
  PrintableString   => [ asn_encode_tag(ASN_UNIVERSAL | 19), opSTRING ],
  TeletexString     => [ asn_encode_tag(ASN_UNIVERSAL | 20), opSTRING ],
  T61String         => [ asn_encode_tag(ASN_UNIVERSAL | 20), opSTRING ],
  VideotexString    => [ asn_encode_tag(ASN_UNIVERSAL | 21), opSTRING ],
  IA5String         => [ asn_encode_tag(ASN_UNIVERSAL | 22), opSTRING ],
  UTCTime           => [ asn_encode_tag(ASN_UNIVERSAL | 23), opUTIME ],
  GeneralizedTime   => [ asn_encode_tag(ASN_UNIVERSAL | 24), opGTIME ],
  GraphicString     => [ asn_encode_tag(ASN_UNIVERSAL | 25), opSTRING ],
  VisibleString     => [ asn_encode_tag(ASN_UNIVERSAL | 26), opSTRING ],
  ISO646String      => [ asn_encode_tag(ASN_UNIVERSAL | 26), opSTRING ],
  GeneralString     => [ asn_encode_tag(ASN_UNIVERSAL | 27), opSTRING ],
  CharacterString   => [ asn_encode_tag(ASN_UNIVERSAL | 28), opSTRING ],
  UniversalString   => [ asn_encode_tag(ASN_UNIVERSAL | 28), opSTRING ],
  BMPString         => [ asn_encode_tag(ASN_UNIVERSAL | 30), opSTRING ],
  BCDString         => [ asn_encode_tag(ASN_OCTET_STR), opBCD ],

  CHOICE => [ '', opCHOICE ],
  ANY    => [ '', opANY ],

  EXTENSION_MARKER => [ '', opEXTENSIONS ],
);

my $tagdefault = 1; # 0:IMPLICIT , 1:EXPLICIT default

;# args: class,plicit
sub need_explicit {
  (defined($_[0]) && (defined($_[1])?$_[1]:$tagdefault));
}

;# Given an OP, wrap it in a SEQUENCE

sub explicit {
  my $op = shift;
  my @seq = @$op;

  @seq[cTYPE,cCHILD,cVAR,cLOOP] = ('EXPLICIT',[$op],undef,undef);
  @{$op}[cTAG,cOPT] = ();

  \@seq;
}

sub constWORD () { 1 }
sub constCLASS () { 2 }
sub constSEQUENCE () { 3 }
sub constSET () { 4 }
sub constCHOICE () { 5 }
sub constOF () { 6 }
sub constIMPLICIT () { 7 }
sub constEXPLICIT () { 8 }
sub constOPTIONAL () { 9 }
sub constLBRACE () { 10 }
sub constRBRACE () { 11 }
sub constCOMMA () { 12 }
sub constANY () { 13 }
sub constASSIGN () { 14 }
sub constNUMBER () { 15 }
sub constENUM () { 16 }
sub constCOMPONENTS () { 17 }
sub constPOSTRBRACE () { 18 }
sub constDEFINED () { 19 }
sub constBY () { 20 }
sub constEXTENSION_MARKER () { 21 }
sub constYYERRCODE () { 256 }
my @yylhs = (                                               -1,
    0,    0,    2,    2,    3,    3,    6,    6,    6,    6,
    8,   13,   13,   12,   14,   14,   14,    9,    9,    9,
   10,   18,   18,   18,   18,   18,   19,   19,   11,   16,
   16,   20,   20,   20,   21,   21,    1,    1,    1,   22,
   22,   22,   24,   24,   24,   24,   23,   23,   23,   23,
   15,   15,    4,    4,    5,    5,    5,   17,   17,   25,
    7,    7,
);
my @yylen = (                                                2,
    1,    1,    3,    4,    4,    1,    1,    1,    1,    1,
    3,    1,    1,    6,    1,    1,    1,    4,    4,    4,
    4,    1,    1,    1,    2,    1,    0,    3,    1,    1,
    2,    1,    3,    3,    4,    1,    0,    1,    2,    1,
    3,    3,    2,    1,    1,    1,    4,    1,    3,    1,
    0,    1,    0,    1,    0,    1,    1,    1,    3,    2,
    0,    1,
);
my @yydefred = (                                             0,
    0,   54,    0,   50,    0,    1,    0,    0,   48,    0,
   40,    0,    0,    0,    0,   57,   56,    0,    0,    0,
    3,    0,    6,    0,   11,    0,    0,    0,    0,   49,
    0,   41,   42,    0,   22,    0,    0,    0,    0,   46,
   44,    0,   45,    0,   29,   47,    4,    0,    0,    0,
    0,    7,    8,    9,   10,    0,   25,    0,   52,   43,
    0,    0,    0,    0,   36,    0,    0,   32,   62,    5,
    0,    0,    0,   58,    0,   18,   19,    0,   20,    0,
    0,   28,   60,   21,    0,    0,    0,   34,   33,   59,
    0,    0,   17,   15,   16,    0,   35,   14,
);
my @yydgoto = (                                              5,
    6,    7,   21,    8,   18,   51,   70,    9,   52,   53,
   54,   55,   44,   96,   60,   66,   73,   45,   57,   67,
   68,   10,   11,   46,   74,
);
my @yysindex = (                                             2,
   58,    0,    8,    0,    0,    0,   11,  123,    0,    3,
    0,   59,  123,   19,   73,    0,    0,   92,    7,    7,
    0,  123,    0,  119,    0,   59,  107,  109,  116,    0,
   82,    0,    0,  119,    0,  107,  109,   84,  126,    0,
    0,   90,    0,  132,    0,    0,    0,    7,    7,   10,
  139,    0,    0,    0,    0,  141,    0,  143,    0,    0,
   82,  156,  159,   82,    0,  160,    4,    0,    0,    0,
  171,  158,    6,    0,  123,    0,    0,  123,    0,   10,
   10,    0,    0,    0,  143,  124,  119,    0,    0,    0,
  107,  109,    0,    0,    0,   90,    0,    0,
);
my @yyrindex = (                                           155,
  105,    0,    0,    0,    0,    0,  174,  111,    0,   80,
    0,  105,  138,    0,    0,    0,    0,    0,  161,  145,
    0,  138,    0,    0,    0,  105,    0,    0,    0,    0,
  105,    0,    0,    0,    0,   29,   33,   70,   74,    0,
    0,   46,    0,    0,    0,    0,    0,   45,   45,    0,
   54,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  105,    0,    0,  105,    0,    0,  164,    0,    0,    0,
    0,    0,    0,    0,  138,    0,    0,  138,    0,    0,
  165,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   89,   93,    0,    0,    0,   25,    0,    0,
);
my @yygindex = (                                             0,
   85,    0,  151,    1,  -12,   91,    0,   47,  -18,  -19,
  -17,  157,    0,    0,   83,    0,    0,    0,    0,    0,
   -3,    0,  127,    0,   95,
);
sub constYYTABLESIZE () { 181 }
my @yytable = (                                             30,
   24,   13,    1,    2,   41,   40,   42,   31,    2,   34,
   64,   15,   22,   14,   19,   80,   84,   85,    3,   25,
   20,   81,    4,    3,   51,   51,   22,    4,   23,   23,
   65,   13,   24,   24,   12,   51,   51,   23,   13,   23,
   23,   24,   51,   24,   24,   51,   23,   53,   53,   53,
   24,   53,   53,   61,   61,   37,   51,   51,   23,    2,
    2,   75,   86,   51,   78,   87,   94,   93,   95,   27,
   27,   12,   23,   26,   26,    3,   88,   89,   27,   38,
   27,   27,   26,    2,   26,   26,   26,   27,   23,   23,
   38,   26,   24,   24,   27,   28,   29,   23,   59,   23,
   23,   24,   56,   24,   24,   53,   23,   53,   53,   53,
   24,   53,   53,   55,   55,   55,   48,   53,   49,   35,
   53,   36,   37,   29,   35,   50,   91,   92,   29,   16,
   17,   38,   62,   63,   39,   58,   38,   61,   55,   39,
   55,   55,   55,   72,   39,   32,   33,   53,   53,   53,
   55,   53,   53,   55,   37,   39,   69,   53,   53,   53,
   71,   53,   53,   53,   53,   53,   76,   53,   53,   77,
   79,   82,   83,    2,   30,   31,   47,   97,   98,   90,
   43,
);
my @yycheck = (                                             18,
   13,    1,    1,    2,   24,   24,   24,    1,    2,   22,
    1,    1,   12,    6,   12,   12,   11,   12,   17,    1,
   18,   18,   21,   17,    0,    1,   26,   21,    0,    1,
   21,   31,    0,    1,    6,   11,   12,    9,    6,   11,
   12,    9,   18,   11,   12,    0,   18,    3,    4,    5,
   18,    7,    8,    0,    1,   11,   11,   12,   12,    2,
    2,   61,   75,   18,   64,   78,   86,   86,   86,    0,
    1,   14,   26,    0,    1,   17,   80,   81,    9,    0,
   11,   12,    9,    2,   11,   12,   14,   18,    0,    1,
   11,   18,    0,    1,    3,    4,    5,    9,    9,   11,
   12,    9,   19,   11,   12,    1,   18,    3,    4,    5,
   18,    7,    8,    3,    4,    5,   10,   13,   10,    1,
   16,    3,    4,    5,    1,   10,    3,    4,    5,    7,
    8,   13,   48,   49,   16,   10,   13,    6,    1,   16,
    3,    4,    5,    1,    0,   19,   20,    3,    4,    5,
   13,    7,    8,   16,    0,   11,   18,    3,    4,    5,
   20,    7,    8,    3,    4,    5,   11,    7,    8,   11,
   11,    1,   15,    0,   11,   11,   26,   87,   96,   85,
   24,
);
sub constYYFINAL () { 5 }



sub constYYMAXTOKEN () { 21 }
sub yyclearin { $yychar = -1; }
sub yyerrok { $yyerrflag = 0; }
sub YYERROR { ++$yynerrs; &yy_err_recover; }
sub yy_err_recover
{
  if ($yyerrflag < 3)
  {
    $yyerrflag = 3;
    while (1)
    {
      if (($yyn = $yysindex[$yyss[$yyssp]]) && 
          ($yyn += constYYERRCODE()) >= 0 && 
          $yyn <= $#yycheck && $yycheck[$yyn] == constYYERRCODE())
      {




        $yyss[++$yyssp] = $yystate = $yytable[$yyn];
        $yyvs[++$yyvsp] = $yylval;
        next yyloop;
      }
      else
      {




        return(1) if $yyssp <= 0;
        --$yyssp;
        --$yyvsp;
      }
    }
  }
  else
  {
    return (1) if $yychar == 0;
    $yychar = -1;
    next yyloop;
  }
0;
} # yy_err_recover

sub yyparse
{

  if ($yys = $ENV{'YYDEBUG'})
  {
    $yydebug = int($1) if $yys =~ /^(\d)/;
  }


  $yynerrs = 0;
  $yyerrflag = 0;
  $yychar = (-1);

  $yyssp = 0;
  $yyvsp = 0;
  $yyss[$yyssp] = $yystate = 0;

yyloop: while(1)
  {
    yyreduce: {
      last yyreduce if ($yyn = $yydefred[$yystate]);
      if ($yychar < 0)
      {
        if (($yychar = &yylex) < 0) { $yychar = 0; }
      }
      if (($yyn = $yysindex[$yystate]) && ($yyn += $yychar) >= 0 &&
              $yyn <= $#yycheck && $yycheck[$yyn] == $yychar)
      {




        $yyss[++$yyssp] = $yystate = $yytable[$yyn];
        $yyvs[++$yyvsp] = $yylval;
        $yychar = (-1);
        --$yyerrflag if $yyerrflag > 0;
        next yyloop;
      }
      if (($yyn = $yyrindex[$yystate]) && ($yyn += $yychar) >= 0 &&
            $yyn <= $#yycheck && $yycheck[$yyn] == $yychar)
      {
        $yyn = $yytable[$yyn];
        last yyreduce;
      }
      if (! $yyerrflag) {
        &yyerror('syntax error');
        ++$yynerrs;
      }
      return undef if &yy_err_recover;
    } # yyreduce




    $yym = $yylen[$yyn];
    $yyval = $yyvs[$yyvsp+1-$yym];
    switch:
    {
my $label = "State$yyn";
goto $label if exists $yystate{$label};
last switch;
State1: {
# 107 "parser.y"
{ $yyval = { '' => $yyvs[$yyvsp-0] }; 
last switch;
} }
State3: {
# 112 "parser.y"
{
		  $yyval = { $yyvs[$yyvsp-2], [$yyvs[$yyvsp-0]] };
		
last switch;
} }
State4: {
# 116 "parser.y"
{
		  $yyval=$yyvs[$yyvsp-3];
		  $yyval->{$yyvs[$yyvsp-2]} = [$yyvs[$yyvsp-0]];
		
last switch;
} }
State5: {
# 123 "parser.y"
{
		  $yyvs[$yyvsp-1]->[cTAG] = $yyvs[$yyvsp-3];
		  $yyval = need_explicit($yyvs[$yyvsp-3],$yyvs[$yyvsp-2]) ? explicit($yyvs[$yyvsp-1]) : $yyvs[$yyvsp-1];
		
last switch;
} }
State11: {
# 137 "parser.y"
{
		  @{$yyval = []}[cTYPE,cCHILD] = ('COMPONENTS', $yyvs[$yyvsp-0]);
		
last switch;
} }
State14: {
# 147 "parser.y"
{
		  $yyvs[$yyvsp-1]->[cTAG] = $yyvs[$yyvsp-3];
		  @{$yyval = []}[cTYPE,cCHILD,cLOOP,cOPT] = ($yyvs[$yyvsp-5], [$yyvs[$yyvsp-1]], 1, $yyvs[$yyvsp-0]);
		  $yyval = explicit($yyval) if need_explicit($yyvs[$yyvsp-3],$yyvs[$yyvsp-2]);
		
last switch;
} }
State18: {
# 160 "parser.y"
{
		  @{$yyval = []}[cTYPE,cCHILD] = ('SEQUENCE', $yyvs[$yyvsp-1]);
		
last switch;
} }
State19: {
# 164 "parser.y"
{
		  @{$yyval = []}[cTYPE,cCHILD] = ('SET', $yyvs[$yyvsp-1]);
		
last switch;
} }
State20: {
# 168 "parser.y"
{
		  @{$yyval = []}[cTYPE,cCHILD] = ('CHOICE', $yyvs[$yyvsp-1]);
		
last switch;
} }
State21: {
# 174 "parser.y"
{
		  @{$yyval = []}[cTYPE] = ('ENUM');
		
last switch;
} }
State22: {
# 179 "parser.y"
{ @{$yyval = []}[cTYPE] = $yyvs[$yyvsp-0]; 
last switch;
} }
State23: {
# 180 "parser.y"
{ @{$yyval = []}[cTYPE] = $yyvs[$yyvsp-0]; 
last switch;
} }
State24: {
# 181 "parser.y"
{ @{$yyval = []}[cTYPE] = $yyvs[$yyvsp-0]; 
last switch;
} }
State25: {
# 183 "parser.y"
{
		  @{$yyval = []}[cTYPE,cCHILD,cDEFINE] = ('ANY',undef,$yyvs[$yyvsp-0]);
		
last switch;
} }
State26: {
# 186 "parser.y"
{ @{$yyval = []}[cTYPE] = $yyvs[$yyvsp-0]; 
last switch;
} }
State27: {
# 189 "parser.y"
{ $yyval=undef; 
last switch;
} }
State28: {
# 190 "parser.y"
{ $yyval=$yyvs[$yyvsp-0]; 
last switch;
} }
State30: {
# 196 "parser.y"
{ $yyval = $yyvs[$yyvsp-0]; 
last switch;
} }
State31: {
# 197 "parser.y"
{ $yyval = $yyvs[$yyvsp-1]; 
last switch;
} }
State32: {
# 201 "parser.y"
{
		  $yyval = [ $yyvs[$yyvsp-0] ];
		
last switch;
} }
State33: {
# 205 "parser.y"
{
		  push @{$yyval=$yyvs[$yyvsp-2]}, $yyvs[$yyvsp-0];
		
last switch;
} }
State34: {
# 209 "parser.y"
{
		  push @{$yyval=$yyvs[$yyvsp-2]}, $yyvs[$yyvsp-0];
		
last switch;
} }
State35: {
# 215 "parser.y"
{
		  @{$yyval=$yyvs[$yyvsp-0]}[cVAR,cTAG] = ($yyvs[$yyvsp-3],$yyvs[$yyvsp-2]);
		  $yyval = explicit($yyval) if need_explicit($yyvs[$yyvsp-2],$yyvs[$yyvsp-1]);
		
last switch;
} }
State36: {
# 220 "parser.y"
{
		    @{$yyval=[]}[cTYPE] = 'EXTENSION_MARKER';
		
last switch;
} }
State37: {
# 226 "parser.y"
{ $yyval = []; 
last switch;
} }
State38: {
# 228 "parser.y"
{
		  my $extension = 0;
		  $yyval = [];
		  for my $i (@{$yyvs[$yyvsp-0]}) {
		    $extension = 1 if $i->[cTYPE] eq 'EXTENSION_MARKER';
		    $i->[cEXT] = $i->[cOPT];
		    $i->[cEXT] = 1 if $extension;
		    push @{$yyval}, $i unless $i->[cTYPE] eq 'EXTENSION_MARKER';
		  }
		  my $e = []; $e->[cTYPE] = 'EXTENSION_MARKER';
		  push @{$yyval}, $e if $extension;
		
last switch;
} }
State39: {
# 241 "parser.y"
{
		  my $extension = 0;
		  $yyval = [];
		  for my $i (@{$yyvs[$yyvsp-1]}) {
		    $extension = 1 if $i->[cTYPE] eq 'EXTENSION_MARKER';
		    $i->[cEXT] = $i->[cOPT];
		    $i->[cEXT] = 1 if $extension;
		    push @{$yyval}, $i unless $i->[cTYPE] eq 'EXTENSION_MARKER';
		  }
		  my $e = []; $e->[cTYPE] = 'EXTENSION_MARKER';
		  push @{$yyval}, $e if $extension;
		
last switch;
} }
State40: {
# 256 "parser.y"
{
		  $yyval = [ $yyvs[$yyvsp-0] ];
		
last switch;
} }
State41: {
# 260 "parser.y"
{
		  push @{$yyval=$yyvs[$yyvsp-2]}, $yyvs[$yyvsp-0];
		
last switch;
} }
State42: {
# 264 "parser.y"
{
		  push @{$yyval=$yyvs[$yyvsp-2]}, $yyvs[$yyvsp-0];
		
last switch;
} }
State43: {
# 270 "parser.y"
{
		  @{$yyval=$yyvs[$yyvsp-1]}[cOPT] = ($yyvs[$yyvsp-0]);
		
last switch;
} }
State47: {
# 279 "parser.y"
{
		  @{$yyval=$yyvs[$yyvsp-0]}[cVAR,cTAG] = ($yyvs[$yyvsp-3],$yyvs[$yyvsp-2]);
		  $yyval->[cOPT] = $yyvs[$yyvsp-3] if $yyval->[cOPT];
		  $yyval = explicit($yyval) if need_explicit($yyvs[$yyvsp-2],$yyvs[$yyvsp-1]);
		
last switch;
} }
State49: {
# 286 "parser.y"
{
		  @{$yyval=$yyvs[$yyvsp-0]}[cTAG] = ($yyvs[$yyvsp-2]);
		  $yyval = explicit($yyval) if need_explicit($yyvs[$yyvsp-2],$yyvs[$yyvsp-1]);
		
last switch;
} }
State50: {
# 291 "parser.y"
{
		    @{$yyval=[]}[cTYPE] = 'EXTENSION_MARKER';
		
last switch;
} }
State51: {
# 296 "parser.y"
{ $yyval = undef; 
last switch;
} }
State52: {
# 297 "parser.y"
{ $yyval = 1;     
last switch;
} }
State53: {
# 301 "parser.y"
{ $yyval = undef; 
last switch;
} }
State55: {
# 305 "parser.y"
{ $yyval = undef; 
last switch;
} }
State56: {
# 306 "parser.y"
{ $yyval = 1;     
last switch;
} }
State57: {
# 307 "parser.y"
{ $yyval = 0;     
last switch;
} }
State58: {
# 310 "parser.y"
{
last switch;
} }
State59: {
# 311 "parser.y"
{
last switch;
} }
State60: {
# 314 "parser.y"
{
last switch;
} }
State61: {
# 317 "parser.y"
{
last switch;
} }
State62: {
# 318 "parser.y"
{
last switch;
} }
    } # switch
    $yyssp -= $yym;
    $yystate = $yyss[$yyssp];
    $yyvsp -= $yym;
    $yym = $yylhs[$yyn];
    if ($yystate == 0 && $yym == 0)
    {




      $yystate = constYYFINAL();
      $yyss[++$yyssp] = constYYFINAL();
      $yyvs[++$yyvsp] = $yyval;
      if ($yychar < 0)
      {
        if (($yychar = &yylex) < 0) { $yychar = 0; }
      }
      return $yyvs[$yyvsp] if $yychar == 0;
      next yyloop;
    }
    if (($yyn = $yygindex[$yym]) && ($yyn += $yystate) >= 0 &&
        $yyn <= $#yycheck && $yycheck[$yyn] == $yystate)
    {
        $yystate = $yytable[$yyn];
    } else {
        $yystate = $yydgoto[$yym];
    }




    $yyss[++$yyssp] = $yystate;
    $yyvs[++$yyvsp] = $yyval;
  } # yyloop
} # yyparse
# 322 "parser.y"

my %reserved = (
  'OPTIONAL' 	=> constOPTIONAL(),
  'CHOICE' 	=> constCHOICE(),
  'OF' 		=> constOF(),
  'IMPLICIT' 	=> constIMPLICIT(),
  'EXPLICIT' 	=> constEXPLICIT(),
  'SEQUENCE'    => constSEQUENCE(),
  'SET'         => constSET(),
  'ANY'         => constANY(),
  'ENUM'        => constENUM(),
  'ENUMERATED'  => constENUM(),
  'COMPONENTS'  => constCOMPONENTS(),
  '{'		=> constLBRACE(),
  '}'		=> constRBRACE(),
  ','		=> constCOMMA(),
  '::='         => constASSIGN(),
  'DEFINED'     => constDEFINED(),
  'BY'		=> constBY()
);

my $reserved = join("|", reverse sort grep { /\w/ } keys %reserved);

my %tag_class = (
  APPLICATION => ASN_APPLICATION,
  UNIVERSAL   => ASN_UNIVERSAL,
  PRIVATE     => ASN_PRIVATE,
  CONTEXT     => ASN_CONTEXT,
  ''	      => ASN_CONTEXT # if not specified, its CONTEXT
);

;##
;## This is NOT thread safe !!!!!!
;##

my $pos;
my $last_pos;
my @stacked;

sub parse {
  local(*asn) = \($_[0]);
  $tagdefault = $_[1] eq 'EXPLICIT' ? 1 : 0;
  ($pos,$last_pos,@stacked) = ();

  eval {
    local $SIG{__DIE__};
    compile(verify(yyparse()));
  }
}

sub compile_one {
  my $tree = shift;
  my $ops = shift;
  my $name = shift;
  foreach my $op (@$ops) {
    next unless ref($op) eq 'ARRAY';
    bless $op;
    my $type = $op->[cTYPE];
    if (exists $base_type{$type}) {
      $op->[cTYPE] = $base_type{$type}->[1];
      $op->[cTAG] = defined($op->[cTAG]) ? asn_encode_tag($op->[cTAG]): $base_type{$type}->[0];
    }
    else {
      die "Unknown type '$type'\n" unless exists $tree->{$type};
      my $ref = compile_one(
		  $tree,
		  $tree->{$type},
		  defined($op->[cVAR]) ? $name . "." . $op->[cVAR] : $name
		);
      if (defined($op->[cTAG]) && $ref->[0][cTYPE] == opCHOICE) {
        @{$op}[cTYPE,cCHILD] = (opSEQUENCE,$ref);
      }
      else {
        @{$op}[cTYPE,cCHILD,cLOOP] = @{$ref->[0]}[cTYPE,cCHILD,cLOOP];
      }
      $op->[cTAG] = defined($op->[cTAG]) ? asn_encode_tag($op->[cTAG]): $ref->[0][cTAG];
    }
    $op->[cTAG] |= pack("C",ASN_CONSTRUCTOR)
      if length $op->[cTAG] && ($op->[cTYPE] == opSET || $op->[cTYPE] == opEXPLICIT || $op->[cTYPE] == opSEQUENCE);

    if ($op->[cCHILD]) {
      ;# If we have children we are one of
      ;#  opSET opSEQUENCE opCHOICE opEXPLICIT

      compile_one($tree, $op->[cCHILD], defined($op->[cVAR]) ? $name . "." . $op->[cVAR] : $name);

      ;# If a CHOICE is given a tag, then it must be EXPLICIT
      if ($op->[cTYPE] == opCHOICE && defined($op->[cTAG]) && length($op->[cTAG])) {
	$op = bless explicit($op);
	$op->[cTYPE] = opSEQUENCE;
      }

      if ( @{$op->[cCHILD]} > 1) {
        ;#if ($op->[cTYPE] != opSEQUENCE) {
        ;# Here we need to flatten CHOICEs and check that SET and CHOICE
        ;# do not contain duplicate tags
        ;#}
	if ($op->[cTYPE] == opSET) {
	  ;# In case we do CER encoding we order the SET elements by thier tags
	  my @tags = map { 
	    length($_->[cTAG])
		? $_->[cTAG]
		: $_->[cTYPE] == opCHOICE
			? (sort map { $_->[cTAG] } $_->[cCHILD])[0]
			: ''
	  } @{$op->[cCHILD]};
	  @{$op->[cCHILD]} = @{$op->[cCHILD]}[sort { $tags[$a] cmp $tags[$b] } 0..$#tags];
	}
      }
      else {
	;# A SET of one element can be treated the same as a SEQUENCE
	$op->[cTYPE] = opSEQUENCE if $op->[cTYPE] == opSET;
      }
    }
  }
  $ops;
}

sub compile {
  my $tree = shift;

  ;# The tree should be valid enough to be able to
  ;#  - resolve references
  ;#  - encode tags
  ;#  - verify CHOICEs do not contain duplicate tags

  ;# once references have been resolved, and also due to
  ;# flattening of COMPONENTS, it is possible for an op
  ;# to appear in multiple places. So once an op is
  ;# compiled we bless it. This ensure we dont try to
  ;# compile it again.

  while(my($k,$v) = each %$tree) {
    compile_one($tree,$v,$k);
  }

  $tree;
}

sub verify {
  my $tree = shift or return;
  my $err = "";

  ;# Well it parsed correctly, now we
  ;#  - check references exist
  ;#  - flatten COMPONENTS OF (checking for loops)
  ;#  - check for duplicate var names

  while(my($name,$ops) = each %$tree) {
    my $stash = {};
    my @scope = ();
    my $path = "";
    my $idx = 0;

    while($ops) {
      if ($idx < @$ops) {
	my $op = $ops->[$idx++];
	my $var;
	if (defined ($var = $op->[cVAR])) {
	  
	  $err .= "$name: $path.$var used multiple times\n"
	    if $stash->{$var}++;

	}
	if (defined $op->[cCHILD]) {
	  if (ref $op->[cCHILD]) {
	    push @scope, [$stash, $path, $ops, $idx];
	    if (defined $var) {
	      $stash = {};
	      $path .= "." . $var;
	    }
	    $idx = 0;
	    $ops = $op->[cCHILD];
	  }
	  elsif ($op->[cTYPE] eq 'COMPONENTS') {
	    splice(@$ops,--$idx,1,expand_ops($tree, $op->[cCHILD]));
	  }
          else {
	    die "Internal error\n";
          }
	}
      }
      else {
	my $s = pop @scope
	  or last;
	($stash,$path,$ops,$idx) = @$s;
      }
    }
  }
  die $err if length $err;
  $tree;
}

sub expand_ops {
  my $tree = shift;
  my $want = shift;
  my $seen = shift || { };
  
  die "COMPONENTS OF loop $want\n" if $seen->{$want}++;
  die "Undefined macro $want\n" unless exists $tree->{$want};
  my $ops = $tree->{$want};
  die "Bad macro for COMPUNENTS OF '$want'\n"
    unless @$ops == 1
        && ($ops->[0][cTYPE] eq 'SEQUENCE' || $ops->[0][cTYPE] eq 'SET')
        && ref $ops->[0][cCHILD];
  $ops = $ops->[0][cCHILD];
  for(my $idx = 0 ; $idx < @$ops ; ) {
    my $op = $ops->[$idx++];
    if ($op->[cTYPE] eq 'COMPONENTS') {
      splice(@$ops,--$idx,1,expand_ops($tree, $op->[cCHILD], $seen));
    }
  }

  @$ops;
}

sub _yylex {
  my $ret = &_yylex;
  warn $ret;
  $ret;
}

sub yylex {
  return shift @stacked if @stacked;

  while ($asn =~ /\G(?:
	  (\s+|--[^\n]*)
	|
	  ([,{}]|::=)
	|
	  ($reserved)\b
	|
	  (
	    (?:OCTET|BIT)\s+STRING
	   |
	    OBJECT\s+IDENTIFIER
	   |
	    RELATIVE-OID
	  )\b
	|
	  (\w+(?:-\w+)*)
	|
	    \[\s*
	  (
	   (?:(?:APPLICATION|PRIVATE|UNIVERSAL|CONTEXT)\s+)?
	   \d+
          )
	    \s*\]
	|
	  \((\d+)\)
	|
	  (\.\.\.)
	)/sxgo
  ) {

    ($last_pos,$pos) = ($pos,pos($asn));

    next if defined $1; # comment or whitespace

    if (defined $2 or defined $3) {
      my $ret = $+;

      # A comma is not required after a '}' so to aid the
      # parser we insert a fake token after any '}'
      if ($ret eq '}') {
        my $p   = pos($asn);
        my @tmp = @stacked;
        @stacked = ();
        pos($asn) = $p if yylex() != constCOMMA();    # swallow it
        @stacked = (@tmp, constPOSTRBRACE());
      }

      return $reserved{$yylval = $ret};
    }

    if (defined $4) {
      ($yylval = $+) =~ s/\s+/_/g;
      return constWORD();
    }

    if (defined $5) {
      $yylval = $+;
      return constWORD();
    }

    if (defined $6) {
      my($class,$num) = ($+ =~ /^([A-Z]*)\s*(\d+)$/);
      $yylval = asn_tag($tag_class{$class}, $num); 
      return constCLASS();
    }

    if (defined $7) {
      $yylval = $+;
      return constNUMBER();
    }

    if (defined $8) {
      return constEXTENSION_MARKER();
    }

    die "Internal error\n";

  }

  die "Parse error before ",substr($asn,$pos,40),"\n"
    unless $pos == length($asn);

  0
}

sub yyerror {
  die @_," ",substr($asn,$last_pos,40),"\n";
}

1;

%yystate = ('State51','','State34','','State11','','State33','','State24',
'','State47','','State40','','State31','','State37','','State23','',
'State22','','State21','','State57','','State39','','State56','','State20',
'','State25','','State38','','State62','','State14','','State19','',
'State5','','State53','','State26','','State27','','State50','','State36',
'','State4','','State3','','State32','','State49','','State43','','State30',
'','State35','','State52','','State55','','State42','','State28','',
'State58','','State61','','State41','','State18','','State59','','State1',
'','State60','');

1;
