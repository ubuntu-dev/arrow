//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

flatbuffers_object!{Table => Bool []}

/// Builder Trait for `Bool` tables.
pub trait BoolBuilder {
    fn start_bool(&mut self);
}

impl BoolBuilder for flatbuffers::Builder {
    fn start_bool(&mut self) {
        self.start_object(0);
    }

}

