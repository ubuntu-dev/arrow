//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

/// ----------------------------------------------------------------------
/// A Buffer represents a single contiguous memory segment
flatbuffers_object!{Struct => Buffer ( size:16, align: 8) [
 field => { name = offset,
            typeOf = i64,
            slot = 0,
            default = 0 }, 
 field => { name = length,
            typeOf = i64,
            slot = 8,
            default = 0 }]}

pub trait BufferBuilder {
    fn build_buffer(&mut self, offset: i64, length: i64) -> flatbuffers::UOffsetT;
}

impl BufferBuilder for flatbuffers::Builder {
    fn build_buffer(&mut self, offset: i64, length: i64) -> flatbuffers::UOffsetT {
        self.prep(8, 16);
        self.add_i64(length);
        self.add_i64(offset);
        self.offset() as flatbuffers::UOffsetT 
    }
}

