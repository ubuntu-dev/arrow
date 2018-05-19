//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

flatbuffers_object!{Table => Binary []}

/// Builder Trait for `Binary` tables.
pub trait BinaryBuilder {
    fn start_binary(&mut self);
}

impl BinaryBuilder for flatbuffers::Builder {
    fn start_binary(&mut self) {
        self.start_object(0);
    }

}

