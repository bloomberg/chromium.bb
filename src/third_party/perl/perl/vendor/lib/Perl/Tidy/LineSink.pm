#####################################################################
#
# the Perl::Tidy::LineSink class supplies a write_line method for
# actual file writing
#
#####################################################################

package Perl::Tidy::LineSink;
use strict;
use warnings;
our $VERSION = '20181120';

sub new {

    my ( $class, $output_file, $tee_file, $line_separator, $rOpts,
        $rpending_logfile_message, $binmode )
      = @_;
    my $fh     = undef;
    my $fh_tee = undef;

    my $output_file_open = 0;

    if ( $rOpts->{'format'} eq 'tidy' ) {
        ( $fh, $output_file ) = Perl::Tidy::streamhandle( $output_file, 'w' );
        unless ($fh) { Perl::Tidy::Die("Cannot write to output stream\n"); }
        $output_file_open = 1;
        if ($binmode) {
            if (   $rOpts->{'character-encoding'}
                && $rOpts->{'character-encoding'} eq 'utf8' )
            {
                if ( ref($fh) eq 'IO::File' ) {
                    $fh->binmode(":encoding(UTF-8)");
                }
                elsif ( $output_file eq '-' ) {
                    binmode STDOUT, ":encoding(UTF-8)";
                }
            }

            # Patch for RT 122030
            elsif ( ref($fh) eq 'IO::File' ) { $fh->binmode(); }

            elsif ( $output_file eq '-' ) { binmode STDOUT }
        }
    }

    # in order to check output syntax when standard output is used,
    # or when it is an object, we have to make a copy of the file
    if ( $output_file eq '-' || ref $output_file ) {
        if ( $rOpts->{'check-syntax'} ) {

            # Turning off syntax check when standard output is used.
            # The reason is that temporary files cause problems on
            # on many systems.
            $rOpts->{'check-syntax'} = 0;
            ${$rpending_logfile_message} .= <<EOM;
Note: --syntax check will be skipped because standard output is used
EOM

        }
    }

    return bless {
        _fh               => $fh,
        _fh_tee           => $fh_tee,
        _output_file      => $output_file,
        _output_file_open => $output_file_open,
        _tee_flag         => 0,
        _tee_file         => $tee_file,
        _tee_file_opened  => 0,
        _line_separator   => $line_separator,
        _binmode          => $binmode,
    }, $class;
}

sub write_line {

    my ( $self, $line ) = @_;
    my $fh = $self->{_fh};

    my $output_file_open = $self->{_output_file_open};
    chomp $line;
    $line .= $self->{_line_separator};

    $fh->print($line) if ( $self->{_output_file_open} );

    if ( $self->{_tee_flag} ) {
        unless ( $self->{_tee_file_opened} ) { $self->really_open_tee_file() }
        my $fh_tee = $self->{_fh_tee};
        print $fh_tee $line;
    }
    return;
}

sub tee_on {
    my $self = shift;
    $self->{_tee_flag} = 1;
    return;
}

sub tee_off {
    my $self = shift;
    $self->{_tee_flag} = 0;
    return;
}

sub really_open_tee_file {
    my $self     = shift;
    my $tee_file = $self->{_tee_file};
    my $fh_tee;
    $fh_tee = IO::File->new(">$tee_file")
      or Perl::Tidy::Die("couldn't open TEE file $tee_file: $!\n");
    binmode $fh_tee if $self->{_binmode};
    $self->{_tee_file_opened} = 1;
    $self->{_fh_tee}          = $fh_tee;
    return;
}

sub close_output_file {
    my $self = shift;

    # Only close physical files, not STDOUT and other objects
    my $output_file = $self->{_output_file};
    if ( $output_file ne '-' && !ref $output_file ) {
        eval { $self->{_fh}->close() } if $self->{_output_file_open};
    }
    $self->close_tee_file();
    return;
}

sub close_tee_file {
    my $self = shift;

    # Only close physical files, not STDOUT and other objects
    if ( $self->{_tee_file_opened} ) {
        my $tee_file = $self->{_tee_file};
        if ( $tee_file ne '-' && !ref $tee_file ) {
            eval { $self->{_fh_tee}->close() };
            $self->{_tee_file_opened} = 0;
        }
    }
    return;
}

1;

