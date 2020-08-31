# Copyrights 2007-2019 by [Mark Overmeer <markov@cpan.org>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report. Meta-POD processed with
# OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Log::Report::Message;
use vars '$VERSION';
$VERSION = '1.28';


use warnings;
use strict;

use Log::Report 'log-report';
use POSIX             qw/locale_h/;
use List::Util        qw/first/;
use Scalar::Util      qw/blessed/;

use Log::Report::Util qw/to_html/;

# Work-around for missing LC_MESSAGES on old Perls and Windows
{ no warnings;
  eval "&LC_MESSAGES";
  *LC_MESSAGES = sub(){5} if $@;
}


use overload
    '""'  => 'toString'
  , '&{}' => sub { my $obj = shift; sub{$obj->clone(@_)} }
  , '.'   => 'concat'
  , fallback => 1;


sub new($@)
{   my ($class, %s) = @_;

    if(ref $s{_count})
    {   my $c        = $s{_count};
        $s{_count}   = ref $c eq 'ARRAY' ? @$c : keys %$c;
    }

    defined $s{_join}
        or $s{_join} = $";

    if($s{_msgid})
    {   $s{_append}  = defined $s{_append} ? $1.$s{_append} : $1
            if $s{_msgid} =~ s/(\s+)$//s;

        $s{_prepend}.= $1
            if $s{_msgid} =~ s/^(\s+)//s;
    }
    if($s{_plural})
    {   s/\s+$//, s/^\s+// for $s{_plural};
    }

    bless \%s, $class;
}

# internal use only: to simplify __*p* functions
sub _msgctxt($) {$_[0]->{_msgctxt} = $_[1]; $_[0]}


sub clone(@)
{   my $self = shift;
    (ref $self)->new(%$self, @_);
}


sub fromTemplateToolkit($$;@)
{   my ($class, $domain, $msgid) = splice @_, 0, 3;
    my $plural = $msgid =~ s/\|(.*)// ? $1 : undef;
    my $args   = @_ && ref $_[-1] eq 'HASH' ? pop : {};

    my $count;
    if(defined $plural)
    {   @_==1 or $msgid .= " (ERROR: missing count for plural)";
        $count = shift || 0;
        $count = @$count if ref $count eq 'ARRAY';
    }
    else
    {   @_==0 or $msgid .= " (ERROR: only named parameters expected)";
    }

    $class->new
      ( _msgid => $msgid, _plural => $plural, _count => $count
      , %$args, _expand => 1, _domain => $domain);
}

#----------------

sub prepend() {shift->{_prepend}}
sub msgid()   {shift->{_msgid}}
sub append()  {shift->{_append}}
sub domain()  {shift->{_domain}}
sub count()   {shift->{_count}}
sub context() {shift->{_context}}
sub msgctxt() {shift->{_msgctxt}}


sub classes()
{   my $class = $_[0]->{_class} || $_[0]->{_classes} || [];
    ref $class ? @$class : split(/[\s,]+/, $class);
}


sub to(;$)
{   my $self = shift;
    @_ ? $self->{_to} = shift : $self->{_to};
}


sub valueOf($) { $_[0]->{$_[1]} }

#--------------

sub inClass($)
{   my @classes = shift->classes;
       ref $_[0] eq 'Regexp'
    ? (first { $_ =~ $_[0] } @classes)
    : (first { $_ eq $_[0] } @classes);
}
    

sub toString(;$)
{   my ($self, $locale) = @_;
    my $count  = $self->{_count} || 0;
	$locale    = $self->{_lang} if $self->{_lang};

    my $prepend = $self->{_prepend} // '';
    my $append  = $self->{_append}  // '';

	if(blessed $prepend) {
        $prepend = $prepend->isa(__PACKAGE__) ? $prepend->toString($locale)
          : "$prepend";
	}
	if(blessed $append) {
        $append  = $append->isa(__PACKAGE__) ? $append->toString($locale)
          : "$append";
	}

    $self->{_msgid}   # no translation, constant string
        or return "$prepend$append";

    # assumed is that switching locales is expensive
    my $oldloc = setlocale(LC_MESSAGES);
    setlocale(LC_MESSAGES, $locale)
        if defined $locale && (!defined $oldloc || $locale ne $oldloc);

    # translate the msgid
	my $domain = $self->{_domain};
	$domain    = textdomain $domain
        unless blessed $domain;

    my $format = $domain->translate($self, $locale || $oldloc);
    defined $format or return ();

    # fill-in the fields
	my $text = $self->{_expand}
      ? $domain->interpolate($format, $self)
      : "$prepend$format$append";

    setlocale(LC_MESSAGES, $oldloc)
        if defined $oldloc && (!defined $locale || $oldloc ne $locale);

    $text;
}



my %tohtml = qw/  > gt   < lt   " quot  & amp /;

sub toHTML(;$) { to_html($_[0]->toString($_[1])) }


sub untranslated()
{  my $self = shift;
     (defined $self->{_prepend} ? $self->{_prepend} : '')
   . (defined $self->{_msgid}   ? $self->{_msgid}   : '')
   . (defined $self->{_append}  ? $self->{_append}  : '');
}


sub concat($;$)
{   my ($self, $what, $reversed) = @_;
    if($reversed)
    {   $what .= $self->{_prepend} if defined $self->{_prepend};
        return ref($self)->new(%$self, _prepend => $what);
    }

    $what = $self->{_append} . $what if defined $self->{_append};
    ref($self)->new(%$self, _append => $what);
}

#----------------

1;
