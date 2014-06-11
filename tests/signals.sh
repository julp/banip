#!/bin/bash

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))

. ${TESTDIR}/assert.sh.inc

${TESTDIR}/../banipd -d -q /test -t dummy -e dummy -p ${TESTDIR}/test.pid
kill -USR1 `cat ${TESTDIR}/test.pid`
sleep 2
assertExitValue "Signal handling (USR1)" "kill -0 `cat ${TESTDIR}/test.pid`" $TRUE
assertExitValue "Signal handling (TERM)" "kill -TERM `cat ${TESTDIR}/test.pid`" $TRUE
