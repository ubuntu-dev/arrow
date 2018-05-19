//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

/// ----------------------------------------------------------------------
/// A Schema describes the columns in a row batch
flatbuffers_object!{Table => Schema [
 field => { name = endianness,
            typeOf = enum Endianness i16,
            slot = 4,
            default = 0 }, 
 field => { name = fields,
            typeOf = [Field],
            slot = 6 }, 
 field => { name = custom_metadata,
            typeOf = [KeyValue],
            slot = 8 }]}

/// Builder Trait for `Schema` tables.
pub trait SchemaBuilder {
    fn start_schema(&mut self);
    /// Set the value for field `endianness`.
    fn add_endianness(&mut self, endianness: i16);
    /// Set the value for field `fields`.
    fn add_fields(&mut self, fields: flatbuffers::UOffsetT);
    /// Initializes bookkeeping for writing a new `fields` vector.
    fn start_fields_vector(&mut self, numElems: usize);
    /// Set the value for field `custom_metadata`.
    fn add_custom_metadata(&mut self, custom_metadata: flatbuffers::UOffsetT);
    /// Initializes bookkeeping for writing a new `custom_metadata` vector.
    fn start_custom_metadata_vector(&mut self, numElems: usize);
}

impl SchemaBuilder for flatbuffers::Builder {
    fn start_schema(&mut self) {
        self.start_object(3);
    }

    fn add_endianness(&mut self, endianness: i16) {
        self.add_slot_i16(0, endianness, 0)
    }

    fn add_fields(&mut self, fields: flatbuffers::UOffsetT) {
        self.add_slot_uoffset(1, fields, 0)
    }

    /// Initializes bookkeeping for writing a new `fields` vector.
    fn start_fields_vector(&mut self, numElems: usize) {
        self.start_vector(4, numElems, 4)
    }

    fn add_custom_metadata(&mut self, custom_metadata: flatbuffers::UOffsetT) {
        self.add_slot_uoffset(2, custom_metadata, 0)
    }

    /// Initializes bookkeeping for writing a new `custom_metadata` vector.
    fn start_custom_metadata_vector(&mut self, numElems: usize) {
        self.start_vector(4, numElems, 4)
    }

}

