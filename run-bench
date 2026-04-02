#!/usr/bin/perl

use strict;
use warnings;

my $volout = '';
my $check = '';
my $analyse = '';
my $RabbitRunner = './RabbitRunner/RabbitRunnerCT';
my $PIN = '~/likwid/CPU/bin/likwid-pin';
#my $PIN = 'likwid-pin';

if ($#ARGV < 1) {
    die "Usage ./run <module> <size> <numProc>\n";
}

my $benchmark = $ARGV[0];
my $size = $ARGV[1];
my $numProc = $ARGV[2];

#$volout = "-o $benchmark-$size.vol";
$check = "-c ./RabbitInput/Reference".$size.".vol";
$analyse = "-a ./RabbitInput/RabbitGeometry.rct";

system("$PIN -c N:0-".$numProc."  $RabbitRunner -b 1 -m ./$benchmark/$benchmark.so $analyse $volout  -i ./RabbitInput/RabbitInput.rct  $check -s $size");

