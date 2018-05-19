//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

/// A Struct_ in the flatbuffer metadata is the same as an Arrow Struct
/// (according to the physical memory layout). We used Struct_ here as
/// Struct is a reserved word in Flatbuffers
flatbuffers_object!{Table => Struct_ []}

/// Builder Trait for `Struct_` tables.
pub trait Struct_Builder {
    fn start_struct_(&mut self);
}

impl Struct_Builder for flatbuffers::Builder {
    fn start_struct_(&mut self) {
        self.start_object(0);
    }

}

