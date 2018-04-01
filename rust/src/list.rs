// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

use std::rc::Rc;
use std::str;

use bytes::{BytesMut, BufMut};

use super::buffer::Buffer;

pub struct List<T> {
    pub data: Rc<Buffer<T>>,
    pub offsets: Rc<Buffer<i32>>
}

impl<T> List<T> {

    pub fn len(&self) -> i32 {
        self.offsets.len()-1
    }

    pub fn slice(&self, index: usize) -> &[T] {
        let start = *self.offsets.get(index) as usize;
        let end = *self.offsets.get(index+1) as usize;
        &self.data.slice(start, end)
    }
}

impl From<Vec<String>> for List<u8> {
    fn from(v: Vec<String>) -> Self {
        let mut offsets : Vec<i32> = Vec::with_capacity(v.len() + 1);
        let mut buf = BytesMut::with_capacity(v.len() * 32);
        offsets.push(0_i32);
        v.iter().for_each(|s| {
            buf.put(s.as_bytes());
            offsets.push(buf.len() as i32);
        });
        let bytes = buf.freeze();
        let buffer = Buffer::new(bytes.as_ptr(), bytes.len() as i32);

        List { data: buffer, offsets: Buffer::from(offsets) }
    }
}

/// This method mostly just used for unit tests
impl From<Vec<&'static str>> for List<u8> {
    fn from(v: Vec<&'static str>) -> Self {
        List::from(v.iter().map(|s| s.to_string()).collect::<Vec<String>>())
    }
}



#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_utf8_slices() {
        let list = List::from(vec!["this", "is", "a", "test"]);
        assert_eq!(4, list.len());
        assert_eq!("this", str::from_utf8(list.slice(0)).unwrap());
        assert_eq!("is", str::from_utf8(list.slice(1)).unwrap());
        assert_eq!("a", str::from_utf8(list.slice(2)).unwrap());
        assert_eq!("test", str::from_utf8(list.slice(3)).unwrap());
    }

}