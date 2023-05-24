set -eux

mkdir -p ./selftest_coverage

# build
# clean is for applying changes on coverage.h
if [ $# -gt 0 ] && [ $1 -eq "c" ]; then
    make -C tools/testing/selftests/ TARGETS=kvm clean
fi

make -C tools/testing/selftests/ TARGETS=kvm
# run
sudo make -C tools/testing/selftests/ TARGETS=kvm run_tests
