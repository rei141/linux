#!/bin/bash

set -eux

# 引数のチェック
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <clean|make|run>"
    exit 1
fi

ACTION=$1

case $ACTION in
    clean)
        # clean is for applying changes on coverage.h
        make -C tools/testing/selftests/ TARGETS=kvm clean
        ;;
    make)
        # build
        make -C tools/testing/selftests/ TARGETS=kvm
        ;;
    run)
        # run
        sudo make -C tools/testing/selftests/ TARGETS=kvm run_tests
        ;;
    *)
        echo "Invalid action: $ACTION"
        echo "Usage: $0 <clean|make|run>"
        exit 1
        ;;
esac
