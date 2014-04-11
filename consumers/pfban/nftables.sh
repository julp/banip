#!/bin/bash

if [ ! -x /sbin/nft ]; then
    exit 0
fi

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))

. ${TESTDIR}/assert.sh.inc

${TESTDIR}/pftest

assertOutputValue "nftables" "nft list set filter blacklist 2>/dev/null | grep -cF 1.2.3.4" 1 "-eq"

nft delete element filter blacklist "{ 1.2.3.4 }" &> /dev/null
