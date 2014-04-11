#!/bin/bash

if [ ! -x /usr/sbin/npfctl ]; then
    exit 0
fi

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))

. ${TESTDIR}/assert.sh.inc

${TESTDIR}/pftest

assertExitValue "NPF" "npfctl table blacklist test 1.2.3.4 &> /dev/null" 0

npfctl table blacklist rem 1.2.3.4
