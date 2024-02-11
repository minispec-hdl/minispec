#!/bin/bash

# $lic$
# Copyright (C) 2019-2024 by Daniel Sanchez
#
# This file is part of the Minispec compiler and toolset.
#
# Minispec is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, version 2.
#
# Minispec is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program. If not, see <http://www.gnu.org/licenses/>.

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
sudo -H pip install .
