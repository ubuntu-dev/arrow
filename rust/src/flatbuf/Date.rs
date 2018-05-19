//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

/// Date is either a 32-bit or 64-bit type representing elapsed time since UNIX
/// epoch (1970-01-01), stored in either of two units:
///
/// * Milliseconds (64 bits) indicating UNIX time elapsed since the epoch (no
///   leap seconds), where the values are evenly divisible by 86400000
/// * Days (32 bits) since the UNIX epoch
flatbuffers_object!{Table => Date [
 field => { name = unit,
            typeOf = enum DateUnit i16,
            slot = 4,
            default = 1 }]}

/// Builder Trait for `Date` tables.
pub trait DateBuilder {
    fn start_date(&mut self);
    /// Set the value for field `unit`.
    fn add_unit(&mut self, unit: i16);
}

impl DateBuilder for flatbuffers::Builder {
    fn start_date(&mut self) {
        self.start_object(1);
    }

    fn add_unit(&mut self, unit: i16) {
        self.add_slot_i16(0, unit, 1)
    }

}

