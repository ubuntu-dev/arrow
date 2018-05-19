//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

/// Time type. The physical storage type depends on the unit
/// - SECOND and MILLISECOND: 32 bits
/// - MICROSECOND and NANOSECOND: 64 bits
flatbuffers_object!{Table => Time [
 field => { name = unit,
            typeOf = enum TimeUnit i16,
            slot = 4,
            default = 1 }, 
 field => { name = bitWidth,
            typeOf = i32,
            slot = 6,
            default = 32 }]}

/// Builder Trait for `Time` tables.
pub trait TimeBuilder {
    fn start_time(&mut self);
    /// Set the value for field `unit`.
    fn add_unit(&mut self, unit: i16);
    /// Set the value for field `bitWidth`.
    fn add_bitWidth(&mut self, bitWidth: i32);
}

impl TimeBuilder for flatbuffers::Builder {
    fn start_time(&mut self) {
        self.start_object(2);
    }

    fn add_unit(&mut self, unit: i16) {
        self.add_slot_i16(0, unit, 1)
    }

    fn add_bitWidth(&mut self, bitWidth: i32) {
        self.add_slot_i32(1, bitWidth, 32)
    }

}

