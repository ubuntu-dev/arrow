//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

/// Unicode with UTF-8 encoding
flatbuffers_object!{Table => Utf8 []}

/// Builder Trait for `Utf8` tables.
pub trait Utf8Builder {
    fn start_utf8(&mut self);
}

impl Utf8Builder for flatbuffers::Builder {
    fn start_utf8(&mut self) {
        self.start_object(0);
    }

}

