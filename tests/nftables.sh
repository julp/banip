#!/bin/bash

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))

. ${TESTDIR}/assert.sh.inc

skipUnlessBinaryExists nft

${TESTDIR}/../pftest nftables

assertOutputValue "nftables" "nft list set filter ${PFBAN_TEST_TABLE} 2>/dev/null | grep -cF 1.2.3.4" 1 "-eq"

nft delete element filter "${PFBAN_TEST_TABLE}" "{ 1.2.3.4 }" &> /dev/null
