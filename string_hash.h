// Copyright 2014. All rights reserved.
// Author: Nelson LIAO <liaoxin1014@gmail.com>
#ifndef STRING_HASH_H_
#define STRING_HASH_H_

#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include <string>
#include <map>

#include "glog/logging.h"
#include "gtest/gtest_prod.h"
#include "hash.h"

// namespace omg {

class Slice {
 public:
  Slice() : data_(NULL), length_(0) {}
  Slice(const char *data, size_t length) : data_(data), length_(length) {}
  explicit Slice(const std::string &str)
    : data_(str.data()), length_(str.length()) {}

  // copy ctor
  Slice(const Slice &other) : data_(other.data()), length_(other.length()) {}

  // copy assigner
  Slice& operator= (const Slice &other) {
    if (this != &other) {
      data_ = other.data();
      length_ = other.length();
    }
    return *this;
  }

  const char* data() const { return data_; }
  size_t length() const { return length_; }
  bool empty() const { return data_ == NULL || length_ == 0; }

 private:
  const char *data_;
  size_t length_;
};

template<class Value>
class StringHash {
 public:
  StringHash();
  ~StringHash();

  // attach table from outer memory
  bool Attach(const char *data, size_t length);
  bool IsAttached() const { return meta_ != NULL; }

  void Reset() {
    meta_ = NULL;
    index_ = NULL;
    data_ = NULL;
    index_mask_ = 0;
  }

  // interface for searching
  bool Search(const Slice &slice, Value *value) const;
  bool Search(const std::string &key, Value *value) const {
    return Search(Slice(key), value);
  }

 public:
  // interface for building this table

  // build from string map
  static bool Build(const std::map<std::string, Value> &data,
                    std::string *buffer) {
    return BuildImpl(data, buffer);
  }

  // build from slice map
  static bool Build(const std::map<Slice, Value> &data,
                    std::string *buffer) {
    return BuildImpl(data, buffer);
  }

 public:
  struct Meta {
    uint32_t magic;
    uint32_t fixed_value_size;
    uint64_t index_offset;
    uint64_t index_count;
    uint64_t data_offset;
    uint64_t data_length;
    uint64_t data_count;

    uint8_t reserved[128 - 48];

    inline Meta() {
      Reset();
    }
    inline void Reset() {
      ::memset(this, 0, sizeof(*this));
    }
  } __attribute__((packed));

  static const uint32_t kMagic = 0x1234abcd;

  const Meta* meta() const {
    return meta_;
  }

 private:
  template<class Key>
  static bool BuildImpl(const std::map<Key, Value> &data,
                        std::string *buffer);

  static uint64_t GetMask(uint64_t d) {
    uint64_t log2_value = static_cast<uint64_t>(log(d > 0 ? d : 1)/log(2.0));
    return (2 << log2_value) - 1;
  }

  // data structure
  struct DataBlock {
    // offset for the next data_block
    uint64_t next;

    // hash value, for compare
    uint64_t hash;

    // fixed
    Value value;

    // key with variable length
    uint32_t key_size;
    char key_data[0];

    inline DataBlock() {
      Reset();
    }
    inline void Reset() {
      ::memset(this, 0, sizeof(*this));
    }
    inline uint64_t total_size() const {
      return sizeof(DataBlock) + key_size;
    }
  } __attribute__((packed));

  // ensure offset=0 is end
  static const uint64_t kDataBlockPaddingOffset = 16;

  // index structure
  struct IndexBlock {
    // offset for the first data_block
    uint64_t first;

    inline IndexBlock() {
      Reset();
    }
    inline void Reset() {
      ::memset(this, 0, sizeof(*this));
    }
  } __attribute__((packed));

  // meta information and head ptr of memory
  const Meta *meta_;

  // index
  const IndexBlock *index_;
  uint64_t index_mask_;

  // data
  const char *data_;

  // for gtest
  FRIEND_TEST(StringHashTest, GetMask);
};

// Implementation
//

template<class Value>
StringHash<Value>::StringHash() {
  Reset();
}

template<class Value>
StringHash<Value>::~StringHash() {}

template<class Value>
bool StringHash<Value>::Attach(const char *data, size_t length) {
  // check meta
  if (length < sizeof(Meta)) {
    return false;
  }
  const Meta* meta = reinterpret_cast<const Meta*>(data);
  if (meta->magic != kMagic || meta->fixed_value_size != sizeof(Value)) {
    return false;
  }

  // check total length
  uint64_t total_length = sizeof(Meta) +  // size of Header
      sizeof(IndexBlock) * meta->index_count +  // size of index blocks
      meta->data_length;  // size of data blocks
  if (length < total_length) {
    return false;
  }

  // attach OK
  meta_ = meta;
  index_ = reinterpret_cast<const IndexBlock*>(data + meta->index_offset);
  index_mask_ = meta->index_count - 1;
  data_ = data + meta->data_offset;

  return true;
}

template<class Value>
template<class Key>
bool StringHash<Value>::BuildImpl(
    const std::map<Key, Value> &data,
    std::string *buffer) {
  // calculate length of data nodes
  uint64_t data_length = kDataBlockPaddingOffset;
  for (typename std::map<Key, Value>::const_iterator it = data.begin();
      it != data.end(); ++it) {
    data_length += sizeof(DataBlock) + it->first.length();
  }

  // calculate index length
  uint64_t data_count = data.size();
  uint64_t index_count = GetMask(data_count) + 1;

  // calculate total length
  uint64_t total_length = sizeof(Meta) +  // size of Header
      sizeof(IndexBlock) * index_count +  // size of index blocks
      data_length;  // size of data blocks

  buffer->resize(total_length);

  // meta
  Meta *meta = reinterpret_cast<Meta*>(&(*buffer)[0]);
  meta->Reset();
  meta->magic = kMagic;
  meta->fixed_value_size = sizeof(Value);
  meta->index_offset = sizeof(*meta);
  meta->index_count = index_count;
  meta->data_offset = meta->index_offset + sizeof(IndexBlock) * index_count;
  meta->data_length = data_length;
  meta->data_count = data_count;

  // index
  IndexBlock *index_block = reinterpret_cast<IndexBlock*>(&(*buffer)[0] + meta->index_offset);
  for (size_t i = 0; i < index_count; ++i) {
    index_block[i].Reset();
  }

  // data
  char *data_block_base = static_cast<char*>(&(*buffer)[0] + meta->data_offset);
  char *ptr = data_block_base + kDataBlockPaddingOffset;
  for (typename std::map<Key, Value>::const_iterator it = data.begin();
      it != data.end(); ++it) {
    // set up block
    DataBlock *data_block = reinterpret_cast<DataBlock*>(ptr);
    data_block->Reset();
    data_block->hash = hash_string(it->first.data(), it->first.length());
    data_block->value = it->second;
    data_block->key_size = it->first.length();
    ::memcpy(data_block->key_data, it->first.data(), data_block->key_size);

    // insert into index
    uint64_t index = data_block->hash & (index_count - 1);
    data_block->next = index_block[index].first;
    index_block[index].first = reinterpret_cast<char*>(data_block) - data_block_base;

    // calculate next offset
    ptr += data_block->total_size();
  }
  CHECK_EQ(data_block_base + data_length, ptr);

  return true;
}

template<class Value>
bool StringHash<Value>::Search(const Slice &slice, Value *value) const {
  if (!IsAttached()) {
    return false;
  }

  uint64_t hash = hash_string(slice.data(), slice.length());
  uint64_t index = hash & index_mask_;
  uint64_t offset = index_[index].first;

  while (offset != 0) {
    const DataBlock *data_block = reinterpret_cast<const DataBlock*>(data_ + offset);

    // compare hash and key
    if (data_block->hash == hash &&
        data_block->key_size == slice.length() &&
        0 == memcmp(data_block->key_data, slice.data(), data_block->key_size)) {
      *value = data_block->value;
      return true;
    }

    // update offset
    offset = data_block->next;
  }

  // not found
  return false;
}

// }  // namespace omg

#endif  // STRING_HASH_H_
