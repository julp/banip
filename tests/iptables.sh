#!/bin/bash

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))

. ${TESTDIR}/assert.sh.inc

skipUnlessBinaryExists iptables

${TESTDIR}/../pftest iptables

iptables -N ${PFBAN_TEST_TABLE}
iptables -A ${PFBAN_TEST_TABLE} -j RETURN -p tcp --dport http
iptables -I INPUT -j ${PFBAN_TEST_TABLE}

assertOutputValue "iptables" "iptables -nL 2>/dev/null | grep -cF 1.2.3.4" 1 "-eq"

iptables -D ${PFBAN_TEST_TABLE} -s 1.2.3.4 -j DROP

iptables -j ${PFBAN_TEST_TABLE} -D INPUT -p tcp --dport http
iptables -F ${PFBAN_TEST_TABLE}
iptables -X ${PFBAN_TEST_TABLE}
