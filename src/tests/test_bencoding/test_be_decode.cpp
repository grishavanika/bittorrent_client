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
			list_.add(StringRefBuilder().set(str).build_once());
			return *this;
		}

		BEElementRef::List build_once()
		{
			return list_.build_once().as_list();
		}

	private:
		ListRefBuilder list_;
	};

	class StringsDictionaryRefBuilder
	{
	public:
		StringsDictionaryRefBuilder& add(nonstd::string_view key, nonstd::string_view value)
		{
			dict_.add(std::move(key), StringRefBuilder().set(value).build_once());
			return *this;
		}

		BEElementRef::Dictionary build_once()
		{
			return dict_.build_once().as_dictionary();
		}

	private:
		DictionaryRefBuilder dict_;
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

TEST(BEElementRefDecode, Missing_Colon_For_String_Fails)
{
	auto decoded = be::decode("10");
	ASSERT_FALSE(decoded);
	const DecodeError& error = decoded.error();
	ASSERT_GT(error.pos, 0u);
	ASSERT_EQ(ElementId::String, error.element);
	ASSERT_EQ(error.kind, DecodeErrorKind::MissingStringStart);
}

TEST(BEElementRefDecode, Strings_List)
{
	auto decoded = be::decode("l4:spam4:eggse");
	const auto expected = StringsListRefBuilder()
		.add("spam")
		.add("eggs")
		.build_once();
	ASSERT_TRUE(decoded);
	
	const std::vector<BEElementRef>& elements = *decoded;
	ASSERT_EQ(1u, elements.size());
	const auto& list_element = elements[0];
	ASSERT_TRUE(list_element.is_list());

	ASSERT_EQ(expected, list_element.as_list());
}

TEST(BEElementRefDecode, Strings_Dictionary)
{
	auto decoded = be::decode("d3:cow3:moo4:spam4:eggse");
	ASSERT_TRUE(decoded);
	const auto expected = StringsDictionaryRefBuilder()
		.add("cow", "moo")
		.add("spam", "eggs")
		.build_once();
	const std::vector<BEElementRef>& elements = *decoded;
	ASSERT_EQ(1u, elements.size());
	const auto& dict_element = elements[0];
	ASSERT_TRUE(dict_element.is_dictionary());

	ASSERT_EQ(expected, dict_element.as_dictionary());
}

TEST(BEElementRefDecode, Dictionary_With_List_Value)
{
	auto decoded = be::decode("d4:spaml1:a1:bee");
	ASSERT_TRUE(decoded);
	const auto expected = DictionaryRefBuilder()
		.add("spam"
			, ListRefBuilder()
				.add(StringRefBuilder().set("a").build_once())
				.add(StringRefBuilder().set("b").build_once())
				.build_once())
		.build_once()
		.as_dictionary();
	const std::vector<BEElementRef>& elements = *decoded;
	ASSERT_EQ(1u, elements.size());
	const auto& dict_element = elements[0];
	ASSERT_TRUE(dict_element.is_dictionary());

	ASSERT_EQ(expected, dict_element.as_dictionary());
}
