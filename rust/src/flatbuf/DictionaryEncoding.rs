//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

/// ----------------------------------------------------------------------
/// Dictionary encoding metadata
flatbuffers_object!{Table => DictionaryEncoding [
 field => { name = id,
            typeOf = i64,
            slot = 4,
            default = 0 }, 
 field => { name = indexType,
            typeOf = Int,
            slot = 6 }, 
 field => { name = isOrdered,
            typeOf = bool,
            slot = 8,
            default = false }]}

/// Builder Trait for `DictionaryEncoding` tables.
pub trait DictionaryEncodingBuilder {
    fn start_dictionaryencoding(&mut self);
    /// Set the value for field `id`.
    fn add_id(&mut self, id: i64);
    /// Set the value for field `indexType`.
    fn add_indexType(&mut self, indexType: flatbuffers::UOffsetT);
    /// Set the value for field `isOrdered`.
    fn add_isOrdered(&mut self, isOrdered: bool);
}

impl DictionaryEncodingBuilder for flatbuffers::Builder {
    fn start_dictionaryencoding(&mut self) {
        self.start_object(3);
    }

    fn add_id(&mut self, id: i64) {
        self.add_slot_i64(0, id, 0)
    }

    fn add_indexType(&mut self, indexType: flatbuffers::UOffsetT) {
        self.add_slot_uoffset(1, indexType, 0)
    }

    fn add_isOrdered(&mut self, isOrdered: bool) {
        self.add_slot_bool(2, isOrdered, false)
    }

}

