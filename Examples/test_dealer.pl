#!/usr/bin/perl

my ($exe, $file) = @ARGV;

if ($file eq "") {
  $file = "Descr.*";
}

if ($exe eq "") {
  $exe = "dealer"
}

$exe = "../$exe";

print $exe
#
# Fixed seed so that the output should be the same from run to run
#
$seed = 1;

$exitcode = 0;

foreach $input (`ls $file`) {
  # Loop over all files that start with Descr.
  chop $input;
  print "  TEST   $input\n";

  $output = $input;
  $output =~ s/Descr/Output/;
  $refer  = $input;
  $refer  =~ s/Descr/Refer/;

  system ("$exe -s $seed $input 2>$output.err >$output");

  if (-s "$output.err" == 0) {
    unlink("$output.err");
  }

  if (-e $refer) {
    $diff = `diff $output $refer`;
    $diff =~ s/^(.*Time needed.*|.*\[Date ".*|[^<>].*)\R//mg;
    if (-e "$refer.err" || -e "$output.err") {
      if (-e "$refer.err" && -e "$output.err") {
        $err = `diff $output.err $refer.err`;
      } else { if (-e "$output.err") {
          print "$refer.err missing\n";
          $err = `cat "$output.err"`;
        } else {
          print "$output.err missing\n";
          $err = `cat $refer.err`;
        }
      }
      if ($err ne "") {
        print "stderr not matching\n";
        print $err;
        $exitcode = 2;
      }
    }
    if ($diff ne "") {
      print "output not matching\n";
      print $diff;
      $exitcode = 1;
    }
  } else {
    print "$refer is missing\n";
  }

}
exit $exitcode;
