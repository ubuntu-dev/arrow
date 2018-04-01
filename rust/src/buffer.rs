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

use std::mem;

#[repr(align(64))]
struct AlignedUInt8([u8; 64]);

#[repr(align(64))]
struct AlignedUInt16([u16; 32]);

#[repr(align(64))]
struct AlignedUInt32([u32; 16]);

#[repr(align(64))]
struct AlignedUInt64([u64; 8]);

#[repr(align(64))]
struct AlignedInt8([i8; 64]);

#[repr(align(64))]
struct AlignedInt16([i16; 32]);

#[repr(align(64))]
struct AlignedInt32([i32; 16]);

#[repr(align(64))]
struct AlignedInt64([i64; 8]);

pub trait Buffer<T> {
    fn len(&self) -> i32;
    fn data(&self) -> *const T;
    fn slice(&self, start: usize, end: usize) -> &[T];
    fn get(&self, i: usize) -> &T;
    fn set(&mut self, i: usize, v: T);
}


/*
    fn new(data: *const T, len: i32) -> Self {
        Buffer { data, len }
    }

    fn len(&self) -> i32 {
        self.len
    }

    fn data(&self) -> *const T {
        self.data
    }

    fn slice(&self, start: usize, end: usize) -> &[T] {
        unsafe { slice::from_raw_parts(self.data.offset(start as isize), (end-start) as usize) }
    }

    fn get(&self, i: usize) -> &T {
        unsafe { &(*self.data.offset(i as isize)) }
    }

    fn set(&mut self, i: usize, v: T) {
        unsafe {
            let p = mem::transmute::<*const T, *mut T>(self.data);
            *p.offset(i as isize) = v;
        }
    }
*/
macro_rules! array_from_primitive {
    ($DT:ty) => {
        impl From<Vec<$DT>> for Buffer<$DT> {
            fn from(v: Vec<$DT>) -> Self {
                // allocate aligned memory buffer
//                let len = v.len();
//                let sz = mem::size_of::<$DT>();
//                let buffer = allocate_aligned((len * sz) as i64).unwrap();
//                Buffer {
//                    len: len as i32,
//                    data: unsafe {
//                        let dst = mem::transmute::<*const u8, *mut libc::c_void>(buffer);
//                        libc::memcpy(dst, mem::transmute::<*const $DT, *const libc::c_void>(v.as_ptr()), len * sz);
//                        mem::transmute::<*mut libc::c_void, *const $DT>(dst)
//                    }
//                }
                unimplemented!()
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
//
//
//#[cfg(test)]
//mod tests {
//    use super::*;
//
//    #[test]
//    fn test_buffer_i32() {
//        let b: Buffer<i32> = Buffer::from(vec![1, 2, 3, 4, 5]);
//        assert_eq!(5, b.len);
//    }
//
//    #[test]
//    fn test_aligned_vec() {
//        let a = AlignedInt32([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
//        let b = AlignedInt32([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
//        let v = vec![a, b];
//        assert_eq!(0, ((&v[0] as *const AlignedInt32) as usize) % 64);
//        assert_eq!(0, ((&v[1] as *const AlignedInt32) as usize) % 64);
//    }
//
//}
