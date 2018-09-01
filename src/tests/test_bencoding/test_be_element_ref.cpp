#include <gtest/gtest.h>
#include <bencoding/be_element_ref.h>
#include <bencoding/be_element_ref_decoder.h>
#include <bencoding/be_element_ref_builders.h>

using namespace be;

namespace
{
	class StringsListRefBuilder
	{
	public:
		StringsListRefBuilder& add(nonstd::string_view str)
		{
			list_.add(StringRefBuilder().set(std::move(str)).build_once());
			return *this;
		}

		BEElementRef::List build_once()
		{
			return list_.build_once().as_list();
		}

	private:
		ListRefBuilder list_;
	};
} // namespace

TEST(BEElementRefDecode, Empty_String_Is_UnexpectedEnd_Error)
{
	auto decoded = be::decode("");
	ASSERT_FALSE(decoded);
	const DecodeError& error = decoded.error();
	ASSERT_EQ(0u, error.pos);
	ASSERT_EQ(ElementId::None, error.element);
	ASSERT_EQ(error.kind, DecodeErrorKind::UnexpectedEnd);
}

TEST(BEElementRefDecode, ZeroString_IsValid)
{
	auto decoded = be::decode("0:");
	ASSERT_TRUE(decoded);
	const std::vector<BEElementRef>& elements = *decoded;
	ASSERT_EQ(1u, elements.size());
	const BEElementRef& str_element = elements[0];
	ASSERT_TRUE(str_element.is_string());
	ASSERT_EQ(nonstd::string_view(), str_element.as_string());
}

TEST(BEElementRefDecode, String_Is_NonNegative_Number_Sepatated_With_Colon)
{
	auto decoded = be::decode("3:str");
	ASSERT_TRUE(decoded);
	const std::vector<BEElementRef>& elements = *decoded;
	ASSERT_EQ(1u, elements.size());
	const BEElementRef& str_element = elements[0];
	ASSERT_TRUE(str_element.is_string());
	ASSERT_EQ(nonstd::string_view("str"), str_element.as_string());
}

TEST(BEElementRefDecode, Too_Long_String_Fails_With_OutOfBound_Error)
{
	auto decoded = be::decode("10:s");
	ASSERT_FALSE(decoded);
	const DecodeError& error = decoded.error();
	ASSERT_GT(error.pos, 0u);
	ASSERT_EQ(ElementId::String, error.element);
	ASSERT_EQ(error.kind, DecodeErrorKind::StringOutOfBound);
}

TEST(BEElementRefDecode, Strings_List)
{
	auto decoded = be::decode("l4:spam4:eggse");
	const auto expected = StringsListRefBuilder()
		.add("spam").add("eggs")
		.build_once();
	ASSERT_TRUE(decoded);
	
	const std::vector<BEElementRef>& elements = *decoded;
	ASSERT_EQ(1u, elements.size());
	ASSERT_TRUE(elements[0].is_list());

	const auto strs = elements[0].as_list();

	ASSERT_EQ(expected, strs);
}

TEST(BEElementRefDecode, Strings_Dictionary)
{
	auto decoded = be::decode("d3:cow3:moo4:spam4:eggse");
	ASSERT_TRUE(decoded);
	// ...
}
