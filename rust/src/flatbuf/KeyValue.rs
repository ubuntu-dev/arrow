//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

/// ----------------------------------------------------------------------
/// user defined key value pairs to add custom metadata to arrow
/// key namespacing is the responsibility of the user
flatbuffers_object!{Table => KeyValue [
 field => { name = key,
            typeOf = string,
            slot = 4,
            default = 0 }, 
 field => { name = value,
            typeOf = string,
            slot = 6,
            default = 0 }]}

/// Builder Trait for `KeyValue` tables.
pub trait KeyValueBuilder {
    fn start_keyvalue(&mut self);
    /// Set the value for field `key`.
    fn add_key(&mut self, key: flatbuffers::UOffsetT);
    /// Set the value for field `value`.
    fn add_value(&mut self, value: flatbuffers::UOffsetT);
}

impl KeyValueBuilder for flatbuffers::Builder {
    fn start_keyvalue(&mut self) {
        self.start_object(2);
    }

    fn add_key(&mut self, key: flatbuffers::UOffsetT) {
        self.add_slot_uoffset(0, key, 0)
    }

    fn add_value(&mut self, value: flatbuffers::UOffsetT) {
        self.add_slot_uoffset(1, value, 0)
    }

}

