#!/bin/bash

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))

. ${TESTDIR}/assert.sh.inc

skipUnlessBinaryExists npfctl

${TESTDIR}/pftest -e npf

TABLE="${PFBAN_TEST_TABLE}"
ntpctl show | grep -qF "<${TABLE}>"
if [ $? -ne 0 ]; then
        TABLE="0"
fi

assertExitValue "NPF" "npfctl table ${TABLE} test 1.2.3.4 &> /dev/null" 0
npfctl table ${TABLE} rem 1.2.3.4
