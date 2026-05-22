#include "src/weld.h"
#include "gtest/gtest.h"
#include <gtest/gtest.h>
#include "common.h"

template<typename E>
struct InputSectionTest : public ::testing::Test {
  using impl_class = weld::InputSection<E>;
};

TYPED_TEST_SUITE(InputSectionTest, arch_list);

TYPED_TEST(InputSectionTest, TestScanRelocation) {
  typename TestFixture::impl_class sec;
};
