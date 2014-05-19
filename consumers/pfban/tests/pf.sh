#!/bin/bash

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))

. ${TESTDIR}/assert.sh.inc

skipUnlessBinaryExists pfctl

${TESTDIR}/../pftest -e pf

assertExitValue "PF" "pfctl -t ${PFBAN_TEST_TABLE} -T test 1.2.3.4 &> /dev/null" 0

pfctl -t "${PFBAN_TEST_TABLE}" -T delete 1.2.3.4
