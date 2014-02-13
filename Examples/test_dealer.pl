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

sub replaceprogname {
  my ($exe, $file) = @_;
  $cov  = $exe;
  $orig = $exe;
  $cov  =~ s/^.*\/([^\/]*[^.][^c][^o][^vf])(\.cov|\.prof|)$/\1.cov/;
  $orig =~ s/^.*\/([^\/]*[^.][^c].[^v])(\.cov|)$/\1/;
 
  open (IN, "+<$file");
  @lines = <IN>;

  seek IN,0,0;
  foreach $line (@lines) {
    $line =~ s/$orig([^.][^c][^o][^v])/$cov\1/g;
    print IN $line;
  }
  close IN;
}

foreach $input (`ls $file`) {
  # Loop over all files that start with Descr.
  chop $input;
  print "  TEST   $input\n";

  $output = $input;
  $output =~ s/Descr/Output/;
  $refer  = $input;
  $refer  =~ s/Descr/Refer/;
  $params = $input;
  $params =~ s/Descr/Params/;

  if (-e $params) {
    open my $info, $params;
    unlink $output;
    unlink "$output.err";
    while( my $line = <$info>) {
      $line =~ s/\R//;
      my($rule, $arg) = split(',', $line);
      if (eval($rule)) {
        system ("echo $arg $input >> $output");
        system ("$exe $arg $input 2>>$output.err >>$output");
      }
    }
  } else {
    system ("$exe -s $seed $input 2>$output.err >$output");
  }

  replaceprogname($exe, "$output.err");
  replaceprogname($exe, $output);

  if (-s "$output.err" == 0) {
    unlink("$output.err");
  }

  if (-e $refer) {
    $diff = `diff -Z $output $refer`;
    $diff =~ s/^(.*Time needed.*|.*\[Date ".*|[^<>].*)\R//mg;
    if (-e "$refer.err" || -e "$output.err") {
      if (-e "$refer.err" && -e "$output.err") {
        $err = `diff -Z $output.err $refer.err`;
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
