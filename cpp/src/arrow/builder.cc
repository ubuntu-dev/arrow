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

#include "arrow/builder.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <sstream>
#include <utility>
#include <vector>

#include "arrow/array.h"
#include "arrow/buffer.h"
#include "arrow/compare.h"
#include "arrow/status.h"
#include "arrow/type.h"
#include "arrow/type_traits.h"
#include "arrow/util/bit-util.h"
#include "arrow/util/cpu-info.h"
#include "arrow/util/decimal.h"
#include "arrow/util/hash-util.h"
#include "arrow/util/hash.h"
#include "arrow/util/logging.h"

namespace arrow {

using internal::AdaptiveIntBuilderBase;

Status ArrayBuilder::AppendToBitmap(bool is_valid) {
  if (length_ == capacity_) {
    // If the capacity was not already a multiple of 2, do so here
    // TODO(emkornfield) doubling isn't great default allocation practice
    // see https://github.com/facebook/folly/blob/master/folly/docs/FBVector.md
    // fo discussion
    RETURN_NOT_OK(Resize(BitUtil::NextPower2(capacity_ + 1)));
  }
  UnsafeAppendToBitmap(is_valid);
  return Status::OK();
}

Status ArrayBuilder::AppendToBitmap(const uint8_t* valid_bytes, int64_t length) {
  RETURN_NOT_OK(Reserve(length));

  UnsafeAppendToBitmap(valid_bytes, length);
  return Status::OK();
}

Status ArrayBuilder::Init(int64_t capacity) {
  int64_t to_alloc = BitUtil::CeilByte(capacity) / 8;
  null_bitmap_ = std::make_shared<PoolBuffer>(pool_);
  RETURN_NOT_OK(null_bitmap_->Resize(to_alloc));
  // Buffers might allocate more then necessary to satisfy padding requirements
  const int64_t byte_capacity = null_bitmap_->capacity();
  capacity_ = capacity;
  null_bitmap_data_ = null_bitmap_->mutable_data();
  memset(null_bitmap_data_, 0, static_cast<size_t>(byte_capacity));
  return Status::OK();
}

Status ArrayBuilder::Resize(int64_t new_bits) {
  if (!null_bitmap_) {
    return Init(new_bits);
  }
  int64_t new_bytes = BitUtil::CeilByte(new_bits) / 8;
  int64_t old_bytes = null_bitmap_->size();
  RETURN_NOT_OK(null_bitmap_->Resize(new_bytes));
  null_bitmap_data_ = null_bitmap_->mutable_data();
  // The buffer might be overpadded to deal with padding according to the spec
  const int64_t byte_capacity = null_bitmap_->capacity();
  capacity_ = new_bits;
  if (old_bytes < new_bytes) {
    memset(null_bitmap_data_ + old_bytes, 0,
           static_cast<size_t>(byte_capacity - old_bytes));
  }
  return Status::OK();
}

Status ArrayBuilder::Advance(int64_t elements) {
  if (length_ + elements > capacity_) {
    return Status::Invalid("Builder must be expanded");
  }
  length_ += elements;
  return Status::OK();
}

Status ArrayBuilder::Finish(std::shared_ptr<Array>* out) {
  std::shared_ptr<ArrayData> internal_data;
  RETURN_NOT_OK(FinishInternal(&internal_data));
  *out = MakeArray(internal_data);
  return Status::OK();
}

Status ArrayBuilder::Reserve(int64_t elements) {
  if (length_ + elements > capacity_) {
    // TODO(emkornfield) power of 2 growth is potentially suboptimal
    int64_t new_capacity = BitUtil::NextPower2(length_ + elements);
    return Resize(new_capacity);
  }
  return Status::OK();
}

void ArrayBuilder::Reset() {
  capacity_ = length_ = null_count_ = 0;
  null_bitmap_ = nullptr;
}

Status ArrayBuilder::SetNotNull(int64_t length) {
  RETURN_NOT_OK(Reserve(length));
  UnsafeSetNotNull(length);
  return Status::OK();
}

void ArrayBuilder::UnsafeAppendToBitmap(const uint8_t* valid_bytes, int64_t length) {
  if (valid_bytes == nullptr) {
    UnsafeSetNotNull(length);
    return;
  }

  int64_t byte_offset = length_ / 8;
  int64_t bit_offset = length_ % 8;
  uint8_t bitset = null_bitmap_data_[byte_offset];

  for (int64_t i = 0; i < length; ++i) {
    if (bit_offset == 8) {
      bit_offset = 0;
      null_bitmap_data_[byte_offset] = bitset;
      byte_offset++;
      // TODO: Except for the last byte, this shouldn't be needed
      bitset = null_bitmap_data_[byte_offset];
    }

    if (valid_bytes[i]) {
      bitset |= BitUtil::kBitmask[bit_offset];
    } else {
      bitset &= BitUtil::kFlippedBitmask[bit_offset];
      ++null_count_;
    }

    bit_offset++;
  }
  if (bit_offset != 0) {
    null_bitmap_data_[byte_offset] = bitset;
  }
  length_ += length;
}

void ArrayBuilder::UnsafeAppendToBitmap(const std::vector<bool>& is_valid) {
  int64_t byte_offset = length_ / 8;
  int64_t bit_offset = length_ % 8;
  uint8_t bitset = null_bitmap_data_[byte_offset];

  const int64_t length = static_cast<int64_t>(is_valid.size());

  for (int64_t i = 0; i < length; ++i) {
    if (bit_offset == 8) {
      bit_offset = 0;
      null_bitmap_data_[byte_offset] = bitset;
      byte_offset++;
      // TODO: Except for the last byte, this shouldn't be needed
      bitset = null_bitmap_data_[byte_offset];
    }

    if (is_valid[i]) {
      bitset |= BitUtil::kBitmask[bit_offset];
    } else {
      bitset &= BitUtil::kFlippedBitmask[bit_offset];
      ++null_count_;
    }

    bit_offset++;
  }
  if (bit_offset != 0) {
    null_bitmap_data_[byte_offset] = bitset;
  }
  length_ += length;
}

void ArrayBuilder::UnsafeSetNotNull(int64_t length) {
  const int64_t new_length = length + length_;

  // Fill up the bytes until we have a byte alignment
  int64_t pad_to_byte = std::min<int64_t>(8 - (length_ % 8), length);

  if (pad_to_byte == 8) {
    pad_to_byte = 0;
  }
  for (int64_t i = length_; i < length_ + pad_to_byte; ++i) {
    BitUtil::SetBit(null_bitmap_data_, i);
  }

  // Fast bitsetting
  int64_t fast_length = (length - pad_to_byte) / 8;
  memset(null_bitmap_data_ + ((length_ + pad_to_byte) / 8), 0xFF,
         static_cast<size_t>(fast_length));

  // Trailing bytes
  for (int64_t i = length_ + pad_to_byte + (fast_length * 8); i < new_length; ++i) {
    BitUtil::SetBit(null_bitmap_data_, i);
  }

  length_ = new_length;
}

// ----------------------------------------------------------------------
// Null builder

Status NullBuilder::FinishInternal(std::shared_ptr<ArrayData>* out) {
  *out = ArrayData::Make(null(), length_, {nullptr}, length_);
  length_ = null_count_ = 0;
  return Status::OK();
}

// ----------------------------------------------------------------------

template <typename T>
Status PrimitiveBuilder<T>::Init(int64_t capacity) {
  RETURN_NOT_OK(ArrayBuilder::Init(capacity));
  data_ = std::make_shared<PoolBuffer>(pool_);

  int64_t nbytes = TypeTraits<T>::bytes_required(capacity);
  RETURN_NOT_OK(data_->Resize(nbytes));
  // TODO(emkornfield) valgrind complains without this
  memset(data_->mutable_data(), 0, static_cast<size_t>(nbytes));

  raw_data_ = reinterpret_cast<value_type*>(data_->mutable_data());
  return Status::OK();
}

template <typename T>
Status PrimitiveBuilder<T>::Resize(int64_t capacity) {
  // XXX: Set floor size for now
  if (capacity < kMinBuilderCapacity) {
    capacity = kMinBuilderCapacity;
  }

  if (capacity_ == 0) {
    RETURN_NOT_OK(Init(capacity));
  } else {
    RETURN_NOT_OK(ArrayBuilder::Resize(capacity));
    const int64_t old_bytes = data_->size();
    const int64_t new_bytes = TypeTraits<T>::bytes_required(capacity);
    RETURN_NOT_OK(data_->Resize(new_bytes));
    raw_data_ = reinterpret_cast<value_type*>(data_->mutable_data());
    // TODO(emkornfield) valgrind complains without this
    memset(data_->mutable_data() + old_bytes, 0,
           static_cast<size_t>(new_bytes - old_bytes));
  }
  return Status::OK();
}

template <typename T>
Status PrimitiveBuilder<T>::Append(const value_type* values, int64_t length,
                                   const uint8_t* valid_bytes) {
  RETURN_NOT_OK(Reserve(length));

  if (length > 0) {
    std::memcpy(raw_data_ + length_, values,
                static_cast<std::size_t>(TypeTraits<T>::bytes_required(length)));
  }

  // length_ is update by these
  ArrayBuilder::UnsafeAppendToBitmap(valid_bytes, length);

  return Status::OK();
}

template <typename T>
Status PrimitiveBuilder<T>::Append(const value_type* values, int64_t length,
                                   const std::vector<bool>& is_valid) {
  RETURN_NOT_OK(Reserve(length));
  DCHECK_EQ(length, static_cast<int64_t>(is_valid.size()));

  if (length > 0) {
    std::memcpy(raw_data_ + length_, values,
                static_cast<std::size_t>(TypeTraits<T>::bytes_required(length)));
  }

  // length_ is update by these
  ArrayBuilder::UnsafeAppendToBitmap(is_valid);

  return Status::OK();
}

template <typename T>
Status PrimitiveBuilder<T>::Append(const std::vector<value_type>& values,
                                   const std::vector<bool>& is_valid) {
  return Append(values.data(), static_cast<int64_t>(values.size()), is_valid);
}

template <typename T>
Status PrimitiveBuilder<T>::Append(const std::vector<value_type>& values) {
  return Append(values.data(), static_cast<int64_t>(values.size()));
}

template <typename T>
Status PrimitiveBuilder<T>::FinishInternal(std::shared_ptr<ArrayData>* out) {
  const int64_t bytes_required = TypeTraits<T>::bytes_required(length_);
  if (bytes_required > 0 && bytes_required < data_->size()) {
    // Trim buffers
    RETURN_NOT_OK(data_->Resize(bytes_required));
  }
  *out = ArrayData::Make(type_, length_, {null_bitmap_, data_}, null_count_);

  data_ = null_bitmap_ = nullptr;
  capacity_ = length_ = null_count_ = 0;
  return Status::OK();
}

template class PrimitiveBuilder<UInt8Type>;
template class PrimitiveBuilder<UInt16Type>;
template class PrimitiveBuilder<UInt32Type>;
template class PrimitiveBuilder<UInt64Type>;
template class PrimitiveBuilder<Int8Type>;
template class PrimitiveBuilder<Int16Type>;
template class PrimitiveBuilder<Int32Type>;
template class PrimitiveBuilder<Int64Type>;
template class PrimitiveBuilder<Date32Type>;
template class PrimitiveBuilder<Date64Type>;
template class PrimitiveBuilder<Time32Type>;
template class PrimitiveBuilder<Time64Type>;
template class PrimitiveBuilder<TimestampType>;
template class PrimitiveBuilder<HalfFloatType>;
template class PrimitiveBuilder<FloatType>;
template class PrimitiveBuilder<DoubleType>;

AdaptiveIntBuilderBase::AdaptiveIntBuilderBase(MemoryPool* pool)
    : ArrayBuilder(int64(), pool), data_(nullptr), raw_data_(nullptr), int_size_(1) {}

Status AdaptiveIntBuilderBase::Init(int64_t capacity) {
  RETURN_NOT_OK(ArrayBuilder::Init(capacity));
  data_ = std::make_shared<PoolBuffer>(pool_);

  int64_t nbytes = capacity * int_size_;
  RETURN_NOT_OK(data_->Resize(nbytes));
  // TODO(emkornfield) valgrind complains without this
  memset(data_->mutable_data(), 0, static_cast<size_t>(nbytes));

  raw_data_ = reinterpret_cast<uint8_t*>(data_->mutable_data());
  return Status::OK();
}

Status AdaptiveIntBuilderBase::Resize(int64_t capacity) {
  // XXX: Set floor size for now
  if (capacity < kMinBuilderCapacity) {
    capacity = kMinBuilderCapacity;
  }

  if (capacity_ == 0) {
    RETURN_NOT_OK(Init(capacity));
  } else {
    RETURN_NOT_OK(ArrayBuilder::Resize(capacity));
    const int64_t old_bytes = data_->size();
    const int64_t new_bytes = capacity * int_size_;
    RETURN_NOT_OK(data_->Resize(new_bytes));
    raw_data_ = data_->mutable_data();
    // TODO(emkornfield) valgrind complains without this
    memset(data_->mutable_data() + old_bytes, 0,
           static_cast<size_t>(new_bytes - old_bytes));
  }
  return Status::OK();
}

AdaptiveIntBuilder::AdaptiveIntBuilder(MemoryPool* pool) : AdaptiveIntBuilderBase(pool) {}

Status AdaptiveIntBuilder::FinishInternal(std::shared_ptr<ArrayData>* out) {
  const int64_t bytes_required = length_ * int_size_;
  if (bytes_required > 0 && bytes_required < data_->size()) {
    // Trim buffers
    RETURN_NOT_OK(data_->Resize(bytes_required));
  }

  std::shared_ptr<DataType> output_type;
  switch (int_size_) {
    case 1:
      output_type = int8();
      break;
    case 2:
      output_type = int16();
      break;
    case 4:
      output_type = int32();
      break;
    case 8:
      output_type = int64();
      break;
    default:
      DCHECK(false);
      return Status::NotImplemented("Only ints of size 1,2,4,8 are supported");
  }

  *out = ArrayData::Make(output_type, length_, {null_bitmap_, data_}, null_count_);

  data_ = null_bitmap_ = nullptr;
  capacity_ = length_ = null_count_ = 0;
  return Status::OK();
}

Status AdaptiveIntBuilder::Append(const int64_t* values, int64_t length,
                                  const uint8_t* valid_bytes) {
  RETURN_NOT_OK(Reserve(length));

  if (length > 0) {
    if (int_size_ < 8) {
      uint8_t new_int_size = int_size_;
      for (int64_t i = 0; i < length; i++) {
        if (valid_bytes == nullptr || valid_bytes[i]) {
          new_int_size = internal::ExpandedIntSize(values[i], new_int_size);
        }
      }
      if (new_int_size != int_size_) {
        RETURN_NOT_OK(ExpandIntSize(new_int_size));
      }
    }
  }

  if (int_size_ == 8) {
    std::memcpy(reinterpret_cast<int64_t*>(raw_data_) + length_, values,
                sizeof(int64_t) * length);
  } else {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    // int_size_ may have changed, so we need to recheck
    switch (int_size_) {
      case 1: {
        int8_t* data_ptr = reinterpret_cast<int8_t*>(raw_data_) + length_;
        std::transform(values, values + length, data_ptr,
                       [](int64_t x) { return static_cast<int8_t>(x); });
      } break;
      case 2: {
        int16_t* data_ptr = reinterpret_cast<int16_t*>(raw_data_) + length_;
        std::transform(values, values + length, data_ptr,
                       [](int64_t x) { return static_cast<int16_t>(x); });
      } break;
      case 4: {
        int32_t* data_ptr = reinterpret_cast<int32_t*>(raw_data_) + length_;
        std::transform(values, values + length, data_ptr,
                       [](int64_t x) { return static_cast<int32_t>(x); });
      } break;
      default:
        DCHECK(false);
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
  }

  // length_ is update by these
  ArrayBuilder::UnsafeAppendToBitmap(valid_bytes, length);

  return Status::OK();
}

template <typename new_type, typename old_type>
typename std::enable_if<sizeof(old_type) >= sizeof(new_type), Status>::type
AdaptiveIntBuilder::ExpandIntSizeInternal() {
  return Status::OK();
}

#define __LESS(a, b) (a) < (b)
template <typename new_type, typename old_type>
typename std::enable_if<__LESS(sizeof(old_type), sizeof(new_type)), Status>::type
AdaptiveIntBuilder::ExpandIntSizeInternal() {
  int_size_ = sizeof(new_type);
  RETURN_NOT_OK(Resize(data_->size() / sizeof(old_type)));

  old_type* src = reinterpret_cast<old_type*>(raw_data_);
  new_type* dst = reinterpret_cast<new_type*>(raw_data_);
  // By doing the backward copy, we ensure that no element is overriden during
  // the copy process and the copy stays in-place.
  std::copy_backward(src, src + length_, dst + length_);

  return Status::OK();
}
#undef __LESS

template <typename new_type>
Status AdaptiveIntBuilder::ExpandIntSizeN() {
  switch (int_size_) {
    case 1:
      RETURN_NOT_OK((ExpandIntSizeInternal<new_type, int8_t>()));
      break;
    case 2:
      RETURN_NOT_OK((ExpandIntSizeInternal<new_type, int16_t>()));
      break;
    case 4:
      RETURN_NOT_OK((ExpandIntSizeInternal<new_type, int32_t>()));
      break;
    case 8:
      RETURN_NOT_OK((ExpandIntSizeInternal<new_type, int64_t>()));
      break;
    default:
      DCHECK(false);
  }
  return Status::OK();
}

Status AdaptiveIntBuilder::ExpandIntSize(uint8_t new_int_size) {
  switch (new_int_size) {
    case 1:
      RETURN_NOT_OK((ExpandIntSizeN<int8_t>()));
      break;
    case 2:
      RETURN_NOT_OK((ExpandIntSizeN<int16_t>()));
      break;
    case 4:
      RETURN_NOT_OK((ExpandIntSizeN<int32_t>()));
      break;
    case 8:
      RETURN_NOT_OK((ExpandIntSizeN<int64_t>()));
      break;
    default:
      DCHECK(false);
  }
  return Status::OK();
}

AdaptiveUIntBuilder::AdaptiveUIntBuilder(MemoryPool* pool)
    : AdaptiveIntBuilderBase(pool) {}

Status AdaptiveUIntBuilder::FinishInternal(std::shared_ptr<ArrayData>* out) {
  const int64_t bytes_required = length_ * int_size_;
  if (bytes_required > 0 && bytes_required < data_->size()) {
    // Trim buffers
    RETURN_NOT_OK(data_->Resize(bytes_required));
  }
  std::shared_ptr<DataType> output_type;
  switch (int_size_) {
    case 1:
      output_type = uint8();
      break;
    case 2:
      output_type = uint16();
      break;
    case 4:
      output_type = uint32();
      break;
    case 8:
      output_type = uint64();
      break;
    default:
      DCHECK(false);
      return Status::NotImplemented("Only ints of size 1,2,4,8 are supported");
  }

  *out = ArrayData::Make(output_type, length_, {null_bitmap_, data_}, null_count_);

  data_ = null_bitmap_ = nullptr;
  capacity_ = length_ = null_count_ = 0;
  return Status::OK();
}

Status AdaptiveUIntBuilder::Append(const uint64_t* values, int64_t length,
                                   const uint8_t* valid_bytes) {
  RETURN_NOT_OK(Reserve(length));

  if (length > 0) {
    if (int_size_ < 8) {
      uint8_t new_int_size = int_size_;
      for (int64_t i = 0; i < length; i++) {
        if (valid_bytes == nullptr || valid_bytes[i]) {
          new_int_size = internal::ExpandedUIntSize(values[i], new_int_size);
        }
      }
      if (new_int_size != int_size_) {
        RETURN_NOT_OK(ExpandIntSize(new_int_size));
      }
    }
  }

  if (int_size_ == 8) {
    std::memcpy(reinterpret_cast<uint64_t*>(raw_data_) + length_, values,
                sizeof(uint64_t) * length);
  } else {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    // int_size_ may have changed, so we need to recheck
    switch (int_size_) {
      case 1: {
        uint8_t* data_ptr = reinterpret_cast<uint8_t*>(raw_data_) + length_;
        std::transform(values, values + length, data_ptr,
                       [](uint64_t x) { return static_cast<uint8_t>(x); });
      } break;
      case 2: {
        uint16_t* data_ptr = reinterpret_cast<uint16_t*>(raw_data_) + length_;
        std::transform(values, values + length, data_ptr,
                       [](uint64_t x) { return static_cast<uint16_t>(x); });
      } break;
      case 4: {
        uint32_t* data_ptr = reinterpret_cast<uint32_t*>(raw_data_) + length_;
        std::transform(values, values + length, data_ptr,
                       [](uint64_t x) { return static_cast<uint32_t>(x); });
      } break;
      default:
        DCHECK(false);
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
  }

  // length_ is update by these
  ArrayBuilder::UnsafeAppendToBitmap(valid_bytes, length);

  return Status::OK();
}

template <typename new_type, typename old_type>
typename std::enable_if<sizeof(old_type) >= sizeof(new_type), Status>::type
AdaptiveUIntBuilder::ExpandIntSizeInternal() {
  return Status::OK();
}

#define __LESS(a, b) (a) < (b)
template <typename new_type, typename old_type>
typename std::enable_if<__LESS(sizeof(old_type), sizeof(new_type)), Status>::type
AdaptiveUIntBuilder::ExpandIntSizeInternal() {
  int_size_ = sizeof(new_type);
  RETURN_NOT_OK(Resize(data_->size() / sizeof(old_type)));

  old_type* src = reinterpret_cast<old_type*>(raw_data_);
  new_type* dst = reinterpret_cast<new_type*>(raw_data_);
  // By doing the backward copy, we ensure that no element is overriden during
  // the copy process and the copy stays in-place.
  std::copy_backward(src, src + length_, dst + length_);

  return Status::OK();
}
#undef __LESS

template <typename new_type>
Status AdaptiveUIntBuilder::ExpandIntSizeN() {
  switch (int_size_) {
    case 1:
      RETURN_NOT_OK((ExpandIntSizeInternal<new_type, uint8_t>()));
      break;
    case 2:
      RETURN_NOT_OK((ExpandIntSizeInternal<new_type, uint16_t>()));
      break;
    case 4:
      RETURN_NOT_OK((ExpandIntSizeInternal<new_type, uint32_t>()));
      break;
    case 8:
      RETURN_NOT_OK((ExpandIntSizeInternal<new_type, uint64_t>()));
      break;
    default:
      DCHECK(false);
  }
  return Status::OK();
}

Status AdaptiveUIntBuilder::ExpandIntSize(uint8_t new_int_size) {
  switch (new_int_size) {
    case 1:
      RETURN_NOT_OK((ExpandIntSizeN<uint8_t>()));
      break;
    case 2:
      RETURN_NOT_OK((ExpandIntSizeN<uint16_t>()));
      break;
    case 4:
      RETURN_NOT_OK((ExpandIntSizeN<uint32_t>()));
      break;
    case 8:
      RETURN_NOT_OK((ExpandIntSizeN<uint64_t>()));
      break;
    default:
      DCHECK(false);
  }
  return Status::OK();
}

BooleanBuilder::BooleanBuilder(MemoryPool* pool)
    : ArrayBuilder(boolean(), pool), data_(nullptr), raw_data_(nullptr) {}

BooleanBuilder::BooleanBuilder(const std::shared_ptr<DataType>& type, MemoryPool* pool)
    : BooleanBuilder(pool) {
  DCHECK_EQ(Type::BOOL, type->id());
}

Status BooleanBuilder::Init(int64_t capacity) {
  RETURN_NOT_OK(ArrayBuilder::Init(capacity));
  data_ = std::make_shared<PoolBuffer>(pool_);

  int64_t nbytes = BitUtil::BytesForBits(capacity);
  RETURN_NOT_OK(data_->Resize(nbytes));
  // TODO(emkornfield) valgrind complains without this
  memset(data_->mutable_data(), 0, static_cast<size_t>(nbytes));

  raw_data_ = reinterpret_cast<uint8_t*>(data_->mutable_data());
  return Status::OK();
}

Status BooleanBuilder::Resize(int64_t capacity) {
  // XXX: Set floor size for now
  if (capacity < kMinBuilderCapacity) {
    capacity = kMinBuilderCapacity;
  }

  if (capacity_ == 0) {
    RETURN_NOT_OK(Init(capacity));
  } else {
    RETURN_NOT_OK(ArrayBuilder::Resize(capacity));
    const int64_t old_bytes = data_->size();
    const int64_t new_bytes = BitUtil::BytesForBits(capacity);

    RETURN_NOT_OK(data_->Resize(new_bytes));
    raw_data_ = reinterpret_cast<uint8_t*>(data_->mutable_data());
    memset(data_->mutable_data() + old_bytes, 0,
           static_cast<size_t>(new_bytes - old_bytes));
  }
  return Status::OK();
}

Status BooleanBuilder::FinishInternal(std::shared_ptr<ArrayData>* out) {
  const int64_t bytes_required = BitUtil::BytesForBits(length_);

  if (bytes_required > 0 && bytes_required < data_->size()) {
    // Trim buffers
    RETURN_NOT_OK(data_->Resize(bytes_required));
  }
  *out = ArrayData::Make(boolean(), length_, {null_bitmap_, data_}, null_count_);

  data_ = null_bitmap_ = nullptr;
  capacity_ = length_ = null_count_ = 0;
  return Status::OK();
}

Status BooleanBuilder::Append(const uint8_t* values, int64_t length,
                              const uint8_t* valid_bytes) {
  RETURN_NOT_OK(Reserve(length));

  for (int64_t i = 0; i < length; ++i) {
    BitUtil::SetBitTo(raw_data_, length_ + i, values[i] != 0);
  }

  // this updates length_
  ArrayBuilder::UnsafeAppendToBitmap(valid_bytes, length);
  return Status::OK();
}

Status BooleanBuilder::Append(const uint8_t* values, int64_t length,
                              const std::vector<bool>& is_valid) {
  RETURN_NOT_OK(Reserve(length));
  DCHECK_EQ(length, static_cast<int64_t>(is_valid.size()));

  for (int64_t i = 0; i < length; ++i) {
    BitUtil::SetBitTo(raw_data_, length_ + i, values[i] != 0);
  }

  // this updates length_
  ArrayBuilder::UnsafeAppendToBitmap(is_valid);
  return Status::OK();
}

Status BooleanBuilder::Append(const std::vector<uint8_t>& values,
                              const std::vector<bool>& is_valid) {
  return Append(values.data(), static_cast<int64_t>(values.size()), is_valid);
}

Status BooleanBuilder::Append(const std::vector<uint8_t>& values) {
  return Append(values.data(), static_cast<int64_t>(values.size()));
}

Status BooleanBuilder::Append(const std::vector<bool>& values,
                              const std::vector<bool>& is_valid) {
  const int64_t length = static_cast<int64_t>(values.size());
  RETURN_NOT_OK(Reserve(length));
  DCHECK_EQ(length, static_cast<int64_t>(is_valid.size()));

  for (int64_t i = 0; i < length; ++i) {
    BitUtil::SetBitTo(raw_data_, length_ + i, values[i]);
  }

  // this updates length_
  ArrayBuilder::UnsafeAppendToBitmap(is_valid);
  return Status::OK();
}

Status BooleanBuilder::Append(const std::vector<bool>& values) {
  const int64_t length = static_cast<int64_t>(values.size());
  RETURN_NOT_OK(Reserve(length));

  for (int64_t i = 0; i < length; ++i) {
    BitUtil::SetBitTo(raw_data_, length_ + i, values[i]);
  }

  ArrayBuilder::UnsafeSetNotNull(length);
  return Status::OK();
}

// ----------------------------------------------------------------------
// DictionaryBuilder

using internal::WrappedBinary;

template <typename T>
DictionaryBuilder<T>::DictionaryBuilder(const std::shared_ptr<DataType>& type,
                                        MemoryPool* pool)
    : ArrayBuilder(type, pool),
      hash_slots_(nullptr),
      dict_builder_(type, pool),
      overflow_dict_builder_(type, pool),
      values_builder_(pool),
      byte_width_(-1) {
  if (!::arrow::CpuInfo::initialized()) {
    ::arrow::CpuInfo::Init();
  }
}

DictionaryBuilder<NullType>::DictionaryBuilder(const std::shared_ptr<DataType>& type,
                                               MemoryPool* pool)
    : ArrayBuilder(type, pool), values_builder_(pool) {
  if (!::arrow::CpuInfo::initialized()) {
    ::arrow::CpuInfo::Init();
  }
}

DictionaryBuilder<NullType>::~DictionaryBuilder() {}

template <>
DictionaryBuilder<FixedSizeBinaryType>::DictionaryBuilder(
    const std::shared_ptr<DataType>& type, MemoryPool* pool)
    : ArrayBuilder(type, pool),
      hash_slots_(nullptr),
      dict_builder_(type, pool),
      overflow_dict_builder_(type, pool),
      values_builder_(pool),
      byte_width_(static_cast<const FixedSizeBinaryType&>(*type).byte_width()) {
  if (!::arrow::CpuInfo::initialized()) {
    ::arrow::CpuInfo::Init();
  }
}

template <typename T>
Status DictionaryBuilder<T>::Init(int64_t elements) {
  RETURN_NOT_OK(ArrayBuilder::Init(elements));

  // Fill the initial hash table
  RETURN_NOT_OK(internal::NewHashTable(kInitialHashTableSize, pool_, &hash_table_));
  hash_slots_ = reinterpret_cast<int32_t*>(hash_table_->mutable_data());
  hash_table_size_ = kInitialHashTableSize;
  entry_id_offset_ = 0;
  mod_bitmask_ = kInitialHashTableSize - 1;
  hash_table_load_threshold_ =
      static_cast<int64_t>(static_cast<double>(elements) * kMaxHashTableLoad);

  return values_builder_.Init(elements);
}

Status DictionaryBuilder<NullType>::Init(int64_t elements) {
  RETURN_NOT_OK(ArrayBuilder::Init(elements));
  return values_builder_.Init(elements);
}

template <typename T>
Status DictionaryBuilder<T>::Resize(int64_t capacity) {
  if (capacity < kMinBuilderCapacity) {
    capacity = kMinBuilderCapacity;
  }

  if (capacity_ == 0) {
    return Init(capacity);
  } else {
    return ArrayBuilder::Resize(capacity);
  }
}

Status DictionaryBuilder<NullType>::Resize(int64_t capacity) {
  if (capacity < kMinBuilderCapacity) {
    capacity = kMinBuilderCapacity;
  }

  if (capacity_ == 0) {
    return Init(capacity);
  } else {
    return ArrayBuilder::Resize(capacity);
  }
}

template <typename T>
Status DictionaryBuilder<T>::Append(const Scalar& value) {
  RETURN_NOT_OK(Reserve(1));
  // Based on DictEncoder<DType>::Put
  int64_t j = HashValue(value) & mod_bitmask_;
  hash_slot_t index = hash_slots_[j];

  // Find an empty slot
  while (kHashSlotEmpty != index && SlotDifferent(index, value)) {
    // Linear probing
    ++j;
    if (j == hash_table_size_) {
      j = 0;
    }
    index = hash_slots_[j];
  }

  if (index == kHashSlotEmpty) {
    // Not in the hash table, so we insert it now
    index = static_cast<hash_slot_t>(dict_builder_.length() + entry_id_offset_);
    hash_slots_[j] = index;
    RETURN_NOT_OK(AppendDictionary(value));

    if (ARROW_PREDICT_FALSE(static_cast<int32_t>(dict_builder_.length()) >
                            hash_table_load_threshold_)) {
      RETURN_NOT_OK(DoubleTableSize());
    }
  }

  RETURN_NOT_OK(values_builder_.Append(index));

  return Status::OK();
}

template <typename T>
Status DictionaryBuilder<T>::AppendArray(const Array& array) {
  const auto& numeric_array = static_cast<const NumericArray<T>&>(array);
  for (int64_t i = 0; i < array.length(); i++) {
    if (array.IsNull(i)) {
      RETURN_NOT_OK(AppendNull());
    } else {
      RETURN_NOT_OK(Append(numeric_array.Value(i)));
    }
  }
  return Status::OK();
}

Status DictionaryBuilder<NullType>::AppendArray(const Array& array) {
  for (int64_t i = 0; i < array.length(); i++) {
    RETURN_NOT_OK(AppendNull());
  }
  return Status::OK();
}

template <>
Status DictionaryBuilder<FixedSizeBinaryType>::AppendArray(const Array& array) {
  if (!type_->Equals(*array.type())) {
    return Status::Invalid("Cannot append FixedSizeBinary array with non-matching type");
  }

  const auto& numeric_array = static_cast<const FixedSizeBinaryArray&>(array);
  for (int64_t i = 0; i < array.length(); i++) {
    if (array.IsNull(i)) {
      RETURN_NOT_OK(AppendNull());
    } else {
      RETURN_NOT_OK(Append(numeric_array.Value(i)));
    }
  }
  return Status::OK();
}

template <typename T>
Status DictionaryBuilder<T>::AppendNull() {
  return values_builder_.AppendNull();
}

Status DictionaryBuilder<NullType>::AppendNull() { return values_builder_.AppendNull(); }

template <typename T>
Status DictionaryBuilder<T>::DoubleTableSize() {
#define INNER_LOOP                                                               \
  Scalar value = GetDictionaryValue(dict_builder_, static_cast<int64_t>(index)); \
  int64_t j = HashValue(value) & new_mod_bitmask;

  DOUBLE_TABLE_SIZE(, INNER_LOOP);

  return Status::OK();
}

template <typename T>
typename DictionaryBuilder<T>::Scalar DictionaryBuilder<T>::GetDictionaryValue(
    typename TypeTraits<T>::BuilderType& dictionary_builder, int64_t index) {
  const Scalar* data = reinterpret_cast<const Scalar*>(dictionary_builder.data()->data());
  return data[index];
}

template <typename T>
Status DictionaryBuilder<T>::FinishInternal(std::shared_ptr<ArrayData>* out) {
  entry_id_offset_ += dict_builder_.length();
  RETURN_NOT_OK(overflow_dict_builder_.Append(
      reinterpret_cast<const DictionaryBuilder<T>::Scalar*>(dict_builder_.data()->data()),
      dict_builder_.length(), nullptr));

  std::shared_ptr<Array> dictionary;
  RETURN_NOT_OK(dict_builder_.Finish(&dictionary));

  RETURN_NOT_OK(values_builder_.FinishInternal(out));
  (*out)->type = std::make_shared<DictionaryType>((*out)->type, dictionary);

  RETURN_NOT_OK(dict_builder_.Init(capacity_));
  RETURN_NOT_OK(values_builder_.Init(capacity_));
  return Status::OK();
}

Status DictionaryBuilder<NullType>::FinishInternal(std::shared_ptr<ArrayData>* out) {
  std::shared_ptr<Array> dictionary = std::make_shared<NullArray>(0);

  RETURN_NOT_OK(values_builder_.FinishInternal(out));
  (*out)->type = std::make_shared<DictionaryType>((*out)->type, dictionary);
  return Status::OK();
}

template <>
const uint8_t* DictionaryBuilder<FixedSizeBinaryType>::GetDictionaryValue(
    typename TypeTraits<FixedSizeBinaryType>::BuilderType& dictionary_builder,
    int64_t index) {
  return dictionary_builder.GetValue(index);
}

template <>
Status DictionaryBuilder<FixedSizeBinaryType>::FinishInternal(
    std::shared_ptr<ArrayData>* out) {
  entry_id_offset_ += dict_builder_.length();

  for (uint64_t index = 0, limit = dict_builder_.length(); index < limit; ++index) {
    const Scalar value = GetDictionaryValue(dict_builder_, index);
    RETURN_NOT_OK(overflow_dict_builder_.Append(value));
  }

  std::shared_ptr<Array> dictionary;
  RETURN_NOT_OK(dict_builder_.Finish(&dictionary));

  RETURN_NOT_OK(values_builder_.FinishInternal(out));
  (*out)->type = std::make_shared<DictionaryType>((*out)->type, dictionary);

  RETURN_NOT_OK(dict_builder_.Init(capacity_));
  RETURN_NOT_OK(values_builder_.Init(capacity_));

  return Status::OK();
}

template <typename T>
int64_t DictionaryBuilder<T>::HashValue(const Scalar& value) {
  return HashUtil::Hash(&value, sizeof(Scalar), 0);
}

template <>
int64_t DictionaryBuilder<FixedSizeBinaryType>::HashValue(const Scalar& value) {
  return HashUtil::Hash(value, byte_width_, 0);
}

template <typename T>
bool DictionaryBuilder<T>::SlotDifferent(hash_slot_t index, const Scalar& value) {
  const bool value_found =
      index >= entry_id_offset_ &&
      GetDictionaryValue(dict_builder_, static_cast<int64_t>(index - entry_id_offset_)) ==
          value;
  const bool value_found_overflow =
      entry_id_offset_ > 0 &&
      GetDictionaryValue(overflow_dict_builder_, static_cast<int64_t>(index)) == value;
  return !(value_found || value_found_overflow);
}

template <>
bool DictionaryBuilder<FixedSizeBinaryType>::SlotDifferent(hash_slot_t index,
                                                           const Scalar& value) {
  int32_t width = static_cast<const FixedSizeBinaryType&>(*type_).byte_width();
  bool value_found = false;
  if (index >= entry_id_offset_) {
    const Scalar other =
        GetDictionaryValue(dict_builder_, static_cast<int64_t>(index - entry_id_offset_));
    value_found = memcmp(other, value, width) == 0;
  }

  bool value_found_overflow = false;
  if (entry_id_offset_ > 0) {
    const Scalar other_overflow =
        GetDictionaryValue(overflow_dict_builder_, static_cast<int64_t>(index));
    value_found_overflow = memcmp(other_overflow, value, width) == 0;
  }
  return !(value_found || value_found_overflow);
}

template <typename T>
Status DictionaryBuilder<T>::AppendDictionary(const Scalar& value) {
  return dict_builder_.Append(value);
}

#define BINARY_DICTIONARY_SPECIALIZATIONS(Type)                                        \
  template <>                                                                          \
  WrappedBinary DictionaryBuilder<Type>::GetDictionaryValue(                           \
      typename TypeTraits<Type>::BuilderType& dictionary_builder, int64_t index) {     \
    int32_t v_len;                                                                     \
    const uint8_t* v = dictionary_builder.GetValue(                                    \
        static_cast<int64_t>(index - entry_id_offset_), &v_len);                       \
    return WrappedBinary(v, v_len);                                                    \
  }                                                                                    \
                                                                                       \
  template <>                                                                          \
  Status DictionaryBuilder<Type>::AppendDictionary(const WrappedBinary& value) {       \
    return dict_builder_.Append(value.ptr_, value.length_);                            \
  }                                                                                    \
                                                                                       \
  template <>                                                                          \
  Status DictionaryBuilder<Type>::AppendArray(const Array& array) {                    \
    const BinaryArray& binary_array = static_cast<const BinaryArray&>(array);          \
    WrappedBinary value(nullptr, 0);                                                   \
    for (int64_t i = 0; i < array.length(); i++) {                                     \
      if (array.IsNull(i)) {                                                           \
        RETURN_NOT_OK(AppendNull());                                                   \
      } else {                                                                         \
        value.ptr_ = binary_array.GetValue(i, &value.length_);                         \
        RETURN_NOT_OK(Append(value));                                                  \
      }                                                                                \
    }                                                                                  \
    return Status::OK();                                                               \
  }                                                                                    \
                                                                                       \
  template <>                                                                          \
  int64_t DictionaryBuilder<Type>::HashValue(const WrappedBinary& value) {             \
    return HashUtil::Hash(value.ptr_, value.length_, 0);                               \
  }                                                                                    \
                                                                                       \
  template <>                                                                          \
  bool DictionaryBuilder<Type>::SlotDifferent(hash_slot_t index,                       \
                                              const WrappedBinary& value) {            \
    int32_t other_length;                                                              \
    bool value_found = false;                                                          \
    if (index >= entry_id_offset_) {                                                   \
      const uint8_t* other_value = dict_builder_.GetValue(                             \
          static_cast<int64_t>(index - entry_id_offset_), &other_length);              \
      value_found = other_length == value.length_ &&                                   \
                    memcmp(other_value, value.ptr_, value.length_) == 0;               \
    }                                                                                  \
                                                                                       \
    bool value_found_overflow = false;                                                 \
    if (entry_id_offset_ > 0) {                                                        \
      const uint8_t* other_value_overflow =                                            \
          overflow_dict_builder_.GetValue(static_cast<int64_t>(index), &other_length); \
      value_found_overflow =                                                           \
          other_length == value.length_ &&                                             \
          memcmp(other_value_overflow, value.ptr_, value.length_) == 0;                \
    }                                                                                  \
    return !(value_found || value_found_overflow);                                     \
  }                                                                                    \
                                                                                       \
  template <>                                                                          \
  Status DictionaryBuilder<Type>::FinishInternal(std::shared_ptr<ArrayData>* out) {    \
    entry_id_offset_ += dict_builder_.length();                                        \
    for (uint64_t index = 0, limit = dict_builder_.length(); index < limit; ++index) { \
      int32_t out_length;                                                              \
      const uint8_t* value = dict_builder_.GetValue(index, &out_length);               \
      RETURN_NOT_OK(overflow_dict_builder_.Append(value, out_length));                 \
    }                                                                                  \
                                                                                       \
    std::shared_ptr<Array> dictionary;                                                 \
    RETURN_NOT_OK(dict_builder_.Finish(&dictionary));                                  \
                                                                                       \
    RETURN_NOT_OK(values_builder_.FinishInternal(out));                                \
    (*out)->type = std::make_shared<DictionaryType>((*out)->type, dictionary);         \
                                                                                       \
    RETURN_NOT_OK(dict_builder_.Init(capacity_));                                      \
    RETURN_NOT_OK(values_builder_.Init(capacity_));                                    \
    return Status::OK();                                                               \
  }

BINARY_DICTIONARY_SPECIALIZATIONS(StringType);
BINARY_DICTIONARY_SPECIALIZATIONS(BinaryType);

template class DictionaryBuilder<UInt8Type>;
template class DictionaryBuilder<UInt16Type>;
template class DictionaryBuilder<UInt32Type>;
template class DictionaryBuilder<UInt64Type>;
template class DictionaryBuilder<Int8Type>;
template class DictionaryBuilder<Int16Type>;
template class DictionaryBuilder<Int32Type>;
template class DictionaryBuilder<Int64Type>;
template class DictionaryBuilder<Date32Type>;
template class DictionaryBuilder<Date64Type>;
template class DictionaryBuilder<Time32Type>;
template class DictionaryBuilder<Time64Type>;
template class DictionaryBuilder<TimestampType>;
template class DictionaryBuilder<FloatType>;
template class DictionaryBuilder<DoubleType>;
template class DictionaryBuilder<FixedSizeBinaryType>;
template class DictionaryBuilder<BinaryType>;
template class DictionaryBuilder<StringType>;

// ----------------------------------------------------------------------
// Decimal128Builder

Decimal128Builder::Decimal128Builder(const std::shared_ptr<DataType>& type,
                                     MemoryPool* pool)
    : FixedSizeBinaryBuilder(type, pool) {}

Status Decimal128Builder::Append(const Decimal128& value) {
  RETURN_NOT_OK(FixedSizeBinaryBuilder::Reserve(1));
  return FixedSizeBinaryBuilder::Append(value.ToBytes());
}

Status Decimal128Builder::FinishInternal(std::shared_ptr<ArrayData>* out) {
  std::shared_ptr<Buffer> data;
  RETURN_NOT_OK(byte_builder_.Finish(&data));

  *out = ArrayData::Make(type_, length_, {null_bitmap_, data}, null_count_);
  return Status::OK();
}

// ----------------------------------------------------------------------
// ListBuilder

ListBuilder::ListBuilder(MemoryPool* pool, std::unique_ptr<ArrayBuilder> value_builder,
                         const std::shared_ptr<DataType>& type)
    : ArrayBuilder(type ? type
                        : std::static_pointer_cast<DataType>(
                              std::make_shared<ListType>(value_builder->type())),
                   pool),
      offsets_builder_(pool),
      value_builder_(std::move(value_builder)) {}

Status ListBuilder::Append(const int32_t* offsets, int64_t length,
                           const uint8_t* valid_bytes) {
  RETURN_NOT_OK(Reserve(length));
  UnsafeAppendToBitmap(valid_bytes, length);
  offsets_builder_.UnsafeAppend(offsets, length);
  return Status::OK();
}

Status ListBuilder::AppendNextOffset() {
  int64_t num_values = value_builder_->length();
  if (ARROW_PREDICT_FALSE(num_values > kListMaximumElements)) {
    std::stringstream ss;
    ss << "ListArray cannot contain more then INT32_MAX - 1 child elements,"
       << " have " << num_values;
    return Status::Invalid(ss.str());
  }
  return offsets_builder_.Append(static_cast<int32_t>(num_values));
}

Status ListBuilder::Append(bool is_valid) {
  RETURN_NOT_OK(Reserve(1));
  UnsafeAppendToBitmap(is_valid);
  return AppendNextOffset();
}

Status ListBuilder::Init(int64_t elements) {
  DCHECK_LE(elements, kListMaximumElements);
  RETURN_NOT_OK(ArrayBuilder::Init(elements));
  // one more then requested for offsets
  return offsets_builder_.Resize((elements + 1) * sizeof(int32_t));
}

Status ListBuilder::Resize(int64_t capacity) {
  DCHECK_LE(capacity, kListMaximumElements);
  // one more then requested for offsets
  RETURN_NOT_OK(offsets_builder_.Resize((capacity + 1) * sizeof(int32_t)));
  return ArrayBuilder::Resize(capacity);
}

Status ListBuilder::FinishInternal(std::shared_ptr<ArrayData>* out) {
  RETURN_NOT_OK(AppendNextOffset());

  std::shared_ptr<Buffer> offsets;
  RETURN_NOT_OK(offsets_builder_.Finish(&offsets));

  std::shared_ptr<ArrayData> items;
  if (values_) {
    items = values_->data();
  } else {
    RETURN_NOT_OK(value_builder_->FinishInternal(&items));
  }

  *out = ArrayData::Make(type_, length_, {null_bitmap_, offsets}, null_count_);
  (*out)->child_data.emplace_back(std::move(items));
  Reset();
  return Status::OK();
}

void ListBuilder::Reset() {
  ArrayBuilder::Reset();
  values_ = nullptr;
}

ArrayBuilder* ListBuilder::value_builder() const {
  DCHECK(!values_) << "Using value builder is pointless when values_ is set";
  return value_builder_.get();
}

// ----------------------------------------------------------------------
// String and binary

BinaryBuilder::BinaryBuilder(const std::shared_ptr<DataType>& type, MemoryPool* pool)
    : ArrayBuilder(type, pool), offsets_builder_(pool), value_data_builder_(pool) {}

BinaryBuilder::BinaryBuilder(MemoryPool* pool) : BinaryBuilder(binary(), pool) {}

Status BinaryBuilder::Init(int64_t elements) {
  DCHECK_LE(elements, kListMaximumElements);
  RETURN_NOT_OK(ArrayBuilder::Init(elements));
  // one more then requested for offsets
  return offsets_builder_.Resize((elements + 1) * sizeof(int32_t));
}

Status BinaryBuilder::Resize(int64_t capacity) {
  DCHECK_LE(capacity, kListMaximumElements);
  // one more then requested for offsets
  RETURN_NOT_OK(offsets_builder_.Resize((capacity + 1) * sizeof(int32_t)));
  return ArrayBuilder::Resize(capacity);
}

Status BinaryBuilder::ReserveData(int64_t elements) {
  if (value_data_length() + elements > value_data_capacity()) {
    if (value_data_length() + elements > kBinaryMemoryLimit) {
      return Status::Invalid("Cannot reserve capacity larger than 2^31 - 1 for binary");
    }
    RETURN_NOT_OK(value_data_builder_.Reserve(elements));
  }
  return Status::OK();
}

Status BinaryBuilder::AppendNextOffset() {
  const int64_t num_bytes = value_data_builder_.length();
  if (ARROW_PREDICT_FALSE(num_bytes > kBinaryMemoryLimit)) {
    std::stringstream ss;
    ss << "BinaryArray cannot contain more than " << kBinaryMemoryLimit << " bytes, have "
       << num_bytes;
    return Status::Invalid(ss.str());
  }
  return offsets_builder_.Append(static_cast<int32_t>(num_bytes));
}

Status BinaryBuilder::Append(const uint8_t* value, int32_t length) {
  RETURN_NOT_OK(Reserve(1));
  RETURN_NOT_OK(AppendNextOffset());
  RETURN_NOT_OK(value_data_builder_.Append(value, length));
  UnsafeAppendToBitmap(true);
  return Status::OK();
}

Status BinaryBuilder::AppendNull() {
  RETURN_NOT_OK(AppendNextOffset());
  RETURN_NOT_OK(Reserve(1));
  UnsafeAppendToBitmap(false);
  return Status::OK();
}

Status BinaryBuilder::FinishInternal(std::shared_ptr<ArrayData>* out) {
  // Write final offset (values length)
  RETURN_NOT_OK(AppendNextOffset());
  std::shared_ptr<Buffer> offsets, value_data;

  RETURN_NOT_OK(offsets_builder_.Finish(&offsets));
  RETURN_NOT_OK(value_data_builder_.Finish(&value_data));

  *out = ArrayData::Make(type_, length_, {null_bitmap_, offsets, value_data}, null_count_,
                         0);
  Reset();
  return Status::OK();
}

void BinaryBuilder::Reset() {
  ArrayBuilder::Reset();
  offsets_builder_.Reset();
  value_data_builder_.Reset();
}

const uint8_t* BinaryBuilder::GetValue(int64_t i, int32_t* out_length) const {
  const int32_t* offsets = offsets_builder_.data();
  int32_t offset = offsets[i];
  if (i == (length_ - 1)) {
    *out_length = static_cast<int32_t>(value_data_builder_.length()) - offset;
  } else {
    *out_length = offsets[i + 1] - offset;
  }
  return value_data_builder_.data() + offset;
}

StringBuilder::StringBuilder(MemoryPool* pool) : BinaryBuilder(utf8(), pool) {}

Status StringBuilder::Append(const std::vector<std::string>& values,
                             uint8_t* null_bytes) {
  std::size_t total_length = std::accumulate(
      values.begin(), values.end(), 0ULL,
      [](uint64_t sum, const std::string& str) { return sum + str.size(); });
  RETURN_NOT_OK(Reserve(values.size()));
  RETURN_NOT_OK(value_data_builder_.Reserve(total_length));
  RETURN_NOT_OK(offsets_builder_.Reserve(values.size()));

  for (std::size_t i = 0; i < values.size(); ++i) {
    RETURN_NOT_OK(AppendNextOffset());
    if (null_bytes[i]) {
      UnsafeAppendToBitmap(false);
    } else {
      RETURN_NOT_OK(value_data_builder_.Append(
          reinterpret_cast<const uint8_t*>(values[i].data()), values[i].size()));
      UnsafeAppendToBitmap(true);
    }
  }
  return Status::OK();
}

// ----------------------------------------------------------------------
// Fixed width binary

FixedSizeBinaryBuilder::FixedSizeBinaryBuilder(const std::shared_ptr<DataType>& type,
                                               MemoryPool* pool)
    : ArrayBuilder(type, pool),
      byte_width_(static_cast<const FixedSizeBinaryType&>(*type).byte_width()),
      byte_builder_(pool) {}

Status FixedSizeBinaryBuilder::Append(const uint8_t* value) {
  RETURN_NOT_OK(Reserve(1));
  UnsafeAppendToBitmap(true);
  return byte_builder_.Append(value, byte_width_);
}

Status FixedSizeBinaryBuilder::Append(const uint8_t* data, int64_t length,
                                      const uint8_t* valid_bytes) {
  RETURN_NOT_OK(Reserve(length));
  UnsafeAppendToBitmap(valid_bytes, length);
  return byte_builder_.Append(data, length * byte_width_);
}

Status FixedSizeBinaryBuilder::Append(const std::string& value) {
  return Append(reinterpret_cast<const uint8_t*>(value.c_str()));
}

Status FixedSizeBinaryBuilder::AppendNull() {
  RETURN_NOT_OK(Reserve(1));
  UnsafeAppendToBitmap(false);
  return byte_builder_.Advance(byte_width_);
}

Status FixedSizeBinaryBuilder::Init(int64_t elements) {
  RETURN_NOT_OK(ArrayBuilder::Init(elements));
  return byte_builder_.Resize(elements * byte_width_);
}

Status FixedSizeBinaryBuilder::Resize(int64_t capacity) {
  RETURN_NOT_OK(byte_builder_.Resize(capacity * byte_width_));
  return ArrayBuilder::Resize(capacity);
}

Status FixedSizeBinaryBuilder::FinishInternal(std::shared_ptr<ArrayData>* out) {
  std::shared_ptr<Buffer> data;
  RETURN_NOT_OK(byte_builder_.Finish(&data));

  *out = ArrayData::Make(type_, length_, {null_bitmap_, data}, null_count_);

  null_bitmap_ = nullptr;
  capacity_ = length_ = null_count_ = 0;
  return Status::OK();
}

const uint8_t* FixedSizeBinaryBuilder::GetValue(int64_t i) const {
  const uint8_t* data_ptr = byte_builder_.data();
  return data_ptr + i * byte_width_;
}

// ----------------------------------------------------------------------
// Struct

StructBuilder::StructBuilder(const std::shared_ptr<DataType>& type, MemoryPool* pool,
                             std::vector<std::unique_ptr<ArrayBuilder>>&& field_builders)
    : ArrayBuilder(type, pool) {
  field_builders_ = std::move(field_builders);
}

Status StructBuilder::FinishInternal(std::shared_ptr<ArrayData>* out) {
  *out = ArrayData::Make(type_, length_, {null_bitmap_}, null_count_);

  (*out)->child_data.resize(field_builders_.size());
  for (size_t i = 0; i < field_builders_.size(); ++i) {
    RETURN_NOT_OK(field_builders_[i]->FinishInternal(&(*out)->child_data[i]));
  }

  null_bitmap_ = nullptr;
  capacity_ = length_ = null_count_ = 0;
  return Status::OK();
}

  // ----------------------------------------------------------------------
  // Helper functions

#define BUILDER_CASE(ENUM, BuilderType)      \
  case Type::ENUM:                           \
    out->reset(new BuilderType(type, pool)); \
    return Status::OK();

// Initially looked at doing this with vtables, but shared pointers makes it
// difficult
//
// TODO(wesm): come up with a less monolithic strategy
Status MakeBuilder(MemoryPool* pool, const std::shared_ptr<DataType>& type,
                   std::unique_ptr<ArrayBuilder>* out) {
  switch (type->id()) {
    case Type::NA: {
      out->reset(new NullBuilder(pool));
      return Status::OK();
    }
      BUILDER_CASE(UINT8, UInt8Builder);
      BUILDER_CASE(INT8, Int8Builder);
      BUILDER_CASE(UINT16, UInt16Builder);
      BUILDER_CASE(INT16, Int16Builder);
      BUILDER_CASE(UINT32, UInt32Builder);
      BUILDER_CASE(INT32, Int32Builder);
      BUILDER_CASE(UINT64, UInt64Builder);
      BUILDER_CASE(INT64, Int64Builder);
      BUILDER_CASE(DATE32, Date32Builder);
      BUILDER_CASE(DATE64, Date64Builder);
      BUILDER_CASE(TIME32, Time32Builder);
      BUILDER_CASE(TIME64, Time64Builder);
      BUILDER_CASE(TIMESTAMP, TimestampBuilder);
      BUILDER_CASE(BOOL, BooleanBuilder);
      BUILDER_CASE(HALF_FLOAT, HalfFloatBuilder);
      BUILDER_CASE(FLOAT, FloatBuilder);
      BUILDER_CASE(DOUBLE, DoubleBuilder);
      BUILDER_CASE(STRING, StringBuilder);
      BUILDER_CASE(BINARY, BinaryBuilder);
      BUILDER_CASE(FIXED_SIZE_BINARY, FixedSizeBinaryBuilder);
      BUILDER_CASE(DECIMAL, Decimal128Builder);
    case Type::LIST: {
      std::unique_ptr<ArrayBuilder> value_builder;
      std::shared_ptr<DataType> value_type =
          static_cast<ListType*>(type.get())->value_type();
      RETURN_NOT_OK(MakeBuilder(pool, value_type, &value_builder));
      out->reset(new ListBuilder(pool, std::move(value_builder)));
      return Status::OK();
    }

    case Type::STRUCT: {
      const std::vector<std::shared_ptr<Field>>& fields = type->children();
      std::vector<std::unique_ptr<ArrayBuilder>> values_builder;

      for (auto it : fields) {
        std::unique_ptr<ArrayBuilder> builder;
        RETURN_NOT_OK(MakeBuilder(pool, it->type(), &builder));
        values_builder.emplace_back(std::move(builder));
      }
      out->reset(new StructBuilder(type, pool, std::move(values_builder)));
      return Status::OK();
    }

    default: {
      std::stringstream ss;
      ss << "MakeBuilder: cannot construct builder for type " << type->ToString();
      return Status::NotImplemented(ss.str());
    }
  }
}

}  // namespace arrow
