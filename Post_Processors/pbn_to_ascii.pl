#!/usr/bin/perl
eval 'exec /usr/local/appl/bin/perl  -S $0 ${1+"$@"}'
    if 0; # not running under some shell

# dealpbn takes a PBN file and generates hands as ascii in square layout
# relevant aspects of PBM format were quessed from the output of dealer
# e.g.
#	dealer Descr.pbn | dealpbn
#
# Robin Barker (Robin.Barker@NPL.co.uk), 1999/03/10

use strict;

local $/ = '';		# paragraph mode

my $VERSION 	= 0.1;

my $compass_char = "NESW";
my @compass_list = split //, $compass_char;
my %compass_rank;
@compass_rank{@compass_list} = 0 .. $#compass_list;

my %hands;

sub printhand { 
    my $h = shift ;
    foreach my $i (@{$h}) { print "\t".($i || "--")."\n"}
};

sub printhands {
    my($h1,$h2) = @_;
    foreach my $i (0..3){ printf "%-16s%s\n", $h1->[$i] || '--', $h2->[$i] || '--'; }
}

while( <> ) {
    die $_ unless /\A(\[\w+\s+"[^"]*"\]\s+)+\Z/;
    my %info;
    while( /\[(\w+)\s+"([^"]*)"\]/g ) { $info{$1} = $2; }
    die "No Deal\n$_" unless $_ = $info{Deal};
    my $first = s/^([$compass_char]):// ? $1 : ($info{Dealer} || 'N');
    my %hands;
    @hands{ map { $compass_list[($compass_rank{$first} + $_) % 4] } 0 .. 3 } = 
	map { [ (split /\./, $_, 4) ] } split;
    
    printhand $hands{'N'};
    printhands $hands{'W'}, $hands{'E'};
    printhand $hands{'S'};
    print "\n";
}
