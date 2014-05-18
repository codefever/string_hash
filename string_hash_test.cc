// Copyright 2014. All rights reserved.
// Author: Nelson LIAO <liaoxin1014@gmail.com>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "string_hash.h"

template<typename T>
std::string ConvertToString(const T& t) {
  std::ostringstream oss;
  oss << t;
  return oss.str();
}

double WallTime_Now() {
  struct timeval tv;
  CHECK_EQ(0, gettimeofday(&tv, NULL));
  return static_cast<double>(tv.tv_sec) +
      static_cast<double>(tv.tv_usec) / 1000000;
}

TEST(StringHashTest, GetMask) {
  ASSERT_EQ(31ul, StringHash<int>::GetMask(16));
  ASSERT_EQ(31ul, StringHash<int>::GetMask(18));
  ASSERT_EQ(15ul, StringHash<int>::GetMask(15));
}

TEST(StringHashTest, Build) {
  std::map<std::string, int> m;
  m["hello"] = 9;
  m["1"] = 1;
  m["abc"] = 100;
  m["wew"] = 101;
  m["sd\0fff"] = 102;

  std::string buffer;
  ASSERT_TRUE(StringHash<int>::Build(m, &buffer));
  VLOG(0) << "Build OK";

  StringHash<int> table;
  ASSERT_TRUE(table.Attach(buffer.data(), buffer.length()));
  ASSERT_TRUE(table.IsAttached());
  VLOG(0) << "Attach OK";

  int value = 0;
  EXPECT_TRUE(table.Search("hello", &value));
  EXPECT_EQ(9, value);
  EXPECT_TRUE(table.Search("1", &value));
  EXPECT_EQ(1, value);
  EXPECT_TRUE(table.Search("abc", &value));
  EXPECT_EQ(100, value);
  EXPECT_TRUE(table.Search("wew", &value));
  EXPECT_EQ(101, value);
  EXPECT_TRUE(table.Search("sd\0fff", &value));
  EXPECT_EQ(102, value);
  EXPECT_FALSE(table.Search("jsdof", &value));
  VLOG(0) << "Search OK";
}

TEST(StringHashTest, BuildLarger) {
  std::map<std::string, uint32_t> m;
  std::vector<std::string> keys;
  const uint64_t kTotalKeyCount = 6000000;
  for (uint64_t i = 0; i < kTotalKeyCount; ++i) {
    std::string key = ConvertToString(i);
    keys.push_back(key);

    if (i >= kTotalKeyCount/2) {
      m[key] = static_cast<uint32_t>(i);
    }
  }

  std::string buffer;
  ASSERT_TRUE(StringHash<uint32_t>::Build(m, &buffer));
  VLOG(0) << "Build OK, size=[" << buffer.size() << "]";

  StringHash<uint32_t> table;
  ASSERT_TRUE(table.Attach(buffer.data(), buffer.length()));
  ASSERT_TRUE(table.IsAttached());
  VLOG(0) << "Attach OK";

  const StringHash<uint32_t>::Meta *meta = table.meta();
  VLOG(0) << "index_count=[" << meta->index_count << "]";
  VLOG(0) << "data_count=[" << meta->data_count << "]";

  double search_begin = WallTime_Now();
  // positive cases
  for (size_t i = kTotalKeyCount/2; i < kTotalKeyCount; ++i) {
    uint32_t value = 0;
    ASSERT_TRUE(table.Search(keys[i], &value));
    ASSERT_EQ(static_cast<uint32_t>(i), value);
  }
  double search_mid = WallTime_Now();
  // negative cases
  for (size_t i = 0; i < kTotalKeyCount/2; ++i) {
    uint32_t value = 0;
    ASSERT_FALSE(table.Search(keys[i], &value));
  }
  double search_end = WallTime_Now();
  VLOG(0) << "Search OK, time=[" << std::setprecision(6)
      << (search_mid - search_begin) << ","
      << (search_end - search_mid) << "]";
}
