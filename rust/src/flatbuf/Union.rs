//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

/// A union is a complex type with children in Field
/// By default ids in the type vector refer to the offsets in the children
/// optionally typeIds provides an indirection between the child offset and the type id
/// for each child typeIds[offset] is the id used in the type vector
flatbuffers_object!{Table => Union [
 field => { name = mode,
            typeOf = enum UnionMode i16,
            slot = 4,
            default = 0 }, 
 field => { name = typeIds,
            typeOf = [i32],
            slot = 6 }]}

/// Builder Trait for `Union` tables.
pub trait UnionBuilder {
    fn start_union(&mut self);
    /// Set the value for field `mode`.
    fn add_mode(&mut self, mode: i16);
    /// Set the value for field `typeIds`.
    fn add_typeIds(&mut self, typeIds: flatbuffers::UOffsetT);
    /// Initializes bookkeeping for writing a new `typeIds` vector.
    fn start_typeIds_vector(&mut self, numElems: usize);
}

impl UnionBuilder for flatbuffers::Builder {
    fn start_union(&mut self) {
        self.start_object(2);
    }

    fn add_mode(&mut self, mode: i16) {
        self.add_slot_i16(0, mode, 0)
    }

    fn add_typeIds(&mut self, typeIds: flatbuffers::UOffsetT) {
        self.add_slot_uoffset(1, typeIds, 0)
    }

    /// Initializes bookkeeping for writing a new `typeIds` vector.
    fn start_typeIds_vector(&mut self, numElems: usize) {
        self.start_vector(4, numElems, 4)
    }

}

