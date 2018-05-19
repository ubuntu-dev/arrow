//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

flatbuffers_object!{Table => FixedSizeList [
 field => { name = listSize,
            typeOf = i32,
            slot = 4,
            default = 0 }]}

/// Builder Trait for `FixedSizeList` tables.
pub trait FixedSizeListBuilder {
    fn start_fixedsizelist(&mut self);
    /// Set the value for field `listSize`.
    fn add_listSize(&mut self, listSize: i32);
}

impl FixedSizeListBuilder for flatbuffers::Builder {
    fn start_fixedsizelist(&mut self) {
        self.start_object(1);
    }

    fn add_listSize(&mut self, listSize: i32) {
        self.add_slot_i32(0, listSize, 0)
    }

}

