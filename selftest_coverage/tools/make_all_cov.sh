cat arch_* > tmp
cat tmp | sort | uniq > all_kvm_arch
tools/cov2nested.sh all_kvm_arch cov_all_kvm_arch
cat kvm_* > tmp
cat tmp | sort | uniq > all_kvm
# ./cov2nested.sh all_kvm cov_all_kvm
