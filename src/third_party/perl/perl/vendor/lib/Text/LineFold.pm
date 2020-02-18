#-*- perl -*-

package Text::LineFold;
require 5.008;

=encoding utf-8

=head1 NAME

Text::LineFold - Line Folding for Plain Text

=head1 SYNOPSIS

    use Text::LineFold;
    $lf = Text::LineFold->new();
    
    # Fold lines
    $folded = $lf->fold($string, 'PLAIN');
    $indented = $lf->fold(' ' x 8, ' ' x 4, $string);

    # Unfold lines
    $unfolded = $lf->unfold($string, 'FIXED');

=head1 DESCRIPTION

Text::LineFold folds or unfolds lines of plain text.
As it mainly focuses on plain text e-mail messages,
RFC 3676 flowed format is also supported.

=cut

### Pragmas:
use strict;
use vars qw($VERSION @EXPORT_OK @ISA $Config);

### Exporting:
use Exporter;

### Inheritance:
our @ISA = qw(Exporter Unicode::LineBreak);

### Other modules:
use Carp qw(croak carp);
use Encode qw(is_utf8);
use MIME::Charset;
use Unicode::LineBreak qw(:all);

### Globals

### The package Version
our $VERSION = '2018.012';

### Public Configuration Attributes
our $Config = {
    ### %{$Unicode::LineBreak::Config},
    Charset => 'UTF-8',
    Language => 'XX',
    OutputCharset => undef,
    TabSize => 8,
};

### Privates

my %FORMAT_FUNCS = (
    'FIXED' => sub {
        my $self = shift;
        my $action = shift;
        my $str = shift;
        if ($action =~ /^so[tp]/) {
            $self->{_} = {};
            $self->{_}->{'ColMax'} = $self->config('ColMax');
            $self->config('ColMax' => 0) if $str =~ /^>/;
        } elsif ($action eq "") {
            $self->{_}->{line} = $str;
        } elsif ($action eq "eol") {
            return $self->config('Newline');
        } elsif ($action =~ /^eo/) {
            if (length $self->{_}->{line} and $self->config('ColMax')) {
                $str = $self->config('Newline').$self->config('Newline');
            } else {
                $str = $self->config('Newline');
            }
            $self->config('ColMax' => $self->{_}->{'ColMax'});
            delete $self->{_};
            return $str;
        }
        undef;
    },
    'FLOWED' => sub { # RFC 3676
        my $self = shift;
        my $action = shift;
        my $str = shift;
        if ($action eq 'sol') {
            if ($self->{_}->{prefix}) {
                return $self->{_}->{prefix}.' '.$str;
            }
        } elsif ($action =~ /^so/) {
            $self->{_} = {};
            if ($str =~ /^(>+)/) {
                $self->{_}->{prefix} = $1;
            } else {
                $self->{_}->{prefix} = '';
            }
        } elsif ($action eq "") {
            if ($str =~ /^(?: |From )/
                or $str =~ /^>/ and !length $self->{_}->{prefix}) {
                return $self->{_}->{line} = ' ' . $str;
            }
            $self->{_}->{line} = $str;
        } elsif ($action eq 'eol') {
            $str = ' ' if length $str;
            return $str.' '.$self->config('Newline');
        } elsif ($action =~ /^eo/) {
            if (length $self->{_}->{line} and !length $self->{_}->{prefix}) {
                $str = ' '.$self->config('Newline').$self->config('Newline');
            } else {
                $str = $self->config('Newline');
            }
            delete $self->{_};
            return $str;
        }
        undef;
    },
    'PLAIN' => sub {
        return $_[0]->config('Newline') if $_[1] =~ /^eo/;
        undef;
    },
);

=head2 Public Interface

=over 4

=item new ([KEY => VALUE, ...])

I<Constructor>.
About KEY => VALUE pairs see config method.

=back

=cut

sub new {
    my $class = shift;
    my $self = bless __PACKAGE__->SUPER::new(), $class;
    $self->config(@_);
    $self;
}

=over 4

=item $self->config (KEY)

=item $self->config ([KEY => VAL, ...])

I<Instance method>.
Get or update configuration.  Following KEY => VALUE pairs may be specified.

=over 4

=item Charset => CHARSET

Character set that is used to encode string.
It may be string or L<MIME::Charset> object.
Default is C<"UTF-8">.

=item Language => LANGUAGE

Along with Charset option, this may be used to define language/region
context.
Default is C<"XX">.
See also L<Unicode::LineBreak/Context> option.

=item Newline => STRING

String to be used for newline sequence.
Default is C<"\n">.

=item OutputCharset => CHARSET

Character set that is used to encode result of fold()/unfold().
It may be string or L<MIME::Charset> object.
If a special value C<"_UNICODE_"> is specified, result will be Unicode string.
Default is the value of Charset option.

=item TabSize => NUMBER

Column width of tab stops.
When 0 is specified, tab stops are ignored.
Default is 8.

=item BreakIndent

=item CharMax

=item ColMax

=item ColMin

=item ComplexBreaking

=item EAWidth

=item HangulAsAL

=item LBClass

=item LegacyCM

=item Prep

=item Urgent

See L<Unicode::LineBreak/Options>.

=back

=back

=cut

sub config {
    my $self = shift;
    my @opts = qw{Charset Language OutputCharset TabSize};
    my %opts = map { (uc $_ => $_) } @opts;
    my $newline = undef;

    # Get config.
    if (scalar @_ == 1) {
        if ($opts{uc $_[0]}) {
            return $self->{$opts{uc $_[0]}};
        }
        return $self->SUPER::config($_[0]);
    }

    # Set config.
    my @o = ();
    while (scalar @_) {
        my $k = shift;
        my $v = shift;
        if ($opts{uc $k}) {
            $self->{$opts{uc $k}} = $v;
        } elsif (uc $k eq uc 'Newline') {
            $newline = $v;
        } else {
            push @o, $k => $v;
        }
    }
    $self->SUPER::config(@o) if scalar @o;

    # Character set and language assumed.
    if (ref $self->{Charset} eq 'MIME::Charset') {
        $self->{_charset} = $self->{Charset};
    } else {
        $self->{Charset} ||= $Config->{Charset};
        $self->{_charset} = MIME::Charset->new($self->{Charset});
    }
    $self->{Charset} = $self->{_charset}->as_string;
    my $ocharset = uc($self->{OutputCharset} || $self->{Charset});
    $ocharset = MIME::Charset->new($ocharset)
        unless ref $ocharset eq 'MIME::Charset' or $ocharset eq '_UNICODE_';
    unless ($ocharset eq '_UNICODE_') {
        $self->{_charset}->encoder($ocharset);
        $self->{OutputCharset} = $ocharset->as_string;
    }
    $self->{Language} = uc($self->{Language} || $Config->{Language});

    ## Context
    $self->SUPER::config(Context =>
                         context(Charset => $self->{Charset},
                                 Language => $self->{Language}));

    ## Set sizing method.
    $self->SUPER::config(Sizing => sub {
        my ($self, $cols, $pre, $spc, $str) = @_;

        my $tabsize = $self->{TabSize};
        my $spcstr = $spc.$str;
        $spcstr->pos(0);
        while (!$spcstr->eos and $spcstr->item->lbc == LB_SP) {
            my $c = $spcstr->next;
            if ($c eq "\t") {
                $cols += $tabsize - $cols % $tabsize if $tabsize;
            } else {
                $cols += $c->columns;
            }
        }
        return $cols + $spcstr->substr($spcstr->pos)->columns;
    });

    ## Classify horizontal tab as line breaking class SP.
    $self->SUPER::config(LBClass => [ord("\t") => LB_SP]);
    ## Tab size
    if (defined $self->{TabSize}) {
        croak "Invalid TabSize option" unless $self->{TabSize} =~ /^\d+$/;
        $self->{TabSize} += 0;
    } else {
        $self->{TabSize} = $Config->{TabSize};
    }

    ## Newline
    if (defined $newline) {
        $newline = $self->{_charset}->decode($newline)
            unless is_utf8($newline);
        $self->SUPER::config(Newline => $newline);
    }
}

=over 4

=item $self->fold (STRING, [METHOD])

=item $self->fold (INITIAL_TAB, SUBSEQUENT_TAB, STRING, ...)

I<Instance method>.
fold() folds lines of string STRING and returns it.
Surplus SPACEs and horizontal tabs at end of line are removed,
newline sequences are replaced by that specified by Newline option
and newline is appended at end of text if it does not exist.
Horizontal tabs are treated as tab stops according to TabSize option.

By the first style, following options may be specified for METHOD argument.

=over 4

=item C<"FIXED">

Lines preceded by C<"E<gt>"> won't be folded.
Paragraphs are separated by empty line.

=item C<"FLOWED">

C<"Format=Flowed; DelSp=Yes"> formatting defined by RFC 3676.

=item C<"PLAIN">

Default method.  All lines are folded.

=back

Second style is similar to L<Text::Wrap/wrap()>.
All lines are folded.
INITIAL_TAB is inserted at beginning of paragraphs and SUBSEQUENT_TAB
at beginning of other broken lines.

=back

=cut

# Special breaking characters: VT, FF, NEL, LS, PS
my $special_break = qr/([\x{000B}\x{000C}\x{0085}\x{2028}\x{2029}])/os;

sub fold {
    my $self = shift;
    my $str;

    if (2 < scalar @_) {
        my $initial_tab = shift || '';
        $initial_tab = $self->{_charset}->decode($initial_tab)
            unless is_utf8($initial_tab);
        my $subsequent_tab = shift || '';
        $subsequent_tab = $self->{_charset}->decode($subsequent_tab)
            unless is_utf8($subsequent_tab);
        my @str = @_;

        ## Decode and concat strings.
        $str = shift @str;
        $str = $self->{_charset}->decode($str) unless is_utf8($str);
        foreach my $s (@str) {
            next unless defined $s and length $s;

            $s = $self->{_charset}->decode($s) unless is_utf8($s);
            unless (length $str) {
                $str = $s;
            } elsif ($str =~ /(\s|$special_break)$/ or
                     $s =~ /^(\s|$special_break)/) {
                $str .= $s;
            } else {
                $str .= ' ' if $self->breakingRule($str, $s) == INDIRECT;
                $str .= $s;
            }
        }

        ## Set format method.
        $self->SUPER::config(Format => sub {
            my $self = shift;
            my $event = shift;
            my $str = shift;
            if ($event =~ /^eo/) { return $self->config('Newline'); }
            if ($event =~ /^so[tp]/) { return $initial_tab.$str; }
            if ($event eq 'sol') { return $subsequent_tab.$str; }
            undef;
        });
    } else {
        $str = shift;
        my $method = uc(shift || '');
        return '' unless defined $str and length $str;

        ## Decode string.
        $str = $self->{_charset}->decode($str) unless is_utf8($str);

        ## Set format method.
        $self->SUPER::config(Format => $FORMAT_FUNCS{$method} ||
                             $FORMAT_FUNCS{'PLAIN'});
    }

    ## Do folding.
    my $result = '';
    foreach my $s (split $special_break, $str) {
        if ($s =~ $special_break) {
            $result .= $s;
        } else {
            $result .= $self->break($str);
        }
    }

    ## Encode result.
    if ($self->{OutputCharset} eq '_UNICODE_') {
        return $result;
    } else {
        return $self->{_charset}->encode($result);
    }
}

=over 4

=item $self->unfold (STRING, METHOD)

Conjunct folded paragraphs of string STRING and returns it.

Following options may be specified for METHOD argument.

=over 4

=item C<"FIXED">

Default method.
Lines preceded by C<"E<gt>"> won't be conjuncted.
Treat empty line as paragraph separator.

=item C<"FLOWED">

Unfold C<"Format=Flowed; DelSp=Yes"> formatting defined by RFC 3676.

=item C<"FLOWEDSP">

Unfold C<"Format=Flowed; DelSp=No"> formatting defined by RFC 3676.

=begin comment

=item C<"OBSFLOWED">

Unfold C<"Format=Flowed> formatting defined by (obsoleted) RFC 2646
as well as possible.

=end comment

=back

=back

=cut

sub unfold {
    my $self = shift;
    my $str = shift;
    return '' unless defined $str and length $str;

    ## Get format method.
    my $method = uc(shift || 'FIXED');
    $method = 'FIXED' unless $method =~ /^(?:FIXED|FLOWED|FLOWEDSP|OBSFLOWED)$/;
    my $delsp = $method eq 'FLOWED';

    ## Decode string and canonizalize newline.
    $str = $self->{_charset}->decode($str) unless is_utf8($str);
    $str =~ s/\r\n|\r/\n/g;

    ## Do unfolding.
    my $result = '';
    foreach my $s (split $special_break, $str) {
        if ($s eq '') {
            next;
        } elsif ($s =~ $special_break) {
            $result .= $s;
            next;
        } elsif ($method eq 'FIXED') {
            pos($s) = 0;
            while ($s !~ /\G\z/cg) {
                if ($s =~ /\G\n/cg) {
                    $result .= $self->config('Newline');
                } elsif ($s =~ /\G(.+)\n\n/cg) {
                    $result .= $1.$self->config('Newline');
                } elsif ($s =~ /\G(>.*)\n/cg) {
                    $result .= $1.$self->config('Newline');
                } elsif ($s =~ /\G(.+)\n(?=>)/cg) {
                    $result .= $1.$self->config('Newline');
                } elsif ($s =~ /\G(.+?)( *)\n(?=(.+))/cg) {
                    my ($l, $s, $n) = ($1, $2, $3);
                    $result .= $l;
                    if ($n =~ /^ /) {
                        $result .= $self->config('Newline');
                    } elsif (length $s) {
                        $result .= $s;
                    } elsif (length $l) {
                        $result .= ' '
                            if $self->breakingRule($l, $n) == INDIRECT;
                    }
                } elsif ($s =~ /\G(.+)\n/cg) {
                    $result .= $1.$self->config('Newline');
                } elsif ($s =~ /\G(.+)/cg) {
                    $result .= $1.$self->config('Newline');
                    last;
                }
            }
        } elsif ($method eq 'FLOWED' or $method eq 'FLOWEDSP' or
                 $method eq 'OBSFLOWED') {
            my $prefix = undef;
            pos($s) = 0;
            while ($s !~ /\G\z/cg) {
                if ($s =~ /\G(>+) ?(.*?)( ?)\n/cg) {
                    my ($p, $l, $s) = ($1, $2, $3);
                    unless (defined $prefix) {
                        $result .= $p.' '.$l;
                    } elsif ($p ne $prefix) {
                        $result .= $self->config('Newline');
                        $result .= $p.' '.$l;
                    } else {
                        $result .= $l;
                    }
                    unless (length $s) {
                        $result .= $self->config('Newline');
                        $prefix = undef;
                    } else {
                        $prefix = $p;
                        $result .= $s unless $delsp;
                    }
                } elsif ($s =~ /\G ?(.*?)( ?)\n/cg) {
                    my ($l, $s) = ($1, $2);
                    unless (defined $prefix) {
                        $result .= $l;
                    } elsif ('' ne $prefix) {
                        $result .= $self->config('Newline');
                        $result .= $l;
                    } else {
                        $result .= $l;
                    }
                    unless (length $s) {
                        $result .= $self->config('Newline');
                        $prefix = undef;
                    } else {
                        $result .= $s unless $delsp;
                        $prefix = '';
                    }
                } elsif ($s =~ /\G ?(.*)/cg) {
                    $result .= $1.$self->config('Newline');
                    last;
                }
            }
        }
    }
    ## Encode result.
    if ($self->{OutputCharset} eq '_UNICODE_') {
        return $result;
    } else {
        return $self->{_charset}->encode($result);
    }
}

=head1 BUGS

Please report bugs or buggy behaviors to developer.

CPAN Request Tracker:
L<http://rt.cpan.org/Public/Dist/Display.html?Name=Unicode-LineBreak>.

=head1 VERSION

Consult $VERSION variable.

=head1 SEE ALSO

L<Unicode::LineBreak>, L<Text::Wrap>.

=head1 AUTHOR

Copyright (C) 2009-2012 Hatuka*nezumi - IKEDA Soji <hatuka(at)nezumi.nu>.

This program is free software; you can redistribute it and/or modify it 
under the same terms as Perl itself.

=cut

1;
