//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

flatbuffers_object!{Table => Decimal [
 field => { name = precision,
            typeOf = i32,
            slot = 4,
            default = 0 }, 
 field => { name = scale,
            typeOf = i32,
            slot = 6,
            default = 0 }]}

/// Builder Trait for `Decimal` tables.
pub trait DecimalBuilder {
    fn start_decimal(&mut self);
    /// Set the value for field `precision`.
    fn add_precision(&mut self, precision: i32);
    /// Set the value for field `scale`.
    fn add_scale(&mut self, scale: i32);
}

impl DecimalBuilder for flatbuffers::Builder {
    fn start_decimal(&mut self) {
        self.start_object(2);
    }

    fn add_precision(&mut self, precision: i32) {
        self.add_slot_i32(0, precision, 0)
    }

    fn add_scale(&mut self, scale: i32) {
        self.add_slot_i32(1, scale, 0)
    }

}

