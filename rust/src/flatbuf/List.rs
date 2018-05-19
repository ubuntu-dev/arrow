//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

flatbuffers_object!{Table => List []}

/// Builder Trait for `List` tables.
pub trait ListBuilder {
    fn start_list(&mut self);
}

impl ListBuilder for flatbuffers::Builder {
    fn start_list(&mut self) {
        self.start_object(0);
    }

}

