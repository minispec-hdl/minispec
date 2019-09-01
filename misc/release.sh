#!/bin/bash
# Minispec release script
scons -j16
strip msc
RELDIR=minispec_r`python misc/gitver.py | cut -d: -f2`
rm -rf $RELDIR
mkdir $RELDIR
cp -r examples tests misc src synth docs jupyter syntax README.md SConstruct Vagrantfile msc $RELDIR/
rm $RELDIR/synth/full.lib # it's gigantic and not needed
env GZIP=-9 tar czvf $RELDIR.tar.gz $RELDIR/
