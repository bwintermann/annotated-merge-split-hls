#include "ap_int.h"
#include "hls_stream.h"
#include <utility>
#include <algorithm>

// TODO: Templated / paramter packed version

/***
 * Usage:
 * 
 * void top(hls::stream<..> &in1, hls::stream<...> &in2, ...,  hls::stream<...> &out) {
 *      RoundRobinAnnotatedMerger<..., ap_uint<...>, ...> merger;
 *      while (true) {
 *          merger.read_from(in1);
 *          merger.read_from(in2);
 *          ...
 *          merger.write_into(out); 
 *      }
 * }
 */
template<unsigned int N, typename T>
class RoundRobinAnnotatedMerger {
    private:
        // Current stream to be assigned to
        unsigned int current_read_stream = 0;

        // All streams
        // TODO: Different bitwidths
        hls::stream<T> in_streams[N];
        
        // TODO: Optimize datawidth
        // The current round-robin selected candidate. Moves after every write_into() call
        unsigned int rr_candidate = 0;

    public:

        RoundRobinAnnotatedMerger() = default;

        // Width to shift the header so that it is at MSB
        const unsigned int HEADER_SHIFT = (T::width - sizeof(rr_candidate) * 8);
    
        /***
         * Read from the given stream into the merger's stream. Is non-blocking. Internally increments the current stream being read. => Needs to be
         * called EXACTLY once per all the N kernels, otherwise the indexing might be off.
         */
        void read_from(hls::stream<T> &in) {
            T buffer;
            if(in.read_nb(buffer)) {
                in_streams[current_read_stream].write_nb(buffer);
            }
            current_read_stream = (current_read_stream + 1) % N;
        } 

        /***
         * (Non-blocking) Try to write data from the current round-robin candidate into the target stream.
         * If not available, the next stream with data is used. Advances the round-robin candidate if
         * data was sent. In case that no streams have data returns false, else true.
         */
        bool write_into(hls::stream<T> &target) {
            // TODO: Add pragma where applicable
            #ifndef __SYNTHESIS__
                if (current_read_stream != 0) { throw std::runtime_error("Make sure to read every stream exactly once before trying to merge!"); }
            #endif
            if (std::all_of(in_streams, in_streams+N, [](auto& st){ return st.empty(); })) {
                return false;
            }
            T data;
            for (unsigned int index = rr_candidate; index != rr_candidate - 1; index = (index + 1) % N) {
                if (!in_streams[index].empty()) {
                    data = index;
                    data = (data << HEADER_SHIFT) | in_streams[index].read();
                    target.write(data);
                    rr_candidate = (rr_candidate + 1) % N;
                    return true;
                }
            }
            return false;
        }
};

template<unsigned int N, typename T>
class AnnotatedSplitter {
    public:
        AnnotatedSplitter() = default;

        static const unsigned int DATAWIDTH = T::width - sizeof(unsigned int) * 8;

        hls::stream<T> out_streams[N];

        /**
         * Get the header from the incoming data. TODO: Fix for any width of source ID
         */
        static unsigned int get_data_header(T incoming_data) {
            // TODO: Flexible source id bitwidth
            return static_cast<unsigned int>(incoming_data >> DATAWIDTH);
        }

        /**
         * Get the contents of an incoming data package, without the header
         */
        static T get_data_contents(T incoming_data) {
            return incoming_data & ((static_cast<T>(1) << DATAWIDTH) - 1);
        }

        /**
         * Read and demux if data is available. If the source ID is unknown do nothing and return false. Also return false if no data is available
         */
        bool try_read_and_demux(hls::stream<T> &incoming) {
            T data;
            if (incoming.read_nb(data)) {
                auto source = AnnotatedSplitter<N, T>::get_data_header(data);
                auto contents = AnnotatedSplitter<N, T>::get_data_contents(data);
                if (source >= N) {
                    return false;
                }
                out_streams[source].write(contents);
                return true;
            }
            return false;
        }
};