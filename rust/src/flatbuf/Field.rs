//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

/// ----------------------------------------------------------------------
/// A field represents a named column in a record / row batch or child of a
/// nested type.
///
/// - children is only for nested Arrow arrays
/// - For primitive types, children will have length 0
/// - nullable should default to true in general
flatbuffers_object!{Table => Field [
 field => { name = name,
            typeOf = string,
            slot = 4,
            default = 0 }, 
 field => { name = nullable,
            typeOf = bool,
            slot = 6,
            default = false }, 
 field => { name = type_type,
            typeOf = enum TypeType u8,
            slot = 8,
            default = 0 }, 
 field => { name = type,
            typeOf = union Type,
            slot = 10,
            default = 0 }, 
 field => { name = dictionary,
            typeOf = DictionaryEncoding,
            slot = 12 }, 
 field => { name = children,
            typeOf = [Field],
            slot = 14 }, 
 field => { name = custom_metadata,
            typeOf = [KeyValue],
            slot = 16 }]}

/// Builder Trait for `Field` tables.
pub trait FieldBuilder {
    fn start_field(&mut self);
    /// Set the value for field `name`.
    fn add_name(&mut self, name: flatbuffers::UOffsetT);
    /// Set the value for field `nullable`.
    fn add_nullable(&mut self, nullable: bool);
    /// Set the value for field `type_type`.
    fn add_type_type(&mut self, type_type: u8);
    /// Set the value for field `type`.
    fn add_dtype(&mut self, dtype: flatbuffers::UOffsetT);
    /// Set the value for field `dictionary`.
    fn add_dictionary(&mut self, dictionary: flatbuffers::UOffsetT);
    /// Set the value for field `children`.
    fn add_children(&mut self, children: flatbuffers::UOffsetT);
    /// Initializes bookkeeping for writing a new `children` vector.
    fn start_children_vector(&mut self, numElems: usize);
    /// Set the value for field `custom_metadata`.
    fn add_custom_metadata(&mut self, custom_metadata: flatbuffers::UOffsetT);
    /// Initializes bookkeeping for writing a new `custom_metadata` vector.
    fn start_custom_metadata_vector(&mut self, numElems: usize);
}

impl FieldBuilder for flatbuffers::Builder {
    fn start_field(&mut self) {
        self.start_object(7);
    }

    fn add_name(&mut self, name: flatbuffers::UOffsetT) {
        self.add_slot_uoffset(0, name, 0)
    }

    fn add_nullable(&mut self, nullable: bool) {
        self.add_slot_bool(1, nullable, false)
    }

    fn add_type_type(&mut self, type_type: u8) {
        self.add_slot_u8(2, type_type, 0)
    }

    fn add_type(&mut self, dtype: flatbuffers::UOffsetT) {
        self.add_slot_uoffset(3, dtype, 0)
    }

    fn add_dictionary(&mut self, dictionary: flatbuffers::UOffsetT) {
        self.add_slot_uoffset(4, dictionary, 0)
    }

    fn add_children(&mut self, children: flatbuffers::UOffsetT) {
        self.add_slot_uoffset(5, children, 0)
    }

    /// Initializes bookkeeping for writing a new `children` vector.
    fn start_children_vector(&mut self, numElems: usize) {
        self.start_vector(4, numElems, 4)
    }

    fn add_custom_metadata(&mut self, custom_metadata: flatbuffers::UOffsetT) {
        self.add_slot_uoffset(6, custom_metadata, 0)
    }

    /// Initializes bookkeeping for writing a new `custom_metadata` vector.
    fn start_custom_metadata_vector(&mut self, numElems: usize) {
        self.start_vector(4, numElems, 4)
    }

}

