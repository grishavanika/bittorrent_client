#include <gtest/gtest.h>
#include <bencoding/be_element_ref.h>

TEST(BEElementRef, Dummy_Parse)
{
	auto decoded = be::decode("");
	ASSERT_TRUE(decoded);
	ASSERT_EQ(0u, decoded->size());
}
