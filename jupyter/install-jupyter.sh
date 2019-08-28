#!/bin/bash

# Download Jupyter, patch with Minispec codemirror language, build and install
# This is needed because the jupyter distrbution has multiple minified js files
# with codemirror code, so you can't properly install a new syntax file without
# rebuilding the frontend

SRC_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

wget -nv https://github.com/jupyter/notebook/archive/5.7.8.tar.gz
tar xfz 5.7.8.tar.gz
cd notebook-5.7.8/
# Fix preact breakage: https://github.com/jupyter/notebook/issues/4680
sed -i 's/@^/@~/g' bower.json

# Get deps
npm install
./node_modules/bower/bin/bower install

# Install minispec codemirror syntax file
mkdir notebook/static/components/codemirror/mode/minispec
cp $SRC_DIR/minispec.js notebook/static/components/codemirror/mode/minispec/
(cd notebook/static/components/codemirror/mode/ && patch < $SRC_DIR/meta.js.patch)

# Install
sudo pip install .
