#!/bin/bash

if [ ! -x /sbin/pfctl ]; then
    exit 0
fi

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))

. ${TESTDIR}/assert.sh.inc

${TESTDIR}/pftest

assertExitValue "PF" "pfctl -t blacklist -T test 1.2.3.4 &> /dev/null" 0

pfctl -t blacklist -T delete 1.2.3.4
