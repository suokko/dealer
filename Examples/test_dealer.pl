#!/usr/bin/perl

my ($exe, $file) = @ARGV;

if ($file eq "") {
  $file = "Descr.*";
}

if ($exe eq "") {
  $exe = "../dealer"
}

#
# Fixed seed so that the output should be the same from run to run
#
$seed = 1;

$exitcode = 0;

$cov  = $exe;
$orig = $exe;
$cov  =~ s/^.*[\/\\]([^\/\\]*[^vfe])(\.cov|\.prof|)(\.exe|)$/\1.cov/;
$orig =~ s/^(.*[\/\\])([^\/\\]*[^v](\.cov|)(\.exe|))$/(\1|)\2/;
$wine = '\\b[A-Z]:\\\\.*\\\\dealer.*\\.exe(?!\\\\)';
$features = "(ssse3|sse3|sse2|sse4.1|sse4.2|sse|lzcnt|popcnt|bmi2|avx2|avx|bmi)";

sub sanitize_output {
  my ($out_pipe, $line) = @_;
  $line =~ s/($orig|dealer|$wine)([^.\/][^cp])/$cov\3/mg;

  # Remove version numbers
  # Built: 0.99.0-53-g423341e-dirty
  $line =~ s/(?<=[ (])[0-9]+\.[0-9]+\.[0-9]+(-[0-9]+)?(-g[a-f0-9]+)?(-dirty)?(?=[ )]|\r?$)//mg;
  # Remove cpu features
  $line =~ s/(?<= )$features(,$features)*//mg;

  print $out_pipe $line;
}

my $cpusupports = "";
if (defined $ENV{'CPUSUPPORTS'}) {
  $cpusupports = "CPUSUPPORTS=${ENV{'CPUSUPPORTS'}} "
}

use IPC::Run qw( start pump finish run );

foreach $input (`ls $file`) {
  # Loop over all files that start with Descr.
  chop $input;
  $cmdline = join " ", $0, $exe, $input;
  print "  TEST   $input: $cpusupports$cmdline\n";

  $refer  = $input;
  $refer  =~ s/Descr/Refer/;
  $params = $input;
  $params =~ s/Descr/Params/;

  if (not -e $refer) {
    print "Error: $refer is missing\n";
    $exitcode = 3;
    break;
  }

  # Start diff processes
  my @diff_io = ("diff", "-u", "-Z", "-I", "Time needed*", "-I", "\\[Date ", "$refer", "-");
  my @diff_err = ("diff", "-u", "-Z", "$refer.err", "-");
  @diff_err = ('cat', '-') if (not -e "$refer.err");

  pipe(in_diff_io_read, in_diff_io_write);
  pipe(in_diff_err_read, in_diff_err_write);
  my $h_diff_io = start \@diff_io, '<', \*in_diff_io_read, '>', \*STDOUT;
  my $h_diff_err = start \@diff_err, '<', \*in_diff_err_read, '>', \$out_diff_err;

  if (-e $params) {
    open my $info, $params;
    while( my $line = <$info>) {
      $line =~ s/\R//;
      my($rule, $arg) = split(',', $line, 2);
      if (eval($rule)) {
        $arg =~ s/^ *//;
        print in_diff_io_write "$arg $input\n";
        print in_diff_err_write "$arg $input\n";
        my @dealer = split(/ +/, "$exe $arg $input");
        my $h_dealer = start \@dealer, \undef,
            sub {sanitize_output(in_diff_io_write, $_[0]);},
            sub {sanitize_output(in_diff_err_write, $_[0]);};
        finish $h_dealer;
      }
      pump_nb $h_diff_err;
    }
  } else {
    my @dealer = ($exe, "-s$seed", "-C", "$input");
    my $h_dealer = start \@dealer, \undef,
            sub {sanitize_output(in_diff_io_write, $_[0]);},
            sub {sanitize_output(in_diff_err_write, $_[0]);};
    finish $h_dealer;
  }

  close in_diff_io_write;
  close in_diff_err_write;
  finish $h_diff_io;
  $exitcode += 1 if ($h_diff_io->full_result(0) ne 0);
  finish $h_diff_err;
  $exitcode += 2 if (-e "$refer.err" and $h_diff_err->full_result(0) ne 0);
  $exitcode += 4 if (not -e "$refer.err" and  $out_diff_err ne "");

  print $out_diff_err;
}
exit $exitcode;
