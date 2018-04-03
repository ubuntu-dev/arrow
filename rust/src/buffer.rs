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

use bytes::Bytes;
use std::mem;
use std::slice;
use libc;

use super::memory::*;

pub struct Buffer<T> {
    data: *const T,
    len: i32
}

impl<T> Buffer<T> {

    pub fn len(&self) -> i32 {
        self.len
    }

    pub fn data(&self) -> *const T {
        self.data
    }

    pub fn slice(&self, start: usize, end: usize) -> &[T] {
        assert!(start<=end);
        assert!(start<self.len as usize);
        assert!(end<=self.len as usize);
        unsafe { slice::from_raw_parts(self.data.offset(start as isize), (end-start) as usize) }
    }

    pub fn get(&self, i: usize) -> &T {
        unsafe { &(*self.data.offset(i as isize)) }
    }

    pub fn set(&mut self, i: usize, v: T) {
        unsafe {
            let p = mem::transmute::<*const T, *mut T>(self.data);
            *p.offset(i as isize) = v;
        }
    }

    pub fn iter(&self) -> BufferIterator<T> {
        BufferIterator {
            data: self.data,
            len: self.len,
            index: 0
        }
    }

}

impl<T> Drop for Buffer<T> {
    fn drop(&mut self) {
        mem::drop(self.data)
    }
}

pub struct BufferIterator<T> {
    data: *const T,
    len: i32,
    index: isize
}

impl<T> Iterator for BufferIterator<T> where T: Copy {
    type Item = T;

    fn next(&mut self) -> Option<Self::Item> {
        if self.index < self.len as isize {
            self.index += 1;
            Some(unsafe { *self.data.offset(self.index-1) })
        } else {
            None
        }
    }
}

macro_rules! array_from_primitive {
    ($DT:ty) => {
        impl From<Vec<$DT>> for Buffer<$DT> {
            fn from(v: Vec<$DT>) -> Self {
                // allocate aligned memory buffer
                let len = v.len();
                let sz = mem::size_of::<$DT>();
                let buffer = allocate_aligned((len * sz) as i64).unwrap();
                Buffer {
                    len: len as i32,
                    data: unsafe {
                        let dst = mem::transmute::<*const u8, *mut libc::c_void>(buffer);
                        libc::memcpy(dst, mem::transmute::<*const $DT, *const libc::c_void>(v.as_ptr()), len * sz);
                        mem::transmute::<*mut libc::c_void, *const $DT>(dst)
                    }
                }
            }
        }
    }
}

array_from_primitive!(bool);
array_from_primitive!(f32);
array_from_primitive!(f64);
array_from_primitive!(u8);
array_from_primitive!(u16);
array_from_primitive!(u32);
array_from_primitive!(u64);
array_from_primitive!(i8);
array_from_primitive!(i16);
array_from_primitive!(i32);
array_from_primitive!(i64);

impl From<Bytes> for Buffer<u8> {
    fn from(bytes: Bytes) -> Self {
        // allocate aligned
        let len = bytes.len();
        let sz = mem::size_of::<u8>();
        let buf_mem = allocate_aligned((len * sz) as i64).unwrap();
        Buffer {
            len: len as i32,
            data: unsafe {
                let dst = mem::transmute::<*const u8, *mut libc::c_void>(buf_mem);
                libc::memcpy(dst, mem::transmute::<*const u8, *const libc::c_void>(bytes.as_ptr()), len * sz);
                mem::transmute::<*mut libc::c_void, *const u8>(dst)
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_buffer_i32() {
        let b: Buffer<i32> = Buffer::from(vec![1, 2, 3, 4, 5]);
        assert_eq!(5, b.len);
    }

    #[test]
    fn test_iterator_i32() {
        let b: Buffer<i32> = Buffer::from(vec![1, 2, 3, 4, 5]);
        let it = b.iter();
        let v : Vec<i32> = it.map(|n| n+1).collect();
        assert_eq!(vec![2,3,4,5,6], v);
    }
}
