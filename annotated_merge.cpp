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
template<unsigned int N, typename AP_INT_DTYPE, unsigned int AP_INT_BITWIDTH>
class RoundRobinAnnotatedMerger {
    private:
        // Current stream to be assigned to
        unsigned int current_read_stream = 0;

        // All streams
        // TODO: Different bitwidths
        hls::stream<AP_INT_DTYPE> in_streams[N];
        
        // TODO: Optimize datawidth
        // The current round-robin selected candidate. Moves after every write_into() call
        unsigned int rr_candidate = 0;

    public:

        RoundRobinAnnotatedMerger() = default;

        // Width to shift the header so that it is at MSB
        const unsigned int HEADER_SHIFT = (AP_INT_BITWIDTH - sizeof(rr_candidate) * 8);
    
        /***
         * Read from the given stream into the merger's stream. Is non-blocking. Internally increments the current stream being read. => Needs to be
         * called EXACTLY once per all the N kernels, otherwise the indexing might be off.
         */
        void read_from(hls::stream<AP_INT_DTYPE> &in) {
            AP_INT_DTYPE buffer;
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
        bool write_into(hls::stream<AP_INT_DTYPE> &target) {
            // TODO: Add pragma where applicable
            #ifndef __SYNTHESIS__
                if (current_read_stream != 0) { throw std::runtime_error("Make sure to read every stream exactly once before trying to merge!"); }
            #endif
            if (std::all_of(in_streams, in_streams+N, [](auto& st){ return st.empty(); })) {
                return false;
            }
            AP_INT_DTYPE data;
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

class AnnotatedSplitter {
    // TODO
};