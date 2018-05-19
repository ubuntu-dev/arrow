//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

flatbuffers_object!{Table => FixedSizeBinary [
 field => { name = byteWidth,
            typeOf = i32,
            slot = 4,
            default = 0 }]}

/// Builder Trait for `FixedSizeBinary` tables.
pub trait FixedSizeBinaryBuilder {
    fn start_fixedsizebinary(&mut self);
    /// Set the value for field `byteWidth`.
    fn add_byteWidth(&mut self, byteWidth: i32);
}

impl FixedSizeBinaryBuilder for flatbuffers::Builder {
    fn start_fixedsizebinary(&mut self) {
        self.start_object(1);
    }

    fn add_byteWidth(&mut self, byteWidth: i32) {
        self.add_slot_i32(0, byteWidth, 0)
    }

}

