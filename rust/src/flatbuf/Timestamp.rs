//! Automatically generated, do not modify.

use flatbuffers;
use super::*;

/// Time elapsed from the Unix epoch, 00:00:00.000 on 1 January 1970, excluding
/// leap seconds, as a 64-bit integer. Note that UNIX time does not include
/// leap seconds.
///
/// The Timestamp metadata supports both "time zone naive" and "time zone
/// aware" timestamps. Read about the timezone attribute for more detail
flatbuffers_object!{Table => Timestamp [
 field => { name = unit,
            typeOf = enum TimeUnit i16,
            slot = 4,
            default = 0 }, 
 field => { name = timezone,
            typeOf = string,
            slot = 6,
            default = 0 }]}

/// Builder Trait for `Timestamp` tables.
pub trait TimestampBuilder {
    fn start_timestamp(&mut self);
    /// Set the value for field `unit`.
    fn add_unit(&mut self, unit: i16);
    /// Set the value for field `timezone`.
    fn add_timezone(&mut self, timezone: flatbuffers::UOffsetT);
}

impl TimestampBuilder for flatbuffers::Builder {
    fn start_timestamp(&mut self) {
        self.start_object(2);
    }

    fn add_unit(&mut self, unit: i16) {
        self.add_slot_i16(0, unit, 0)
    }

    fn add_timezone(&mut self, timezone: flatbuffers::UOffsetT) {
        self.add_slot_uoffset(1, timezone, 0)
    }

}

