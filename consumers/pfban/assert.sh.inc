#!/bin/bash

declare -ri TRUE=0
declare -ri FALSE=1

trap "rm -f /tmp/${PPID}.*" EXIT ERR SIGINT

__assertTrue() {
    if eval $2; then
        printf "%s, %s: [ \e[%d;01m%s\e[0m ]\n" `basename $0` "$1" 32 OK
        return $TRUE
    else
        #echo -e "`basename $0`: \e[31;01mKO\e[0m"
        printf "%s, %s: [ \e[%d;01m%s\e[0m ]\n" `basename $0` "$1" 31 KO
        echo "assertion: $2 fails"
        return $FALSE
    fi
}

assertExitValue() {
    if [ $# -lt 3 ]; then
        echo "ArgumentError: ${FUNCNAME[0]}(message, command, expected value[, operator = 'eq'])"
        return $FALSE
    fi
    eval $2
    __assertTrue "$1" "[ \$? ${4:--eq} $3 ]"

    return $?
}

assertOutputValue() {
    if [ $# -lt 3 ]; then
        echo "ArgumentError: ${FUNCNAME[0]}(message, command1, value[, operator = '=='])"
        return $FALSE
    fi
    __assertTrue "$1" "[[ \`$2\` ${4:-==} '$3' ]]"

    if [ $? -ne $TRUE -a -n "${DEBUG}" ]; then
        X=$(${2% 2> /dev/null})
        echo "Get    : '$X'"
        echo "Expect : '$3'"
    fi

    return $?
}

assertOutputCommand() {
    if [ $# -lt 3 ]; then
        echo "ArgumentError: ${FUNCNAME[0]}(message, command1, command2[, operator = '=='])"
        return $FALSE
    fi
    __assertTrue "$1" "[[ \`$2\` ${4:-==} \`$3\` ]]"

    return $?
}

assertOutputValueEx() {
    local tmp1 tmp2

    if [ $# -ne 3 ]; then
        echo "ArgumentError: ${FUNCNAME[0]}(message, command1, command2)"
        return $FALSE
    fi

    tmp1=`mktemp /tmp/${PPID}.XXXXXX`
    tmp2=`mktemp /tmp/${PPID}.XXXXXX`
    eval "\`$2 > $tmp1\`"
    eval "\`$3 > $tmp2\`"

    assertExitValue "$1" "diff -quN ${tmp1} ${tmp2} > /dev/null" $TRUE

    if [ $? -ne $TRUE -a -n "${DEBUG}" ]; then
        diff --text -uN "${tmp1}" "${tmp2}"
    fi

    return $?
}

assertOutputValueExIgnoreBlanks() {
    local tmp1 tmp2

    if [ $# -ne 3 ]; then
        echo "ArgumentError: ${FUNCNAME[0]}(message, command1, command2)"
        return $FALSE
    fi

    tmp1=`mktemp /tmp/${PPID}.XXXXXX`
    tmp2=`mktemp /tmp/${PPID}.XXXXXX`
    eval "\`$2 > $tmp1\`"
    eval "\`$3 > $tmp2\`"

    assertExitValue "$1" "diff -bquN ${tmp1} ${tmp2} > /dev/null" $TRUE

    if [ $? -ne $TRUE -a -n "${DEBUG}" ]; then
        diff --text -uN ${tmp1} ${tmp2}
    fi

    return $?
}
