#include "annotated_merge.cpp"
#include <bitset>

// TODO: Rework for custom header width sizes (!= unsigned int)
const unsigned int Bitwidth = 64;
const unsigned int Headerwidth = sizeof(unsigned int) * 8;
const unsigned int Datawidth = Bitwidth - Headerwidth;
using IntType = ap_uint<Bitwidth>;
using Stream = hls::stream<IntType>;

unsigned int errors = 0;

/**
 * Filter the data from the output
 */
template<typename T, unsigned int DATA_WIDTH>
T get_data(T output) {
    T mask = (static_cast<T>(1) << DATA_WIDTH) - 1;
    return output & mask;
}

/**
 * Filter the source from the output
*/
template<typename T, unsigned int DATA_WIDTH, unsigned int HEADER_WIDTH>
T get_source(T output) {
    T mask = ((static_cast<T>(1) << HEADER_WIDTH) - 1) << DATA_WIDTH;
    return (output & mask) >> DATA_WIDTH;
}


/**
 * Test the case that all streams always have data. Expect full round-robin
 */
template<unsigned int N>
void test_continuous(unsigned int iterations, bool print_on_success) {
    Stream streams[N];
    Stream out;
    std::vector<unsigned int> expected_results;
    std::vector<unsigned int> expected_sources;
    for (unsigned int i = 0; i < iterations; i++) {
        for (unsigned int j = 0; j < N; j++) {
            auto data = (j+1) * i;
            streams[j].write(data);
            
            // in perfect RR we can simply expect data in the order we added it
            expected_results.push_back(data);
            expected_sources.push_back(j);
        }
    }
    RoundRobinAnnotatedMerger<N, IntType, Bitwidth> merger;
    for (unsigned int i = 0; i < iterations; i++) {
        for (unsigned int j = 0; j < N; j++) {
            merger.read_from(streams[j]);
        }
    }
    for (unsigned int i = 0; i < iterations * N; i++) {
        merger.write_into(out);
    }
    IntType output, outdata, outsource;
    for (unsigned int i = 0; i < expected_results.size(); i++) {
        output = out.read();
        outdata = get_data<IntType, Datawidth>(output);
        outsource = get_source<IntType, Datawidth, Headerwidth>(output);
        if (outdata != expected_results[i] || outsource != expected_sources[i]) {
            std::cout << "[" << i << "]   ";
            std::cout << "MISMATCH: Expected data, source: (" << expected_results[i] << ", " << expected_sources[i] << ") but got ";
            std::cout << "(" << outdata << ", " << outsource << ")    Bin: "<< (std::bitset<Bitwidth>(output)) << std::endl;
            errors++;
        } else {
            if (print_on_success) {
                std::cout << "[" << i << "] Match (" << outdata << ", " << outsource << ")" << "\n";
            }
        }
    }
    if (!out.empty()) {
        std::cout << "Expected output stream to be empty, but it still has data!" << std::endl;
    }
}

int main() {
    std::cout << "\n\nTest continuous case with 1, 2, 3, 20 streams\n-----------------------------\n";
    test_continuous<1>(10, false);
    test_continuous<2>(10, false);
    test_continuous<3>(10, false);
    test_continuous<20>(10, false);
    if (errors > 0) {
        throw std::runtime_error("There were errors during simulation. Check the logs.");
    } else {
        std::cout << "---------------------\nNo errors found during simulation\n";
    }
    return 0;
}