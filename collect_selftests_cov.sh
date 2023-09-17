#!/bin/bash

set -eux

usage() {
    echo "Usage: $0 <directory>"
    exit 1
}

if [ "$#" -ne 1 ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    usage
fi

DIR="$1"

if [ ! -d "$DIR" ]; then
    echo "Error: $DIR is not a valid directory."
    exit 1
fi

cat "$DIR"/COVERAGE_ARCH_* > tmp
cat tmp | sort | uniq > all_kvm_arch
cat "$DIR"/COVERAGE_KVM_* > tmp
cat tmp | sort | uniq > all_kvm