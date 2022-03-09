#!/bin/bash

# Download yosys, patch it appropriately for use with synth, and build it.
# Current patches include:
#  - abc.patch is used to convey enough information that synth can perform Minispec type analysis
#  - yosys_path.patch fixes path handling between yosys and yosys-abc

SRC_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

wget -nc -nv https://github.com/YosysHQ/yosys/archive/yosys-0.8.tar.gz -Otmp_yosys.tar.gz && \
tar xzf tmp_yosys.tar.gz && rm tmp_yosys.tar.gz && \
cd yosys-yosys-0.8 && patch -p1 < ${SRC_DIR}/yosys_path.patch && \
make -j16 yosys-abc && cd abc && git apply ${SRC_DIR}/abc.patch && make clean && \
cd .. && sed -i"" -e "s/ABCREV = ae6716b/ABCREV = default/g" Makefile && \
make clean && mkdir install && PREFIX=`pwd`/install make -j16 install && \
echo "Done"
