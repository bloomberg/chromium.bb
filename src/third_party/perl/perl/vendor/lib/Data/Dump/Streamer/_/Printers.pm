{
    package Data::Dump::Streamer::_::StringPrinter;
    #$Id: Printers.pm 26 2006-04-16 15:18:52Z demerphq $#
    $VERSION= "0.1";
    my %items;
    sub DESTROY { delete $items{$_[0]} }

    sub new {
        my $class = shift;
        my $self = bless \do { my $str = '' }, $class;
        $self->print(@_);
        return $self;
    }

    sub print {
        my $self = shift;
        $items{$self} .= join "", @_;
    }
    sub value  { $items{$_[0]} }
    sub string { $_[0]->value() }
    1;
}

{

    package Data::Dump::Streamer::_::ListPrinter;
    $VERSION= "0.1";
    my %items;
    sub DESTROY { delete $items{$_[0]} }

    sub new {
        my $class = shift;
        my $self = bless \do { my $str = '' }, $class;
        $items{$self} = [];
        $self->print(@_);
        return $self;
    }

    sub print {
        my $self = $items{shift (@_)};
        my $str = join ( '', @_ );
        if (   !@$self
            or $self->[-1] =~ /\n/
            or length( $self->[-1] ) > 4000 )
        {
            push @{$self}, $str;
        } else {
            $self->[-1] .= $str;
        }
    }
    sub value { @{$items{$_[0]}} }
    sub string { join ( '', @{$items{$_[0]}} ) }
    1;
}


__END__

