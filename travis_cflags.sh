#!/bin/bash
#
# Set CFLAGS to be strict for supported versions on Travis

#containsElement () {
#  local e
#  for e in "${@:2}"; do [[ "$e" == "$1" ]] && return 0; done
#  return 1
#}


# Bash return statements are not useful. You have to echo the value
# and then capture it by calling the function with $( foo )
function contains() {
    local n=$#
    local value=${!n}
    for ((i=1;i < $#;i++)) {
        if [ "${!i}" == "${value}" ]; then
            echo "1"
            return 0
        fi
    }
    echo "0"
    return 0
}

strictPHPVersions=()
strictPHPVersions+=("5.3")
strictPHPVersions+=("5.4")
strictPHPVersions+=("5.5")
strictPHPVersions+=("5.6")
strictPHPVersions+=("7.0")
strictPHPVersions+=("7.1.0alpha1")

echo "TRAVIS_PHP_VERSION is ${TRAVIS_PHP_VERSION}"

strictPHP=$(contains "${strictPHPVersions[@]}" "${TRAVIS_PHP_VERSION}" )

echo "strictPHP is ${strictPHP}"

if [[ $strictPHP = '1' ]]; then
    CFLAGS="-Wno-deprecated-declarations -Wdeclaration-after-statement -Werror -Wall";
else
    CFLAGS="-Wno-deprecated-declarations";
fi

echo "Setting CFLAGS to ${CFLAGS}"

export CFLAGS=$CFLAGS
