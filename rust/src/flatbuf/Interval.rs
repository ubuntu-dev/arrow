//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

flatbuffers_object!{Table => Interval [
 field => { name = unit,
            typeOf = enum IntervalUnit i16,
            slot = 4,
            default = 0 }]}

/// Builder Trait for `Interval` tables.
pub trait IntervalBuilder {
    fn start_interval(&mut self);
    /// Set the value for field `unit`.
    fn add_unit(&mut self, unit: i16);
}

impl IntervalBuilder for flatbuffers::Builder {
    fn start_interval(&mut self) {
        self.start_object(1);
    }

    fn add_unit(&mut self, unit: i16) {
        self.add_slot_i16(0, unit, 0)
    }

}

