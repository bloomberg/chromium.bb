# OLE::Storage_Lite
#  by Kawai, Takanori (Hippo2000) 2000.11.4, 8, 14
# This Program is Still ALPHA version.
#//////////////////////////////////////////////////////////////////////////////
# OLE::Storage_Lite::PPS Object
#//////////////////////////////////////////////////////////////////////////////
#==============================================================================
# OLE::Storage_Lite::PPS
#==============================================================================
package OLE::Storage_Lite::PPS;
require Exporter;
use strict;
use vars qw($VERSION @ISA);
@ISA = qw(Exporter);
$VERSION = '0.19';

#------------------------------------------------------------------------------
# new (OLE::Storage_Lite::PPS)
#------------------------------------------------------------------------------
sub new ($$$$$$$$$$;$$) {
#1. Constructor for General Usage
  my($sClass, $iNo, $sNm, $iType, $iPrev, $iNext, $iDir,
     $raTime1st, $raTime2nd, $iStart, $iSize, $sData, $raChild) = @_;

  if($iType == OLE::Storage_Lite::PpsType_File()) { #FILE
    return OLE::Storage_Lite::PPS::File->_new
        ($iNo, $sNm, $iType, $iPrev, $iNext, $iDir, $raTime1st, $raTime2nd,
         $iStart, $iSize, $sData, $raChild);
  }
  elsif($iType == OLE::Storage_Lite::PpsType_Dir()) { #DIRECTRY
    return OLE::Storage_Lite::PPS::Dir->_new
        ($iNo, $sNm, $iType, $iPrev, $iNext, $iDir, $raTime1st, $raTime2nd,
         $iStart, $iSize, $sData, $raChild);
  }
  elsif($iType == OLE::Storage_Lite::PpsType_Root()) { #ROOT
    return OLE::Storage_Lite::PPS::Root->_new
        ($iNo, $sNm, $iType, $iPrev, $iNext, $iDir, $raTime1st, $raTime2nd,
         $iStart, $iSize, $sData, $raChild);
  }
  else {
    die "Error PPS:$iType $sNm\n";
  }
}
#------------------------------------------------------------------------------
# _new (OLE::Storage_Lite::PPS)
#   for OLE::Storage_Lite
#------------------------------------------------------------------------------
sub _new ($$$$$$$$$$$;$$) {
  my($sClass, $iNo, $sNm, $iType, $iPrev, $iNext, $iDir,
        $raTime1st, $raTime2nd, $iStart, $iSize, $sData, $raChild) = @_;
#1. Constructor for OLE::Storage_Lite
  my $oThis = {
    No   => $iNo,
    Name => $sNm,
    Type => $iType,
    PrevPps => $iPrev,
    NextPps => $iNext,
    DirPps => $iDir,
    Time1st => $raTime1st,
    Time2nd => $raTime2nd,
    StartBlock => $iStart,
    Size       => $iSize,
    Data       => $sData,
    Child      => $raChild,
  };
  bless $oThis, $sClass;
  return $oThis;
}
#------------------------------------------------------------------------------
# _DataLen (OLE::Storage_Lite::PPS)
# Check for update
#------------------------------------------------------------------------------
sub _DataLen($) {
    my($oSelf) =@_;
    return 0 unless(defined($oSelf->{Data}));
    return ($oSelf->{_PPS_FILE})?
        ($oSelf->{_PPS_FILE}->stat())[7] : length($oSelf->{Data});
}
#------------------------------------------------------------------------------
# _makeSmallData (OLE::Storage_Lite::PPS)
#------------------------------------------------------------------------------
sub _makeSmallData($$$) {
  my($oThis, $aList, $rhInfo) = @_;
  my ($sRes);
  my $FILE = $rhInfo->{_FILEH_};
  my $iSmBlk = 0;

  foreach my $oPps (@$aList) {
#1. Make SBD, small data string
  if($oPps->{Type}==OLE::Storage_Lite::PpsType_File()) {
    next if($oPps->{Size}<=0);
    if($oPps->{Size} < $rhInfo->{_SMALL_SIZE}) {
      my $iSmbCnt = int($oPps->{Size} / $rhInfo->{_SMALL_BLOCK_SIZE})
                    + (($oPps->{Size} % $rhInfo->{_SMALL_BLOCK_SIZE})? 1: 0);
      #1.1 Add to SBD
      for (my $i = 0; $i<($iSmbCnt-1); $i++) {
            print {$FILE} (pack("V", $i+$iSmBlk+1));
      }
      print {$FILE} (pack("V", -2));

      #1.2 Add to Data String(this will be written for RootEntry)
      #Check for update
      if($oPps->{_PPS_FILE}) {
        my $sBuff;
        $oPps->{_PPS_FILE}->seek(0, 0); #To The Top
        while($oPps->{_PPS_FILE}->read($sBuff, 4096)) {
            $sRes .= $sBuff;
        }
      }
      else {
        $sRes .= $oPps->{Data};
      }
      $sRes .= ("\x00" x
        ($rhInfo->{_SMALL_BLOCK_SIZE} - ($oPps->{Size}% $rhInfo->{_SMALL_BLOCK_SIZE})))
        if($oPps->{Size}% $rhInfo->{_SMALL_BLOCK_SIZE});
      #1.3 Set for PPS
      $oPps->{StartBlock} = $iSmBlk;
      $iSmBlk += $iSmbCnt;
    }
  }
  }
  my $iSbCnt = int($rhInfo->{_BIG_BLOCK_SIZE}/ OLE::Storage_Lite::LongIntSize());
  print {$FILE} (pack("V", -1) x ($iSbCnt - ($iSmBlk % $iSbCnt)))
    if($iSmBlk  % $iSbCnt);
#2. Write SBD with adjusting length for block
  return $sRes;
}
#------------------------------------------------------------------------------
# _savePpsWk (OLE::Storage_Lite::PPS)
#------------------------------------------------------------------------------
sub _savePpsWk($$)
{
  my($oThis, $rhInfo) = @_;
#1. Write PPS
  my $FILE = $rhInfo->{_FILEH_};
  print {$FILE} (
            $oThis->{Name}
            . ("\x00" x (64 - length($oThis->{Name})))  #64
            , pack("v", length($oThis->{Name}) + 2)     #66
            , pack("c", $oThis->{Type})         #67
            , pack("c", 0x00) #UK               #68
            , pack("V", $oThis->{PrevPps}) #Prev        #72
            , pack("V", $oThis->{NextPps}) #Next        #76
            , pack("V", $oThis->{DirPps})  #Dir     #80
            , "\x00\x09\x02\x00"                #84
            , "\x00\x00\x00\x00"                #88
            , "\xc0\x00\x00\x00"                #92
            , "\x00\x00\x00\x46"                #96
            , "\x00\x00\x00\x00"                #100
            , OLE::Storage_Lite::LocalDate2OLE($oThis->{Time1st})       #108
            , OLE::Storage_Lite::LocalDate2OLE($oThis->{Time2nd})       #116
            , pack("V", defined($oThis->{StartBlock})?
                      $oThis->{StartBlock}:0)       #116
            , pack("V", defined($oThis->{Size})?
                 $oThis->{Size} : 0)            #124
            , pack("V", 0),                  #128
        );
}

#//////////////////////////////////////////////////////////////////////////////
# OLE::Storage_Lite::PPS::Root Object
#//////////////////////////////////////////////////////////////////////////////
#==============================================================================
# OLE::Storage_Lite::PPS::Root
#==============================================================================
package OLE::Storage_Lite::PPS::Root;
require Exporter;
use strict;
use IO::File;
use IO::Handle;
use Fcntl;
use vars qw($VERSION @ISA);
@ISA = qw(OLE::Storage_Lite::PPS Exporter);
$VERSION = '0.19';
sub _savePpsSetPnt($$$);
sub _savePpsSetPnt2($$$);
#------------------------------------------------------------------------------
# new (OLE::Storage_Lite::PPS::Root)
#------------------------------------------------------------------------------
sub new ($;$$$) {
    my($sClass, $raTime1st, $raTime2nd, $raChild) = @_;
    OLE::Storage_Lite::PPS::_new(
        $sClass,
        undef,
        OLE::Storage_Lite::Asc2Ucs('Root Entry'),
        5,
        undef,
        undef,
        undef,
        $raTime1st,
        $raTime2nd,
        undef,
        undef,
        undef,
        $raChild);
}
#------------------------------------------------------------------------------
# save (OLE::Storage_Lite::PPS::Root)
#------------------------------------------------------------------------------
sub save($$;$$) {
  my($oThis, $sFile, $bNoAs, $rhInfo) = @_;
  #0.Initial Setting for saving
  $rhInfo = {} unless($rhInfo);
  $rhInfo->{_BIG_BLOCK_SIZE}  = 2**
                (($rhInfo->{_BIG_BLOCK_SIZE})?
                    _adjust2($rhInfo->{_BIG_BLOCK_SIZE})  : 9);
  $rhInfo->{_SMALL_BLOCK_SIZE}= 2 **
                (($rhInfo->{_SMALL_BLOCK_SIZE})?
                    _adjust2($rhInfo->{_SMALL_BLOCK_SIZE}): 6);
  $rhInfo->{_SMALL_SIZE} = 0x1000;
  $rhInfo->{_PPS_SIZE} = 0x80;

  my $closeFile = 1;

  #1.Open File
  #1.1 $sFile is Ref of scalar
  if(ref($sFile) eq 'SCALAR') {
    require IO::Scalar;
    my $oIo = new IO::Scalar $sFile, O_WRONLY;
    $rhInfo->{_FILEH_} = $oIo;
  }
  #1.1.1 $sFile is a IO::Scalar object
  # Now handled as a filehandle ref below.

  #1.2 $sFile is a IO::Handle object
  elsif(UNIVERSAL::isa($sFile, 'IO::Handle')) {
    # Not all filehandles support binmode() so try it in an eval.
    eval{ binmode $sFile };
    $rhInfo->{_FILEH_} = $sFile;
  }
  #1.3 $sFile is a simple filename string
  elsif(!ref($sFile)) {
    if($sFile ne '-') {
        my $oIo = new IO::File;
        $oIo->open(">$sFile") || return undef;
        binmode($oIo);
        $rhInfo->{_FILEH_} = $oIo;
    }
    else {
        my $oIo = new IO::Handle;
        $oIo->fdopen(fileno(STDOUT),"w") || return undef;
        binmode($oIo);
        $rhInfo->{_FILEH_} = $oIo;
    }
  }
  #1.4 Assume that if $sFile is a ref then it is a valid filehandle
  else {
    # Not all filehandles support binmode() so try it in an eval.
    eval{ binmode $sFile };
    $rhInfo->{_FILEH_} = $sFile;
    # Caller controls filehandle closing
    $closeFile = 0;
  }

  my $iBlk = 0;
  #1. Make an array of PPS (for Save)
  my @aList=();
  if($bNoAs) {
    _savePpsSetPnt2([$oThis], \@aList, $rhInfo);
  }
  else {
    _savePpsSetPnt([$oThis], \@aList, $rhInfo);
  }
  my ($iSBDcnt, $iBBcnt, $iPPScnt) = $oThis->_calcSize(\@aList, $rhInfo);

  #2.Save Header
  $oThis->_saveHeader($rhInfo, $iSBDcnt, $iBBcnt, $iPPScnt);

  #3.Make Small Data string (write SBD)
  my $sSmWk = $oThis->_makeSmallData(\@aList, $rhInfo);
  $oThis->{Data} = $sSmWk;  #Small Datas become RootEntry Data

  #4. Write BB
  my $iBBlk = $iSBDcnt;
  $oThis->_saveBigData(\$iBBlk, \@aList, $rhInfo);

  #5. Write PPS
  $oThis->_savePps(\@aList, $rhInfo);

  #6. Write BD and BDList and Adding Header informations
  $oThis->_saveBbd($iSBDcnt, $iBBcnt, $iPPScnt,  $rhInfo);

  #7.Close File
  return $rhInfo->{_FILEH_}->close if $closeFile;
}
#------------------------------------------------------------------------------
# _calcSize (OLE::Storage_Lite::PPS)
#------------------------------------------------------------------------------
sub _calcSize($$)
{
  my($oThis, $raList, $rhInfo) = @_;

#0. Calculate Basic Setting
  my ($iSBDcnt, $iBBcnt, $iPPScnt) = (0,0,0);
  my $iSmallLen = 0;
  my $iSBcnt = 0;
  foreach my $oPps (@$raList) {
      if($oPps->{Type}==OLE::Storage_Lite::PpsType_File()) {
        $oPps->{Size} = $oPps->_DataLen();  #Mod
        if($oPps->{Size} < $rhInfo->{_SMALL_SIZE}) {
          $iSBcnt += int($oPps->{Size} / $rhInfo->{_SMALL_BLOCK_SIZE})
                          + (($oPps->{Size} % $rhInfo->{_SMALL_BLOCK_SIZE})? 1: 0);
        }
        else {
          $iBBcnt +=
            (int($oPps->{Size}/ $rhInfo->{_BIG_BLOCK_SIZE}) +
                (($oPps->{Size}% $rhInfo->{_BIG_BLOCK_SIZE})? 1: 0));
        }
      }
  }
  $iSmallLen = $iSBcnt * $rhInfo->{_SMALL_BLOCK_SIZE};
  my $iSlCnt = int($rhInfo->{_BIG_BLOCK_SIZE}/ OLE::Storage_Lite::LongIntSize());
  $iSBDcnt = int($iSBcnt / $iSlCnt)+ (($iSBcnt % $iSlCnt)? 1:0);
  $iBBcnt +=  (int($iSmallLen/ $rhInfo->{_BIG_BLOCK_SIZE}) +
                (( $iSmallLen% $rhInfo->{_BIG_BLOCK_SIZE})? 1: 0));
  my $iCnt = scalar(@$raList);
  my $iBdCnt = $rhInfo->{_BIG_BLOCK_SIZE}/OLE::Storage_Lite::PpsSize();
  $iPPScnt = (int($iCnt/$iBdCnt) + (($iCnt % $iBdCnt)? 1: 0));
  return ($iSBDcnt, $iBBcnt, $iPPScnt);
}
#------------------------------------------------------------------------------
# _adjust2 (OLE::Storage_Lite::PPS::Root)
#------------------------------------------------------------------------------
sub _adjust2($) {
  my($i2) = @_;
  my $iWk;
  $iWk = log($i2)/log(2);
  return ($iWk > int($iWk))? int($iWk)+1:$iWk;
}
#------------------------------------------------------------------------------
# _saveHeader (OLE::Storage_Lite::PPS::Root)
#------------------------------------------------------------------------------
sub _saveHeader($$$$$) {
  my($oThis, $rhInfo, $iSBDcnt, $iBBcnt, $iPPScnt) = @_;
  my $FILE = $rhInfo->{_FILEH_};

#0. Calculate Basic Setting
  my $iBlCnt = $rhInfo->{_BIG_BLOCK_SIZE} / OLE::Storage_Lite::LongIntSize();
  my $i1stBdL = int(($rhInfo->{_BIG_BLOCK_SIZE} - 0x4C) / OLE::Storage_Lite::LongIntSize());
  my $i1stBdMax = $i1stBdL * $iBlCnt  - $i1stBdL;
  my $iBdExL = 0;
  my $iAll = $iBBcnt + $iPPScnt + $iSBDcnt;
  my $iAllW = $iAll;
  my $iBdCntW = int($iAllW / $iBlCnt) + (($iAllW % $iBlCnt)? 1: 0);
  my $iBdCnt = int(($iAll + $iBdCntW) / $iBlCnt) + ((($iAllW+$iBdCntW) % $iBlCnt)? 1: 0);
  my $i;

  if ($iBdCnt > $i1stBdL) {
    #0.1 Calculate BD count
    $iBlCnt--; #the BlCnt is reduced in the count of the last sect is used for a pointer the next Bl
    my $iBBleftover = $iAll - $i1stBdMax;

    if ($iAll >$i1stBdMax) {
      while(1) {
        $iBdCnt = int(($iBBleftover) / $iBlCnt) + ((($iBBleftover) % $iBlCnt)? 1: 0);
        $iBdExL = int(($iBdCnt) / $iBlCnt) + ((($iBdCnt) % $iBlCnt)? 1: 0);
        $iBBleftover = $iBBleftover + $iBdExL;
        last if($iBdCnt == (int(($iBBleftover) / $iBlCnt) + ((($iBBleftover) % $iBlCnt)? 1: 0)));
      }
    }
    $iBdCnt += $i1stBdL;
    #print "iBdCnt = $iBdCnt \n";
  }
#1.Save Header
  print {$FILE} (
            "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1"
            , "\x00\x00\x00\x00" x 4
            , pack("v", 0x3b)
            , pack("v", 0x03)
            , pack("v", -2)
            , pack("v", 9)
            , pack("v", 6)
            , pack("v", 0)
            , "\x00\x00\x00\x00" x 2
            , pack("V", $iBdCnt),
            , pack("V", $iBBcnt+$iSBDcnt), #ROOT START
            , pack("V", 0)
            , pack("V", 0x1000)
            , pack("V", $iSBDcnt ? 0 : -2)                  #Small Block Depot
            , pack("V", $iSBDcnt)
    );
#2. Extra BDList Start, Count
  if($iAll <= $i1stBdMax) {
    print {$FILE} (
                pack("V", -2),      #Extra BDList Start
                pack("V", 0),       #Extra BDList Count
        );
  }
  else {
    print {$FILE} (
            pack("V", $iAll+$iBdCnt),
            pack("V", $iBdExL),
        );
  }

#3. BDList
    for($i=0; $i<$i1stBdL and $i < $iBdCnt; $i++) {
        print {$FILE} (pack("V", $iAll+$i));
    }
    print {$FILE} ((pack("V", -1)) x($i1stBdL-$i)) if($i<$i1stBdL);
}
#------------------------------------------------------------------------------
# _saveBigData (OLE::Storage_Lite::PPS)
#------------------------------------------------------------------------------
sub _saveBigData($$$$) {
  my($oThis, $iStBlk, $raList, $rhInfo) = @_;
  my $iRes = 0;
  my $FILE = $rhInfo->{_FILEH_};

#1.Write Big (ge 0x1000) Data into Block
  foreach my $oPps (@$raList) {
    if($oPps->{Type}!=OLE::Storage_Lite::PpsType_Dir()) {
#print "PPS: $oPps DEF:", defined($oPps->{Data}), "\n";
        $oPps->{Size} = $oPps->_DataLen();  #Mod
        if(($oPps->{Size} >= $rhInfo->{_SMALL_SIZE}) ||
            (($oPps->{Type} == OLE::Storage_Lite::PpsType_Root()) && defined($oPps->{Data}))) {
            #1.1 Write Data
            #Check for update
            if($oPps->{_PPS_FILE}) {
                my $sBuff;
                my $iLen = 0;
                $oPps->{_PPS_FILE}->seek(0, 0); #To The Top
                while($oPps->{_PPS_FILE}->read($sBuff, 4096)) {
                    $iLen += length($sBuff);
                    print {$FILE} ($sBuff);           #Check for update
                }
            }
            else {
                print {$FILE} ($oPps->{Data});
            }
            print {$FILE} (
                        "\x00" x
                        ($rhInfo->{_BIG_BLOCK_SIZE} -
                            ($oPps->{Size} % $rhInfo->{_BIG_BLOCK_SIZE}))
                    ) if ($oPps->{Size} % $rhInfo->{_BIG_BLOCK_SIZE});
            #1.2 Set For PPS
            $oPps->{StartBlock} = $$iStBlk;
            $$iStBlk +=
                    (int($oPps->{Size}/ $rhInfo->{_BIG_BLOCK_SIZE}) +
                        (($oPps->{Size}% $rhInfo->{_BIG_BLOCK_SIZE})? 1: 0));
        }
    }
  }
}
#------------------------------------------------------------------------------
# _savePps (OLE::Storage_Lite::PPS::Root)
#------------------------------------------------------------------------------
sub _savePps($$$)
{
  my($oThis, $raList, $rhInfo) = @_;
#0. Initial
  my $FILE = $rhInfo->{_FILEH_};
#2. Save PPS
  foreach my $oItem (@$raList) {
      $oItem->_savePpsWk($rhInfo);
  }
#3. Adjust for Block
  my $iCnt = scalar(@$raList);
  my $iBCnt = $rhInfo->{_BIG_BLOCK_SIZE} / $rhInfo->{_PPS_SIZE};
  print {$FILE} ("\x00" x (($iBCnt - ($iCnt % $iBCnt)) * $rhInfo->{_PPS_SIZE}))
        if($iCnt % $iBCnt);
  return int($iCnt / $iBCnt) + (($iCnt % $iBCnt)? 1: 0);
}
#------------------------------------------------------------------------------
# _savePpsSetPnt2 (OLE::Storage_Lite::PPS::Root)
#  For Test
#------------------------------------------------------------------------------
sub _savePpsSetPnt2($$$)
{
  my($aThis, $raList, $rhInfo) = @_;
#1. make Array as Children-Relations
#1.1 if No Children
  if($#$aThis < 0) {
      return 0xFFFFFFFF;
  }
  elsif($#$aThis == 0) {
#1.2 Just Only one
      push @$raList, $aThis->[0];
      $aThis->[0]->{No} = $#$raList;
      $aThis->[0]->{PrevPps} = 0xFFFFFFFF;
      $aThis->[0]->{NextPps} = 0xFFFFFFFF;
      $aThis->[0]->{DirPps} = _savePpsSetPnt2($aThis->[0]->{Child}, $raList, $rhInfo);
      return $aThis->[0]->{No};
  }
  else {
#1.3 Array
      my $iCnt = $#$aThis + 1;
#1.3.1 Define Center
      my $iPos = 0; #int($iCnt/ 2);     #$iCnt

      my @aWk = @$aThis;
      my @aPrev = ($#$aThis > 1)? splice(@aWk, 1, 1) : (); #$iPos);
      my @aNext = splice(@aWk, 1); #, $iCnt - $iPos -1);
      $aThis->[$iPos]->{PrevPps} = _savePpsSetPnt2(
            \@aPrev, $raList, $rhInfo);
      push @$raList, $aThis->[$iPos];
      $aThis->[$iPos]->{No} = $#$raList;

#1.3.2 Devide a array into Previous,Next
      $aThis->[$iPos]->{NextPps} = _savePpsSetPnt2(
            \@aNext, $raList, $rhInfo);
      $aThis->[$iPos]->{DirPps} = _savePpsSetPnt2($aThis->[$iPos]->{Child}, $raList, $rhInfo);
      return $aThis->[$iPos]->{No};
  }
}
#------------------------------------------------------------------------------
# _savePpsSetPnt2 (OLE::Storage_Lite::PPS::Root)
#  For Test
#------------------------------------------------------------------------------
sub _savePpsSetPnt2s($$$)
{
  my($aThis, $raList, $rhInfo) = @_;
#1. make Array as Children-Relations
#1.1 if No Children
  if($#$aThis < 0) {
      return 0xFFFFFFFF;
  }
  elsif($#$aThis == 0) {
#1.2 Just Only one
      push @$raList, $aThis->[0];
      $aThis->[0]->{No} = $#$raList;
      $aThis->[0]->{PrevPps} = 0xFFFFFFFF;
      $aThis->[0]->{NextPps} = 0xFFFFFFFF;
      $aThis->[0]->{DirPps} = _savePpsSetPnt2($aThis->[0]->{Child}, $raList, $rhInfo);
      return $aThis->[0]->{No};
  }
  else {
#1.3 Array
      my $iCnt = $#$aThis + 1;
#1.3.1 Define Center
      my $iPos = 0; #int($iCnt/ 2);     #$iCnt
      push @$raList, $aThis->[$iPos];
      $aThis->[$iPos]->{No} = $#$raList;
      my @aWk = @$aThis;
#1.3.2 Devide a array into Previous,Next
      my @aPrev = splice(@aWk, 0, $iPos);
      my @aNext = splice(@aWk, 1, $iCnt - $iPos -1);
      $aThis->[$iPos]->{PrevPps} = _savePpsSetPnt2(
            \@aPrev, $raList, $rhInfo);
      $aThis->[$iPos]->{NextPps} = _savePpsSetPnt2(
            \@aNext, $raList, $rhInfo);
      $aThis->[$iPos]->{DirPps} = _savePpsSetPnt2($aThis->[$iPos]->{Child}, $raList, $rhInfo);
      return $aThis->[$iPos]->{No};
  }
}
#------------------------------------------------------------------------------
# _savePpsSetPnt (OLE::Storage_Lite::PPS::Root)
#------------------------------------------------------------------------------
sub _savePpsSetPnt($$$)
{
  my($aThis, $raList, $rhInfo) = @_;
#1. make Array as Children-Relations
#1.1 if No Children
  if($#$aThis < 0) {
      return 0xFFFFFFFF;
  }
  elsif($#$aThis == 0) {
#1.2 Just Only one
      push @$raList, $aThis->[0];
      $aThis->[0]->{No} = $#$raList;
      $aThis->[0]->{PrevPps} = 0xFFFFFFFF;
      $aThis->[0]->{NextPps} = 0xFFFFFFFF;
      $aThis->[0]->{DirPps} = _savePpsSetPnt($aThis->[0]->{Child}, $raList, $rhInfo);
      return $aThis->[0]->{No};
  }
  else {
#1.3 Array
      my $iCnt = $#$aThis + 1;
#1.3.1 Define Center
      my $iPos = int($iCnt/ 2);     #$iCnt
      push @$raList, $aThis->[$iPos];
      $aThis->[$iPos]->{No} = $#$raList;
      my @aWk = @$aThis;
#1.3.2 Devide a array into Previous,Next
      my @aPrev = splice(@aWk, 0, $iPos);
      my @aNext = splice(@aWk, 1, $iCnt - $iPos -1);
      $aThis->[$iPos]->{PrevPps} = _savePpsSetPnt(
            \@aPrev, $raList, $rhInfo);
      $aThis->[$iPos]->{NextPps} = _savePpsSetPnt(
            \@aNext, $raList, $rhInfo);
      $aThis->[$iPos]->{DirPps} = _savePpsSetPnt($aThis->[$iPos]->{Child}, $raList, $rhInfo);
      return $aThis->[$iPos]->{No};
  }
}
#------------------------------------------------------------------------------
# _savePpsSetPnt (OLE::Storage_Lite::PPS::Root)
#------------------------------------------------------------------------------
sub _savePpsSetPnt1($$$)
{
  my($aThis, $raList, $rhInfo) = @_;
#1. make Array as Children-Relations
#1.1 if No Children
  if($#$aThis < 0) {
      return 0xFFFFFFFF;
  }
  elsif($#$aThis == 0) {
#1.2 Just Only one
      push @$raList, $aThis->[0];
      $aThis->[0]->{No} = $#$raList;
      $aThis->[0]->{PrevPps} = 0xFFFFFFFF;
      $aThis->[0]->{NextPps} = 0xFFFFFFFF;
      $aThis->[0]->{DirPps} = _savePpsSetPnt($aThis->[0]->{Child}, $raList, $rhInfo);
      return $aThis->[0]->{No};
  }
  else {
#1.3 Array
      my $iCnt = $#$aThis + 1;
#1.3.1 Define Center
      my $iPos = int($iCnt/ 2);     #$iCnt
      push @$raList, $aThis->[$iPos];
      $aThis->[$iPos]->{No} = $#$raList;
      my @aWk = @$aThis;
#1.3.2 Devide a array into Previous,Next
      my @aPrev = splice(@aWk, 0, $iPos);
      my @aNext = splice(@aWk, 1, $iCnt - $iPos -1);
      $aThis->[$iPos]->{PrevPps} = _savePpsSetPnt(
            \@aPrev, $raList, $rhInfo);
      $aThis->[$iPos]->{NextPps} = _savePpsSetPnt(
            \@aNext, $raList, $rhInfo);
      $aThis->[$iPos]->{DirPps} = _savePpsSetPnt($aThis->[$iPos]->{Child}, $raList, $rhInfo);
      return $aThis->[$iPos]->{No};
  }
}
#------------------------------------------------------------------------------
# _saveBbd (OLE::Storage_Lite)
#------------------------------------------------------------------------------
sub _saveBbd($$$$)
{
  my($oThis, $iSbdSize, $iBsize, $iPpsCnt, $rhInfo) = @_;
  my $FILE = $rhInfo->{_FILEH_};
#0. Calculate Basic Setting
  my $iBbCnt = $rhInfo->{_BIG_BLOCK_SIZE} / OLE::Storage_Lite::LongIntSize();
  my $iBlCnt = $iBbCnt - 1;
  my $i1stBdL = int(($rhInfo->{_BIG_BLOCK_SIZE} - 0x4C) / OLE::Storage_Lite::LongIntSize());
  my $i1stBdMax = $i1stBdL * $iBbCnt  - $i1stBdL;
  my $iBdExL = 0;
  my $iAll = $iBsize + $iPpsCnt + $iSbdSize;
  my $iAllW = $iAll;
  my $iBdCntW = int($iAllW / $iBbCnt) + (($iAllW % $iBbCnt)? 1: 0);
  my $iBdCnt = 0;
  my $i;
#0.1 Calculate BD count
  my $iBBleftover = $iAll - $i1stBdMax;
  if ($iAll >$i1stBdMax) {

    while(1) {
      $iBdCnt = int(($iBBleftover) / $iBlCnt) + ((($iBBleftover) % $iBlCnt)? 1: 0);
      $iBdExL = int(($iBdCnt) / $iBlCnt) + ((($iBdCnt) % $iBlCnt)? 1: 0);
      $iBBleftover = $iBBleftover + $iBdExL;
      last if($iBdCnt == (int(($iBBleftover) / $iBlCnt) + ((($iBBleftover) % $iBlCnt)? 1: 0)));
    }
  }
  $iAllW += $iBdExL;
  $iBdCnt += $i1stBdL;
  #print "iBdCnt = $iBdCnt \n";

#1. Making BD
#1.1 Set for SBD
  if($iSbdSize > 0) {
    for ($i = 0; $i<($iSbdSize-1); $i++) {
      print {$FILE} (pack("V", $i+1));
    }
    print {$FILE} (pack("V", -2));
  }
#1.2 Set for B
  for ($i = 0; $i<($iBsize-1); $i++) {
      print {$FILE} (pack("V", $i+$iSbdSize+1));
  }
  print {$FILE} (pack("V", -2));

#1.3 Set for PPS
  for ($i = 0; $i<($iPpsCnt-1); $i++) {
      print {$FILE} (pack("V", $i+$iSbdSize+$iBsize+1));
  }
  print {$FILE} (pack("V", -2));
#1.4 Set for BBD itself ( 0xFFFFFFFD : BBD)
  for($i=0; $i<$iBdCnt;$i++) {
    print {$FILE} (pack("V", 0xFFFFFFFD));
  }
#1.5 Set for ExtraBDList
  for($i=0; $i<$iBdExL;$i++) {
    print {$FILE} (pack("V", 0xFFFFFFFC));
  }
#1.6 Adjust for Block
  print {$FILE} (pack("V", -1) x ($iBbCnt - (($iAllW + $iBdCnt) % $iBbCnt)))
                if(($iAllW + $iBdCnt) % $iBbCnt);
#2.Extra BDList
  if($iBdCnt > $i1stBdL)  {
    my $iN=0;
    my $iNb=0;
    for($i=$i1stBdL;$i<$iBdCnt; $i++, $iN++) {
      if($iN>=($iBbCnt-1)) {
          $iN = 0;
          $iNb++;
          print {$FILE} (pack("V", $iAll+$iBdCnt+$iNb));
      }
      print {$FILE} (pack("V", $iBsize+$iSbdSize+$iPpsCnt+$i));
    }
    print {$FILE} (pack("V", -1) x (($iBbCnt-1) - (($iBdCnt-$i1stBdL) % ($iBbCnt-1))))
        if(($iBdCnt-$i1stBdL) % ($iBbCnt-1));
    print {$FILE} (pack("V", -2));
  }
}

#//////////////////////////////////////////////////////////////////////////////
# OLE::Storage_Lite::PPS::File Object
#//////////////////////////////////////////////////////////////////////////////
#==============================================================================
# OLE::Storage_Lite::PPS::File
#==============================================================================
package OLE::Storage_Lite::PPS::File;
require Exporter;
use strict;
use vars qw($VERSION @ISA);
@ISA = qw(OLE::Storage_Lite::PPS Exporter);
$VERSION = '0.19';
#------------------------------------------------------------------------------
# new (OLE::Storage_Lite::PPS::File)
#------------------------------------------------------------------------------
sub new ($$$) {
  my($sClass, $sNm, $sData) = @_;
    OLE::Storage_Lite::PPS::_new(
        $sClass,
        undef,
        $sNm,
        2,
        undef,
        undef,
        undef,
        undef,
        undef,
        undef,
        undef,
        $sData,
        undef);
}
#------------------------------------------------------------------------------
# newFile (OLE::Storage_Lite::PPS::File)
#------------------------------------------------------------------------------
sub newFile ($$;$) {
    my($sClass, $sNm, $sFile) = @_;
    my $oSelf =
    OLE::Storage_Lite::PPS::_new(
        $sClass,
        undef,
        $sNm,
        2,
        undef,
        undef,
        undef,
        undef,
        undef,
        undef,
        undef,
        '',
        undef);
#
    if((!defined($sFile)) or ($sFile eq '')) {
        $oSelf->{_PPS_FILE} = IO::File->new_tmpfile();
    }
    elsif(UNIVERSAL::isa($sFile, 'IO::Handle')) {
        $oSelf->{_PPS_FILE} = $sFile;
    }
    elsif(!ref($sFile)) {
        #File Name
        $oSelf->{_PPS_FILE} = new IO::File;
        return undef unless($oSelf->{_PPS_FILE});
        $oSelf->{_PPS_FILE}->open("$sFile", "r+") || return undef;
    }
    else {
        return undef;
    }
    if($oSelf->{_PPS_FILE}) {
        $oSelf->{_PPS_FILE}->seek(0, 2);
        binmode($oSelf->{_PPS_FILE});
        $oSelf->{_PPS_FILE}->autoflush(1);
    }
    return $oSelf;
}
#------------------------------------------------------------------------------
# append (OLE::Storage_Lite::PPS::File)
#------------------------------------------------------------------------------
sub append ($$) {
    my($oSelf, $sData) = @_;
    if($oSelf->{_PPS_FILE}) {
        print {$oSelf->{_PPS_FILE}} $sData;
    }
    else {
        $oSelf->{Data} .= $sData;
    }
}

#//////////////////////////////////////////////////////////////////////////////
# OLE::Storage_Lite::PPS::Dir Object
#//////////////////////////////////////////////////////////////////////////////
#------------------------------------------------------------------------------
# new (OLE::Storage_Lite::PPS::Dir)
#------------------------------------------------------------------------------
package OLE::Storage_Lite::PPS::Dir;
require Exporter;
use strict;
use vars qw($VERSION @ISA);
@ISA = qw(OLE::Storage_Lite::PPS Exporter);
$VERSION = '0.19';
sub new ($$;$$$) {
    my($sClass, $sName, $raTime1st, $raTime2nd, $raChild) = @_;
    OLE::Storage_Lite::PPS::_new(
        $sClass,
        undef,
        $sName,
        1,
        undef,
        undef,
        undef,
        $raTime1st,
        $raTime2nd,
        undef,
        undef,
        undef,
        $raChild);
}
#==============================================================================
# OLE::Storage_Lite
#==============================================================================
package OLE::Storage_Lite;
require Exporter;

use strict;
use IO::File;
use Time::Local 'timegm';

use vars qw($VERSION @ISA @EXPORT);
@ISA = qw(Exporter);
$VERSION = '0.19';
sub _getPpsSearch($$$$$;$);
sub _getPpsTree($$$;$);
#------------------------------------------------------------------------------
# Const for OLE::Storage_Lite
#------------------------------------------------------------------------------
#0. Constants
sub PpsType_Root {5};
sub PpsType_Dir  {1};
sub PpsType_File {2};
sub DataSizeSmall{0x1000};
sub LongIntSize  {4};
sub PpsSize      {0x80};
#------------------------------------------------------------------------------
# new OLE::Storage_Lite
#------------------------------------------------------------------------------
sub new($$) {
  my($sClass, $sFile) = @_;
  my $oThis = {
    _FILE => $sFile,
  };
  bless $oThis;
  return $oThis;
}
#------------------------------------------------------------------------------
# getPpsTree: OLE::Storage_Lite
#------------------------------------------------------------------------------
sub getPpsTree($;$)
{
  my($oThis, $bData) = @_;
#0.Init
  my $rhInfo = _initParse($oThis->{_FILE});
  return undef unless($rhInfo);
#1. Get Data
  my ($oPps) = _getPpsTree(0, $rhInfo, $bData);
  close(IN);
  return $oPps;
}
#------------------------------------------------------------------------------
# getSearch: OLE::Storage_Lite
#------------------------------------------------------------------------------
sub getPpsSearch($$;$$)
{
  my($oThis, $raName, $bData, $iCase) = @_;
#0.Init
  my $rhInfo = _initParse($oThis->{_FILE});
  return undef unless($rhInfo);
#1. Get Data
  my @aList = _getPpsSearch(0, $rhInfo, $raName, $bData, $iCase);
  close(IN);
  return @aList;
}
#------------------------------------------------------------------------------
# getNthPps: OLE::Storage_Lite
#------------------------------------------------------------------------------
sub getNthPps($$;$)
{
  my($oThis, $iNo, $bData) = @_;
#0.Init
  my $rhInfo = _initParse($oThis->{_FILE});
  return undef unless($rhInfo);
#1. Get Data
  my $oPps = _getNthPps($iNo, $rhInfo, $bData);
  close IN;
  return $oPps;
}
#------------------------------------------------------------------------------
# _initParse: OLE::Storage_Lite
#------------------------------------------------------------------------------
sub _initParse($) {
  my($sFile)=@_;
  my $oIo;
  #1. $sFile is Ref of scalar
  if(ref($sFile) eq 'SCALAR') {
    require IO::Scalar;
    $oIo = new IO::Scalar;
    $oIo->open($sFile);
  }
  #2. $sFile is a IO::Handle object
  elsif(UNIVERSAL::isa($sFile, 'IO::Handle')) {
    $oIo = $sFile;
    binmode($oIo);
  }
  #3. $sFile is a simple filename string
  elsif(!ref($sFile)) {
    $oIo = new IO::File;
    $oIo->open("<$sFile") || return undef;
    binmode($oIo);
  }
  #4 Assume that if $sFile is a ref then it is a valid filehandle
  else {
    $oIo = $sFile;
    # Not all filehandles support binmode() so try it in an eval.
    eval{ binmode $oIo };
  }
  return _getHeaderInfo($oIo);
}
#------------------------------------------------------------------------------
# _getPpsTree: OLE::Storage_Lite
#------------------------------------------------------------------------------
sub _getPpsTree($$$;$) {
  my($iNo, $rhInfo, $bData, $raDone) = @_;
  if(defined($raDone)) {
    return () if(grep {$_ ==$iNo} @$raDone);
  }
  else {
    $raDone=[];
  }
  push @$raDone, $iNo;

  my $iRootBlock = $rhInfo->{_ROOT_START} ;
#1. Get Information about itself
  my $oPps = _getNthPps($iNo, $rhInfo, $bData);
#2. Child
  if($oPps->{DirPps} !=  0xFFFFFFFF) {
    my @aChildL = _getPpsTree($oPps->{DirPps}, $rhInfo, $bData, $raDone);
    $oPps->{Child} =  \@aChildL;
  }
  else {
    $oPps->{Child} =  undef;
  }
#3. Previous,Next PPSs
  my @aList = ();
  push @aList, _getPpsTree($oPps->{PrevPps}, $rhInfo, $bData, $raDone)
                        if($oPps->{PrevPps} != 0xFFFFFFFF);
  push @aList, $oPps;
  push @aList, _getPpsTree($oPps->{NextPps}, $rhInfo, $bData, $raDone)
                if($oPps->{NextPps} != 0xFFFFFFFF);
  return @aList;
}
#------------------------------------------------------------------------------
# _getPpsSearch: OLE::Storage_Lite
#------------------------------------------------------------------------------
sub _getPpsSearch($$$$$;$) {
  my($iNo, $rhInfo, $raName, $bData, $iCase, $raDone) = @_;
  my $iRootBlock = $rhInfo->{_ROOT_START} ;
  my @aRes;
#1. Check it self
  if(defined($raDone)) {
    return () if(grep {$_==$iNo} @$raDone);
  }
  else {
    $raDone=[];
  }
  push @$raDone, $iNo;
  my $oPps = _getNthPps($iNo, $rhInfo, undef);
#  if(grep($_ eq $oPps->{Name}, @$raName)) {
  if(($iCase && (grep(/^\Q$oPps->{Name}\E$/i, @$raName))) ||
     (grep($_ eq $oPps->{Name}, @$raName))) {
    $oPps = _getNthPps($iNo, $rhInfo, $bData) if ($bData);
    @aRes = ($oPps);
  }
  else {
    @aRes = ();
  }
#2. Check Child, Previous, Next PPSs
  push @aRes, _getPpsSearch($oPps->{DirPps},  $rhInfo, $raName, $bData, $iCase, $raDone)
        if($oPps->{DirPps} !=  0xFFFFFFFF) ;
  push @aRes, _getPpsSearch($oPps->{PrevPps}, $rhInfo, $raName, $bData, $iCase, $raDone)
        if($oPps->{PrevPps} != 0xFFFFFFFF );
  push @aRes, _getPpsSearch($oPps->{NextPps}, $rhInfo, $raName, $bData, $iCase, $raDone)
        if($oPps->{NextPps} != 0xFFFFFFFF);
  return @aRes;
}
#===================================================================
# Get Header Info (BASE Informain about that file)
#===================================================================
sub _getHeaderInfo($){
  my($FILE) = @_;
  my($iWk);
  my $rhInfo = {};
  $rhInfo->{_FILEH_} = $FILE;
  my $sWk;
#0. Check ID
  $rhInfo->{_FILEH_}->seek(0, 0);
  $rhInfo->{_FILEH_}->read($sWk, 8);
  return undef unless($sWk eq "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1");
#BIG BLOCK SIZE
  $iWk = _getInfoFromFile($rhInfo->{_FILEH_}, 0x1E, 2, "v");
  return undef unless(defined($iWk));
  $rhInfo->{_BIG_BLOCK_SIZE} = 2 ** $iWk;
#SMALL BLOCK SIZE
  $iWk = _getInfoFromFile($rhInfo->{_FILEH_}, 0x20, 2, "v");
  return undef unless(defined($iWk));
  $rhInfo->{_SMALL_BLOCK_SIZE} = 2 ** $iWk;
#BDB Count
  $iWk = _getInfoFromFile($rhInfo->{_FILEH_}, 0x2C, 4, "V");
  return undef unless(defined($iWk));
  $rhInfo->{_BDB_COUNT} = $iWk;
#START BLOCK
  $iWk = _getInfoFromFile($rhInfo->{_FILEH_}, 0x30, 4, "V");
  return undef unless(defined($iWk));
  $rhInfo->{_ROOT_START} = $iWk;
#MIN SIZE OF BB
#  $iWk = _getInfoFromFile($rhInfo->{_FILEH_}, 0x38, 4, "V");
#  return undef unless(defined($iWk));
#  $rhInfo->{_MIN_SIZE_BB} = $iWk;
#SMALL BD START
  $iWk = _getInfoFromFile($rhInfo->{_FILEH_}, 0x3C, 4, "V");
  return undef unless(defined($iWk));
  $rhInfo->{_SBD_START} = $iWk;
#SMALL BD COUNT
  $iWk = _getInfoFromFile($rhInfo->{_FILEH_}, 0x40, 4, "V");
  return undef unless(defined($iWk));
  $rhInfo->{_SBD_COUNT} = $iWk;
#EXTRA BBD START
  $iWk = _getInfoFromFile($rhInfo->{_FILEH_}, 0x44, 4, "V");
  return undef unless(defined($iWk));
  $rhInfo->{_EXTRA_BBD_START} = $iWk;
#EXTRA BD COUNT
  $iWk = _getInfoFromFile($rhInfo->{_FILEH_}, 0x48, 4, "V");
  return undef unless(defined($iWk));
  $rhInfo->{_EXTRA_BBD_COUNT} = $iWk;
#GET BBD INFO
  $rhInfo->{_BBD_INFO}= _getBbdInfo($rhInfo);
#GET ROOT PPS
  my $oRoot = _getNthPps(0, $rhInfo, undef);
  $rhInfo->{_SB_START} = $oRoot->{StartBlock};
  $rhInfo->{_SB_SIZE}  = $oRoot->{Size};
  return $rhInfo;
}
#------------------------------------------------------------------------------
# _getInfoFromFile
#------------------------------------------------------------------------------
sub _getInfoFromFile($$$$) {
  my($FILE, $iPos, $iLen, $sFmt) =@_;
  my($sWk);
  return undef unless($FILE);
  return undef if($FILE->seek($iPos, 0)==0);
  return undef if($FILE->read($sWk,  $iLen)!=$iLen);
  return unpack($sFmt, $sWk);
}
#------------------------------------------------------------------------------
# _getBbdInfo
#------------------------------------------------------------------------------
sub _getBbdInfo($) {
  my($rhInfo) =@_;
  my @aBdList = ();
  my $iBdbCnt = $rhInfo->{_BDB_COUNT};
  my $iGetCnt;
  my $sWk;
  my $i1stCnt = int(($rhInfo->{_BIG_BLOCK_SIZE} - 0x4C) / OLE::Storage_Lite::LongIntSize());
  my $iBdlCnt = int($rhInfo->{_BIG_BLOCK_SIZE} / OLE::Storage_Lite::LongIntSize()) - 1;

#1. 1st BDlist
  $rhInfo->{_FILEH_}->seek(0x4C, 0);
  $iGetCnt = ($iBdbCnt < $i1stCnt)? $iBdbCnt: $i1stCnt;
  $rhInfo->{_FILEH_}->read($sWk, OLE::Storage_Lite::LongIntSize()*$iGetCnt);
  push @aBdList, unpack("V$iGetCnt", $sWk);
  $iBdbCnt -= $iGetCnt;
#2. Extra BDList
  my $iBlock = $rhInfo->{_EXTRA_BBD_START};
  while(($iBdbCnt> 0) && _isNormalBlock($iBlock)){
    _setFilePos($iBlock, 0, $rhInfo);
    $iGetCnt= ($iBdbCnt < $iBdlCnt)? $iBdbCnt: $iBdlCnt;
    $rhInfo->{_FILEH_}->read($sWk, OLE::Storage_Lite::LongIntSize()*$iGetCnt);
    push @aBdList, unpack("V$iGetCnt", $sWk);
    $iBdbCnt -= $iGetCnt;
    $rhInfo->{_FILEH_}->read($sWk, OLE::Storage_Lite::LongIntSize());
    $iBlock = unpack("V", $sWk);
  }
#3.Get BDs
  my @aWk;
  my %hBd;
  my $iBlkNo = 0;
  my $iBdL;
  my $i;
  my $iBdCnt = int($rhInfo->{_BIG_BLOCK_SIZE} / OLE::Storage_Lite::LongIntSize());
  foreach $iBdL (@aBdList) {
    _setFilePos($iBdL, 0, $rhInfo);
    $rhInfo->{_FILEH_}->read($sWk, $rhInfo->{_BIG_BLOCK_SIZE});
    @aWk = unpack("V$iBdCnt", $sWk);
    for($i=0;$i<$iBdCnt;$i++, $iBlkNo++) {
       if($aWk[$i] != ($iBlkNo+1)){
            $hBd{$iBlkNo} = $aWk[$i];
        }
    }
  }
  return \%hBd;
}
#------------------------------------------------------------------------------
# getNthPps (OLE::Storage_Lite)
#------------------------------------------------------------------------------
sub _getNthPps($$$){
  my($iPos, $rhInfo, $bData) = @_;
  my($iPpsStart) = ($rhInfo->{_ROOT_START});
  my($iPpsBlock, $iPpsPos);
  my $sWk;
  my $iBlock;

  my $iBaseCnt = $rhInfo->{_BIG_BLOCK_SIZE} / OLE::Storage_Lite::PpsSize();
  $iPpsBlock = int($iPos / $iBaseCnt);
  $iPpsPos   = $iPos % $iBaseCnt;

  $iBlock = _getNthBlockNo($iPpsStart, $iPpsBlock, $rhInfo);
  return undef unless(defined($iBlock));

  _setFilePos($iBlock, OLE::Storage_Lite::PpsSize()*$iPpsPos, $rhInfo);
  $rhInfo->{_FILEH_}->read($sWk, OLE::Storage_Lite::PpsSize());
  return undef unless($sWk);
  my $iNmSize = unpack("v", substr($sWk, 0x40, 2));
  $iNmSize = ($iNmSize > 2)? $iNmSize - 2 : $iNmSize;
  my $sNm= substr($sWk, 0, $iNmSize);
  my $iType = unpack("C", substr($sWk, 0x42, 2));
  my $lPpsPrev = unpack("V", substr($sWk, 0x44, OLE::Storage_Lite::LongIntSize()));
  my $lPpsNext = unpack("V", substr($sWk, 0x48, OLE::Storage_Lite::LongIntSize()));
  my $lDirPps  = unpack("V", substr($sWk, 0x4C, OLE::Storage_Lite::LongIntSize()));
  my @raTime1st =
        (($iType == OLE::Storage_Lite::PpsType_Root()) or ($iType == OLE::Storage_Lite::PpsType_Dir()))?
            OLEDate2Local(substr($sWk, 0x64, 8)) : undef ,
  my @raTime2nd =
        (($iType == OLE::Storage_Lite::PpsType_Root()) or ($iType == OLE::Storage_Lite::PpsType_Dir()))?
            OLEDate2Local(substr($sWk, 0x6C, 8)) : undef,
  my($iStart, $iSize) = unpack("VV", substr($sWk, 0x74, 8));
  if($bData) {
      my $sData = _getData($iType, $iStart, $iSize, $rhInfo);
      return OLE::Storage_Lite::PPS->new(
        $iPos, $sNm, $iType, $lPpsPrev, $lPpsNext, $lDirPps,
        \@raTime1st, \@raTime2nd, $iStart, $iSize, $sData, undef);
  }
  else {
      return OLE::Storage_Lite::PPS->new(
        $iPos, $sNm, $iType, $lPpsPrev, $lPpsNext, $lDirPps,
        \@raTime1st, \@raTime2nd, $iStart, $iSize, undef, undef);
  }
}
#------------------------------------------------------------------------------
# _setFilePos (OLE::Storage_Lite)
#------------------------------------------------------------------------------
sub _setFilePos($$$){
  my($iBlock, $iPos, $rhInfo) = @_;
  $rhInfo->{_FILEH_}->seek(($iBlock+1)*$rhInfo->{_BIG_BLOCK_SIZE}+$iPos, 0);
}
#------------------------------------------------------------------------------
# _getNthBlockNo (OLE::Storage_Lite)
#------------------------------------------------------------------------------
sub _getNthBlockNo($$$){
  my($iStBlock, $iNth, $rhInfo) = @_;
  my $iSv;
  my $iNext = $iStBlock;
  for(my $i =0; $i<$iNth; $i++) {
    $iSv = $iNext;
    $iNext = _getNextBlockNo($iSv, $rhInfo);
    return undef unless _isNormalBlock($iNext);
  }
  return $iNext;
}
#------------------------------------------------------------------------------
# _getData (OLE::Storage_Lite)
#------------------------------------------------------------------------------
sub _getData($$$$)
{
  my($iType, $iBlock, $iSize, $rhInfo) = @_;
  if ($iType == OLE::Storage_Lite::PpsType_File()) {
    if($iSize < OLE::Storage_Lite::DataSizeSmall()) {
        return _getSmallData($iBlock, $iSize, $rhInfo);
    }
    else {
        return _getBigData($iBlock, $iSize, $rhInfo);
    }
  }
  elsif($iType == OLE::Storage_Lite::PpsType_Root()) {  #Root
    return _getBigData($iBlock, $iSize, $rhInfo);
  }
  elsif($iType == OLE::Storage_Lite::PpsType_Dir()) {  # Directory
    return undef;
  }
}
#------------------------------------------------------------------------------
# _getBigData (OLE::Storage_Lite)
#------------------------------------------------------------------------------
sub _getBigData($$$)
{
  my($iBlock, $iSize, $rhInfo) = @_;
  my($iRest, $sWk, $sRes);

  return '' unless(_isNormalBlock($iBlock));
  $iRest = $iSize;
  my($i, $iGetSize, $iNext);
  $sRes = '';
  my @aKeys= sort({$a<=>$b} keys(%{$rhInfo->{_BBD_INFO}}));

  while ($iRest > 0) {
    my @aRes = grep($_ >= $iBlock, @aKeys);
    my $iNKey = $aRes[0];
    $i = $iNKey - $iBlock;
    $iNext = $rhInfo->{_BBD_INFO}{$iNKey};
    _setFilePos($iBlock, 0, $rhInfo);
    my $iGetSize = ($rhInfo->{_BIG_BLOCK_SIZE} * ($i+1));
    $iGetSize = $iRest if($iRest < $iGetSize);
    $rhInfo->{_FILEH_}->read( $sWk, $iGetSize);
    $sRes .= $sWk;
    $iRest -= $iGetSize;
    $iBlock= $iNext;
  }
  return $sRes;
}
#------------------------------------------------------------------------------
# _getNextBlockNo (OLE::Storage_Lite)
#------------------------------------------------------------------------------
sub _getNextBlockNo($$){
  my($iBlockNo, $rhInfo) = @_;
  my $iRes = $rhInfo->{_BBD_INFO}->{$iBlockNo};
  return defined($iRes)? $iRes: $iBlockNo+1;
}
#------------------------------------------------------------------------------
# _isNormalBlock (OLE::Storage_Lite)
# 0xFFFFFFFC : BDList, 0xFFFFFFFD : BBD,
# 0xFFFFFFFE: End of Chain 0xFFFFFFFF : unused
#------------------------------------------------------------------------------
sub _isNormalBlock($){
  my($iBlock) = @_;
  return ($iBlock < 0xFFFFFFFC)? 1: undef;
}
#------------------------------------------------------------------------------
# _getSmallData (OLE::Storage_Lite)
#------------------------------------------------------------------------------
sub _getSmallData($$$)
{
  my($iSmBlock, $iSize, $rhInfo) = @_;
  my($sRes, $sWk);
  my $iRest = $iSize;
  $sRes = '';
  while ($iRest > 0) {
    _setFilePosSmall($iSmBlock, $rhInfo);
    $rhInfo->{_FILEH_}->read($sWk,
        ($iRest >= $rhInfo->{_SMALL_BLOCK_SIZE})?
            $rhInfo->{_SMALL_BLOCK_SIZE}: $iRest);
    $sRes .= $sWk;
    $iRest -= $rhInfo->{_SMALL_BLOCK_SIZE};
    $iSmBlock= _getNextSmallBlockNo($iSmBlock, $rhInfo);
  }
  return $sRes;
}
#------------------------------------------------------------------------------
# _setFilePosSmall(OLE::Storage_Lite)
#------------------------------------------------------------------------------
sub _setFilePosSmall($$)
{
  my($iSmBlock, $rhInfo) = @_;
  my $iSmStart = $rhInfo->{_SB_START};
  my $iBaseCnt = $rhInfo->{_BIG_BLOCK_SIZE} / $rhInfo->{_SMALL_BLOCK_SIZE};
  my $iNth = int($iSmBlock/$iBaseCnt);
  my $iPos = $iSmBlock % $iBaseCnt;

  my $iBlk = _getNthBlockNo($iSmStart, $iNth, $rhInfo);
  _setFilePos($iBlk, $iPos * $rhInfo->{_SMALL_BLOCK_SIZE}, $rhInfo);
}
#------------------------------------------------------------------------------
# _getNextSmallBlockNo (OLE::Storage_Lite)
#------------------------------------------------------------------------------
sub _getNextSmallBlockNo($$)
{
  my($iSmBlock, $rhInfo) = @_;
  my($sWk);

  my $iBaseCnt = $rhInfo->{_BIG_BLOCK_SIZE} / OLE::Storage_Lite::LongIntSize();
  my $iNth = int($iSmBlock/$iBaseCnt);
  my $iPos = $iSmBlock % $iBaseCnt;
  my $iBlk = _getNthBlockNo($rhInfo->{_SBD_START}, $iNth, $rhInfo);
  _setFilePos($iBlk, $iPos * OLE::Storage_Lite::LongIntSize(), $rhInfo);
  $rhInfo->{_FILEH_}->read($sWk, OLE::Storage_Lite::LongIntSize());
  return unpack("V", $sWk);

}
#------------------------------------------------------------------------------
# Asc2Ucs: OLE::Storage_Lite
#------------------------------------------------------------------------------
sub Asc2Ucs($)
{
  my($sAsc) = @_;
  return join("\x00", split //, $sAsc) . "\x00";
}
#------------------------------------------------------------------------------
# Ucs2Asc: OLE::Storage_Lite
#------------------------------------------------------------------------------
sub Ucs2Asc($)
{
  my($sUcs) = @_;
  return join('', map(pack('c', $_), unpack('v*', $sUcs)));
}

#------------------------------------------------------------------------------
# OLEDate2Local()
#
# Convert from a Window FILETIME structure to a localtime array. FILETIME is
# a 64-bit value representing the number of 100-nanosecond intervals since
# January 1 1601.
#
# We first convert the FILETIME to seconds and then subtract the difference
# between the 1601 epoch and the 1970 Unix epoch.
#
sub OLEDate2Local {

    my $oletime = shift;

    # Unpack the FILETIME into high and low longs.
    my ( $lo, $hi ) = unpack 'V2', $oletime;

    # Convert the longs to a double.
    my $nanoseconds = $hi * 2**32 + $lo;

    # Convert the 100 nanosecond units into seconds.
    my $time = $nanoseconds / 1e7;

    # Subtract the number of seconds between the 1601 and 1970 epochs.
    $time -= 11644473600;

    # Convert to a localtime (actually gmtime) structure.
    my @localtime = gmtime($time);

    return @localtime;
}

#------------------------------------------------------------------------------
# LocalDate2OLE()
#
# Convert from a a localtime array to a Window FILETIME structure. FILETIME is
# a 64-bit value representing the number of 100-nanosecond intervals since
# January 1 1601.
#
# We first convert the localtime (actually gmtime) to seconds and then add the
# difference between the 1601 epoch and the 1970 Unix epoch. We convert that to
# 100 nanosecond units, divide it into high and low longs and return it as a
# packed 64bit structure.
#
sub LocalDate2OLE {

    my $localtime = shift;

    return "\x00" x 8 unless $localtime;

    # Convert from localtime (actually gmtime) to seconds.
    my $time = timegm( @{$localtime} );

    # Add the number of seconds between the 1601 and 1970 epochs.
    $time += 11644473600;

    # The FILETIME seconds are in units of 100 nanoseconds.
    my $nanoseconds = $time * 1E7;

use POSIX 'fmod';

    # Pack the total nanoseconds into 64 bits...
    my $hi = int( $nanoseconds / 2**32 );
    my $lo = fmod($nanoseconds, 2**32);

    my $oletime = pack "VV", $lo, $hi;

    return $oletime;
}

1;
__END__


=head1 NAME

OLE::Storage_Lite - Simple Class for OLE document interface.

=head1 SYNOPSIS

    use OLE::Storage_Lite;

    # Initialize.

    # From a file
    my $oOl = OLE::Storage_Lite->new("some.xls");

    # From a filehandle object
    use IO::File;
    my $oIo = new IO::File;
    $oIo->open("<iofile.xls");
    binmode($oIo);
    my $oOl = OLE::Storage_Lite->new($oFile);

    # Read data
    my $oPps = $oOl->getPpsTree(1);

    # Save Data
    # To a File
    $oPps->save("kaba.xls"); #kaba.xls
    $oPps->save('-');        #STDOUT

    # To a filehandle object
    my $oIo = new IO::File;
    $oIo->open(">iofile.xls");
    bimode($oIo);
    $oPps->save($oIo);


=head1 DESCRIPTION

OLE::Storage_Lite allows you to read and write an OLE structured file.

OLE::Storage_Lite::PPS is a class representing PPS. OLE::Storage_Lite::PPS::Root, OLE::Storage_Lite::PPS::File and OLE::Storage_Lite::PPS::Dir
are subclasses of OLE::Storage_Lite::PPS.


=head2 new()

Constructor.

    $oOle = OLE::Storage_Lite->new($sFile);

Creates a OLE::Storage_Lite object for C<$sFile>. C<$sFile> must be a correct file name.

The C<new()> constructor also accepts a valid filehandle. Remember to C<binmode()> the filehandle first.


=head2 getPpsTree()

    $oPpsRoot = $oOle->getPpsTree([$bData]);

Returns PPS as an OLE::Storage_Lite::PPS::Root object.
Other PPS objects will be included as its children.

If C<$bData> is true, the objects will have data in the file.


=head2 getPpsSearch()

    $oPpsRoot = $oOle->getPpsTree($raName [, $bData][, $iCase] );

Returns PPSs as OLE::Storage_Lite::PPS objects that has the name specified in C<$raName> array.

If C<$bData> is true, the objects will have data in the file.
If C<$iCase> is true, search is case insensitive.


=head2 getNthPps()

    $oPpsRoot = $oOle->getNthPps($iNth [, $bData]);

Returns PPS as C<OLE::Storage_Lite::PPS> object specified number C<$iNth>.

If C<$bData> is true, the objects will have data in the file.


=head2 Asc2Ucs()

    $sUcs2 = OLE::Storage_Lite::Asc2Ucs($sAsc>);

Utility function. Just adds 0x00 after every characters in C<$sAsc>.


=head2 Ucs2Asc()

    $sAsc = OLE::Storage_Lite::Ucs2Asc($sUcs2);

Utility function. Just deletes 0x00 after words in C<$sUcs>.


=head1 OLE::Storage_Lite::PPS

OLE::Storage_Lite::PPS has these properties:

=over 4

=item No

Order number in saving.

=item Name

Its name in UCS2 (a.k.a Unicode).

=item Type

Its type (1:Dir, 2:File (Data), 5: Root)

=item PrevPps

Previous pps (as No)

=item NextPps

Next pps (as No)

=item DirPps

Dir pps (as No).

=item Time1st

Timestamp 1st in array ref as similar fomat of localtime.

=item Time2nd

Timestamp 2nd in array ref as similar fomat of localtime.

=item StartBlock

Start block number

=item Size

Size of the pps

=item Data

Its data

=item Child

Its child PPSs in array ref

=back


=head1 OLE::Storage_Lite::PPS::Root

OLE::Storage_Lite::PPS::Root has 2 methods.

=head2 new()

    $oRoot = OLE::Storage_Lite::PPS::Root->new(
                    $raTime1st,
                    $raTime2nd,
                    $raChild);


Constructor.

C<$raTime1st>, C<$raTime2nd> are array refs with ($iSec, $iMin, $iHour, $iDay, $iMon, $iYear).
$iSec means seconds, $iMin means minutes. $iHour means hours.
$iDay means day. $iMon is month -1. $iYear is year - 1900.

C<$raChild> is a array ref of children PPSs.


=head2 save()

    $oRoot = $oRoot>->save(
                    $sFile,
                    $bNoAs);


Saves information into C<$sFile>. If C<$sFile> is '-', this will use STDOUT.

The C<new()> constructor also accepts a valid filehandle. Remember to C<binmode()> the filehandle first.

If C<$bNoAs> is defined, this function will use the No of PPSs for saving order.
If C<$bNoAs> is undefined, this will calculate PPS saving order.


=head1 OLE::Storage_Lite::PPS::Dir

OLE::Storage_Lite::PPS::Dir has 1 method.

=head2 new()

    $oRoot = OLE::Storage_Lite::PPS::Dir->new(
                    $sName,
                  [, $raTime1st]
                  [, $raTime2nd]
                  [, $raChild>]);


Constructor.

C<$sName> is a name of the PPS.

C<$raTime1st>, C<$raTime2nd> is a array ref as
($iSec, $iMin, $iHour, $iDay, $iMon, $iYear).
$iSec means seconds, $iMin means minutes. $iHour means hours.
$iDay means day. $iMon is month -1. $iYear is year - 1900.

C<$raChild> is a array ref of children PPSs.


=head1 OLE::Storage_Lite::PPS::File

OLE::Storage_Lite::PPS::File has 3 method.

=head2 new

    $oRoot = OLE::Storage_Lite::PPS::File->new($sName, $sData);

C<$sName> is name of the PPS.

C<$sData> is data of the PPS.


=head2 newFile()

    $oRoot = OLE::Storage_Lite::PPS::File->newFile($sName, $sFile);

This function makes to use file handle for geting and storing data.

C<$sName> is name of the PPS.

If C<$sFile> is scalar, it assumes that is a filename.
If C<$sFile> is an IO::Handle object, it uses that specified handle.
If C<$sFile> is undef or '', it uses temporary file.

CAUTION: Take care C<$sFile> will be updated by C<append> method.
So if you want to use IO::Handle and append a data to it,
you should open the handle with "r+".


=head2 append()

    $oRoot = $oPps->append($sData);

appends specified data to that PPS.

C<$sData> is appending data for that PPS.


=head1 CAUTION

A saved file with VBA (a.k.a Macros) by this module will not work correctly.
However modules can get the same information from the file,
the file occurs a error in application(Word, Excel ...).


=head1 DEPRECATED FEATURES

Older version of C<OLE::Storage_Lite> autovivified a scalar ref in the C<new()> constructors into a scalar filehandle. This functionality is still there for backwards compatibility but it is highly recommended that you do not use it. Instead create a filehandle (scalar or otherwise) and pass that in.


=head1 COPYRIGHT

The OLE::Storage_Lite module is Copyright (c) 2000,2001 Kawai Takanori. Japan.
All rights reserved.

You may distribute under the terms of either the GNU General Public
License or the Artistic License, as specified in the Perl README file.


=head1 ACKNOWLEDGEMENTS

First of all, I would like to acknowledge to Martin Schwartz and his module OLE::Storage.


=head1 AUTHOR

Kawai Takanori kwitknr@cpan.org

This module is currently maintained by John McNamara jmcnamara@cpan.org


=head1 SEE ALSO

OLE::Storage

Documentation for the OLE Compound document has been released by Microsoft under the I<Open Specification Promise>. See http://www.microsoft.com/interop/docs/supportingtechnologies.mspx

The Digital Imaging Group have also detailed the OLE format in the JPEG2000 specification: see Appendix A of http://www.i3a.org/pdf/wg1n1017.pdf


=cut
