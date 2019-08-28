#!/bin/bash
# Minispec release script
scons -j16
strip msc
RELDIR=minispec_r`python misc/gitver.py | cut -d: -f2`
rm -rf $RELDIR
mkdir $RELDIR
cp -r examples tests misc src README.md SConstruct msc $RELDIR/
env GZIP=-9 tar czvf $RELDIR.tar.gz $RELDIR/
