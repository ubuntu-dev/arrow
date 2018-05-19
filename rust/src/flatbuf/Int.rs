//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

flatbuffers_object!{Table => Int [
 field => { name = bitWidth,
            typeOf = i32,
            slot = 4,
            default = 0 }, 
 field => { name = is_signed,
            typeOf = bool,
            slot = 6,
            default = false }]}

/// Builder Trait for `Int` tables.
pub trait IntBuilder {
    fn start_int(&mut self);
    /// Set the value for field `bitWidth`.
    fn add_bitWidth(&mut self, bitWidth: i32);
    /// Set the value for field `is_signed`.
    fn add_is_signed(&mut self, is_signed: bool);
}

impl IntBuilder for flatbuffers::Builder {
    fn start_int(&mut self) {
        self.start_object(2);
    }

    fn add_bitWidth(&mut self, bitWidth: i32) {
        self.add_slot_i32(0, bitWidth, 0)
    }

    fn add_is_signed(&mut self, is_signed: bool) {
        self.add_slot_bool(1, is_signed, false)
    }

}

