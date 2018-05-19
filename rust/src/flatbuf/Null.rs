//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

/// These are stored in the flatbuffer in the Type union below
flatbuffers_object!{Table => Null []}

/// Builder Trait for `Null` tables.
pub trait NullBuilder {
    fn start_null(&mut self);
}

impl NullBuilder for flatbuffers::Builder {
    fn start_null(&mut self) {
        self.start_object(0);
    }

}

