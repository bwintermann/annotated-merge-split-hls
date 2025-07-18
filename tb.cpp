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
 * Helper function to let the merger merge a certain number of times into the target outgoing stream
 */
template<unsigned int N, typename T, unsigned int TW>
void _merge_times(RoundRobinAnnotatedMerger<N, T, TW> &merger, unsigned int times, hls::stream<T> &target) {
    for (unsigned int i = 0; i < times; i++) {
        merger.write_into(target);
    }
}

/**
 * Helper function to split a certain number of times. Does not check for success of reads
 */
template<unsigned int N, typename T, unsigned int TW>
void _split_times(AnnotatedSplitter<N, T, TW> &splitter, unsigned int times, hls::stream<T> &source) {
    for (unsigned int i = 0; i < times; i++) {
        splitter.try_read_and_demux(source);
    }
}

/**
 * Test the case that all streams always have data. Expect full round-robin
 */
template<unsigned int N>
void test_continuous_roundrobin_merger_only(unsigned int iterations, bool print_on_success) {
    Stream streams[N];
    Stream out;
    std::vector<unsigned int> expected_results;
    std::vector<unsigned int> expected_sources;
    
    // Fill incoming streams with data
    for (unsigned int i = 0; i < iterations; i++) {
        for (unsigned int j = 0; j < N; j++) {
            auto data = (j+1) * i;
            streams[j].write(data);
            
            // in perfect RR we can simply expect data in the order we added it
            expected_results.push_back(data);
            expected_sources.push_back(j);
        }
    }

    // Let the merger read all streams and write everything into the output stream
    RoundRobinAnnotatedMerger<N, IntType, Bitwidth> merger;
    for (unsigned int i = 0; i < iterations; i++) {
        for (unsigned int j = 0; j < N; j++) {
            merger.read_from(streams[j]);
        }
    }
    _merge_times(merger, N*iterations, out);

    // Read out results and test manually (do not use AnnotatedSplitter)
    IntType output, outdata, outsource;
    using Splitter = AnnotatedSplitter<N, IntType, Bitwidth>;
    for (unsigned int i = 0; i < expected_results.size(); i++) {
        output = out.read();
        outdata = Splitter::get_data_contents(output);
        outsource = Splitter::get_data_header(output);
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

/**
 * Test the round-robin merge and split (mux and demux), with the merger and splitter classes
 */
template<unsigned int N>
void test_continuous_roundrobin_complete(unsigned int iterations, bool print_on_success) {
    Stream in_streams[N];
    Stream out_streams[N];
    Stream network_stream;
    
    // Fill incoming streams with data
    for (unsigned int iteration = 0; iteration < iterations; iteration++) {
        for (unsigned int streamno = 0; streamno < N; streamno++) {
            auto data = (streamno+1) * iteration;
            in_streams[streamno].write(data);
        }
    }

    // Read everything into the merger
    RoundRobinAnnotatedMerger<N, IntType, Bitwidth> merger;
    for (unsigned int i = 0; i < iterations; i++) {
        for (unsigned int j = 0; j < N; j++) {
            merger.read_from(in_streams[j]);
        }
    }
    // Merge and split everything
    AnnotatedSplitter<N, IntType, Bitwidth> demux;
    _merge_times(merger, N*iterations, network_stream);
    _split_times(demux, N*iterations, network_stream);

    // Check everything
    for (unsigned int iteration = 0; iteration < iterations; iteration++) {
        for (unsigned int streamno = 0; streamno < N; streamno++) {
            auto data_contents = demux.out_streams[streamno].read();
            auto expected = (streamno+1)*iteration;
            if (data_contents != expected) {
                std::cout << "MISMATCH: " << iteration << "th data on stream " << streamno << ": expected value " << expected << " got " << data_contents << std::endl;
                errors++;
            } else {
                if (print_on_success) {
                    std::cout << "Match: Stream " << streamno << " (" << iteration << "th read)" << std::endl;
                }
            }
        }
    }
}


int main() {
    std::cout << "\n\n[Merger only] [Round Robin] [Streams: 1, 2, 3, 20]\n-----------------------------\n";
    test_continuous_roundrobin_merger_only<1>(10, false);
    test_continuous_roundrobin_merger_only<2>(10, false);
    test_continuous_roundrobin_merger_only<3>(10, false);
    test_continuous_roundrobin_merger_only<20>(10, false);
    std::cout << "Done.\n";
    std::cout << "\n\n[Merger and Splitter] [Round Robin] [Streams: 1, 2, 3, 20]\n-----------------------------\n";
    test_continuous_roundrobin_complete<1>(10, false);
    test_continuous_roundrobin_complete<2>(10, false);
    test_continuous_roundrobin_complete<3>(10, false);
    test_continuous_roundrobin_complete<20>(10, false);
    std::cout << "Done.\n";
    if (errors > 0) {
        throw std::runtime_error("There were errors during simulation. Check the logs.");
    } else {
        std::cout << "\n\n---\nNo errors found during simulation\n---\n\n";
    }
    return 0;
}