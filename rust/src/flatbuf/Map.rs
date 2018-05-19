//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

/// A Map is a logical nested type that is represented as
///
/// List<entry: Struct<key: K, value: V>>
///
/// In this layout, the keys and values are each respectively contiguous. We do
/// not constrain the key and value types, so the application is responsible
/// for ensuring that the keys are hashable and unique. Whether the keys are sorted
/// may be set in the metadata for this field
///
/// In a Field with Map type, the Field has a child Struct field, which then
/// has two children: key type and the second the value type. The names of the
/// child fields may be respectively "entry", "key", and "value", but this is
/// not enforced
///
/// Map
///   - child[0] entry: Struct
///     - child[0] key: K
///     - child[1] value: V
///
/// Neither the "entry" field nor the "key" field may be nullable.
///
/// The metadata is structured so that Arrow systems without special handling
/// for Map can make Map an alias for List. The "layout" attribute for the Map
/// field must have the same contents as a List.
flatbuffers_object!{Table => Map [
 field => { name = keysSorted,
            typeOf = bool,
            slot = 4,
            default = false }]}

/// Builder Trait for `Map` tables.
pub trait MapBuilder {
    fn start_map(&mut self);
    /// Set the value for field `keysSorted`.
    fn add_keysSorted(&mut self, keysSorted: bool);
}

impl MapBuilder for flatbuffers::Builder {
    fn start_map(&mut self) {
        self.start_object(1);
    }

    fn add_keysSorted(&mut self, keysSorted: bool) {
        self.add_slot_bool(0, keysSorted, false)
    }

}

