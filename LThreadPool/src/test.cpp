#include "modules/common/util/ctpl_stl.h"

#include <algorithm>
#include <atomic>
#include <iterator>
#include <set>
#include <sstream>
#include <string>

#include "gtest/gtest.h"

namespace apollo {
namespace common {
namespace util {

namespace {
// ...

// Attention: don't use overloaded functions, otherwise the compiler can't
// deduce the correct edition.
std::string filter_duplicates_str(int id, const char* str1, const char* str2,
                                  const char* str3, const char* str4) {
  // id is unused.

  std::stringstream ss_in;
  ss_in << str1 << " " << str2 << " " << str3 << " " << str4;

  std::set<std::string> string_set;
  std::istream_iterator<std::string> beg(ss_in);
  std::istream_iterator<std::string> end;
  std::copy(beg, end, std::inserter(string_set, string_set.end()));
  std::stringstream ss_out;
  std::copy(std::begin(string_set), std::end(string_set),
            std::ostream_iterator<std::string>(ss_out, " "));

  return ss_out.str();
}

std::string filter_duplicates(int id) {
  // id is unused.

  std::stringstream ss_in;
  ss_in
      << "a a b b b c foo foo bar foobar foobar hello world hello hello world";
  std::set<std::string> string_set;
  std::istream_iterator<std::string> beg(ss_in);
  std::istream_iterator<std::string> end;
  std::copy(beg, end, std::inserter(string_set, string_set.end()));

  std::stringstream ss_out;
  std::copy(std::begin(string_set), std::end(string_set),
            std::ostream_iterator<std::string>(ss_out, " "));

  return ss_out.str();
}

}  // namespace

TEST(ThreadPool, filter_duplicates) {
  const unsigned int hardware_threads = std::thread::hardware_concurrency();
  const unsigned int threads =
      std::min(hardware_threads != 0 ? hardware_threads : 2, 50U);
  ThreadPool p(threads);

  std::vector<std::future<std::string>> futures1, futures2;
  for (int i = 0; i < 1000; ++i) {
    futures1.push_back(std::move(p.Push(
        filter_duplicates_str, "thread pthread", "pthread thread good news",
        "today is a good day", "she is a six years old girl")));
    futures2.push_back(std::move(p.Push(filter_duplicates)));
  }

  for (int i = 0; i < 1000; ++i) {
    std::string result1 = futures1[i].get();
    std::string result2 = futures2[i].get();
    EXPECT_STREQ(
        result1.c_str(),
        "a day girl good is news old pthread she six thread today years ");
    EXPECT_STREQ(result2.c_str(), "a b bar c foo foobar hello world ");
  }
}

}  // namespace util
}  // namespace common
}  // namespace apollo