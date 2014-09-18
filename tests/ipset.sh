#!/bin/bash

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))

. ${TESTDIR}/assert.sh.inc

skipUnlessBinaryExists ipset

if ! zgrep -q ^CONFIG_IP_SET /proc/config.gz; then
    printf "%s: [ \e[%d;01m%s\e[0m ] %s\n" `basename $0` 33 SKIPPED "(kernel compiled without CONFIG_IP_SET option)"
    exit $TRUE
fi

${TESTDIR}/../pftest ipset

ipset -! create ${PFBAN_TEST_TABLE} hash:net family inet

assertExitValue "ipset" "ipset test ${PFBAN_TEST_TABLE} 1.2.3.4" $TRUE

ipset destroy ${PFBAN_TEST_TABLE}
