#if __cpp_lib_parallel_algorithm < 201603
namespace std {
    template<class InputIt, class T, class BinaryOp>
    T reduce(InputIt first, InputIt last, T init, BinaryOp binary_op) {
        return std::accumulate(first, last, init, binary_op);
    }
}
#endif
