//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

flatbuffers_object!{Table => FloatingPoint [
 field => { name = precision,
            typeOf = enum Precision i16,
            slot = 4,
            default = 0 }]}

/// Builder Trait for `FloatingPoint` tables.
pub trait FloatingPointBuilder {
    fn start_floatingpoint(&mut self);
    /// Set the value for field `precision`.
    fn add_precision(&mut self, precision: i16);
}

impl FloatingPointBuilder for flatbuffers::Builder {
    fn start_floatingpoint(&mut self) {
        self.start_object(1);
    }

    fn add_precision(&mut self, precision: i16) {
        self.add_slot_i16(0, precision, 0)
    }

}

