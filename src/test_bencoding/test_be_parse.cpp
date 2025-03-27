#include <bencoding/be_element_ref.h>
#include <bencoding/be_element_ref_builders.h>
#include <bencoding/be_element_ref_parse.h>

#include <gtest/gtest.h>

using namespace be;

namespace
{
    class StringsListRefBuilder
    {
    public:
        StringsListRefBuilder& add(std::string_view str)
        {
            list_.add(StringRefBuilder().set(str).build_once({}));
            return *this;
        }

        ListRef build_once()
        {
            return std::move(*list_.build_once({}).as_list());
        }

    private:
        ListRefBuilder list_;
    };

    class StringsDictionaryRefBuilder
    {
    public:
        StringsDictionaryRefBuilder& add(std::string_view key,
            std::string_view value)
        {
            dict_.add(StringRef::Make(std::move(key)), StringRefBuilder().set(value).build_once({}));
            return *this;
        }

        DictionaryRef build_once()
        {
            return std::move(*dict_.build_once({}).as_dictionary());
        }

    private:
        DictionaryRefBuilder dict_;
    };
} // namespace

TEST(ElementRefParse, Empty_String_Is_UnexpectedEnd_Error)
{
    auto Parsed = be::Parse("");
    ASSERT_FALSE(Parsed);
    const ParseErrorInfo& error = Parsed.error();
    ASSERT_EQ(0u, error.position);
    ASSERT_EQ(ElementId::None, error.element);
    ASSERT_EQ(error.ec, ParseErrorc::UnexpectedEnd);
}

TEST(ElementRefParse, ZeroString_IsValid)
{
    auto Parsed = be::ParseString("0:");
    ASSERT_TRUE(Parsed);
    ASSERT_EQ(std::string_view(), Parsed.value());
}

TEST(ElementRefParse, String_Is_NonNegative_Number_Sepatated_With_Colon)
{
    auto Parsed = be::ParseString("3:str");
    ASSERT_TRUE(Parsed);
    ASSERT_EQ(std::string_view("str"), Parsed.value());
}

TEST(ElementRefParse, Too_Long_String_Fails_With_OutOfBound_Error)
{
    auto Parsed = be::ParseString("10:s");
    ASSERT_FALSE(Parsed);
    const ParseErrorInfo& error = Parsed.error();
    ASSERT_GT(error.position, 0u);
    ASSERT_EQ(ElementId::String, error.element);
    ASSERT_EQ(error.ec, ParseErrorc::StringOutOfBound);
}

TEST(ElementRefParse, Missing_Colon_For_String_Fails)
{
    auto Parsed = be::Parse("10");
    ASSERT_FALSE(Parsed);
    const ParseErrorInfo& error = Parsed.error();
    ASSERT_GT(error.position, 0u);
    ASSERT_EQ(ElementId::String, error.element);
    ASSERT_EQ(error.ec, ParseErrorc::MissingStringStart);
}

TEST(ElementRefParse, Strings_List)
{
    auto Parsed = be::ParseList("l4:spam4:eggse");
    const auto expected = StringsListRefBuilder()
        .add("spam")
        .add("eggs")
        .build_once();
    ASSERT_TRUE(Parsed);

    ASSERT_EQ(expected, Parsed.value());
}

TEST(ElementRefParse, Strings_Dictionary)
{
    auto Parsed = be::ParseDictionary("d3:cow3:moo4:spam4:eggse");
    ASSERT_TRUE(Parsed);
    const auto expected = StringsDictionaryRefBuilder()
        .add("cow", "moo")
        .add("spam", "eggs")
        .build_once();

    ASSERT_EQ(expected, Parsed.value());
}

TEST(ElementRefParse, Dictionary_With_List_Value)
{
    auto Parsed = be::ParseDictionary("d4:spaml1:a1:bee");
    ASSERT_TRUE(Parsed);
    const auto expected =
        *DictionaryRefBuilder()
            .add("spam", ListRefBuilder()
                .add(StringRefBuilder().set("a").build_once({}))
                .add(StringRefBuilder().set("b").build_once({}))
                .build_once({}))
            .build_once({})
            .as_dictionary();

    ASSERT_EQ(expected, Parsed.value());
}

TEST(ElementRefParse, Integers_With_Unary_Minus_Parsed_Only_Without_Leading_Zeroes)
{
    {
        auto Parsed = be::ParseInteger("i-0e");
        ASSERT_FALSE(Parsed);
        ASSERT_EQ(ParseErrorc::BadInteger, Parsed.error().ec);
        ASSERT_EQ(ElementId::Integer, Parsed.error().element);
    }

    {
        auto Parsed = be::ParseInteger("i-e");
        ASSERT_FALSE(Parsed);
        ASSERT_EQ(ParseErrorc::BadInteger, Parsed.error().ec);
        ASSERT_EQ(ElementId::Integer, Parsed.error().element);
    }

    {
        auto Parsed = be::ParseInteger("i-00000e");
        ASSERT_FALSE(Parsed);
        ASSERT_EQ(ParseErrorc::BadInteger, Parsed.error().ec);
        ASSERT_EQ(ElementId::Integer, Parsed.error().element);
    }
}

TEST(ElementRefParse, Only_Single_Zero_Integers_Are_Parsed)
{
    {
        auto Parsed = be::ParseInteger("i0e");
        ASSERT_TRUE(Parsed);
        ASSERT_EQ(std::string_view("0"), Parsed.value());
    }

    {
        auto Parsed = be::ParseInteger("i00e");
        ASSERT_FALSE(Parsed);
        ASSERT_EQ(ParseErrorc::BadInteger, Parsed.error().ec);
        ASSERT_EQ(ElementId::Integer, Parsed.error().element);
    }
}

TEST(ElementRefParse, Only_Negative_Integers_Parsed)
{
    auto bad = be::ParseInteger("i-0e");
    ASSERT_FALSE(bad.has_value());

    const char* k_integers[] = {"-3", "-123", "-1000000000"};

    auto encode = [](const char* i)
    {
        return std::string("i") + i + std::string("e");
    };

    for (const char* integer : k_integers)
    {
        auto encoded = encode(integer);
        auto Parsed = be::ParseInteger(encoded);
        ASSERT_TRUE(Parsed);
        ASSERT_EQ(std::string_view(integer), Parsed.value());
    }
}

TEST(ElementRefParse, Integers_With_Leading_Zeroes_Invalid)
{
    auto only_zero = be::ParseInteger("i0e");
    ASSERT_TRUE(only_zero);

    {
        auto Parsed = be::ParseInteger("i03e");
        ASSERT_FALSE(Parsed);
    }

    {
        auto Parsed = be::ParseInteger("i00e");
        ASSERT_FALSE(Parsed);
    }

    {
        auto Parsed = be::ParseInteger("i0099e");
        ASSERT_FALSE(Parsed);
    }
}

TEST(ElementRefParse, Only_Digits_Expected_Between_Integer_Start_And_End)
{
    const char* k_encoded[] =
    {
        "i-e",
        "i-10xfe",
        "i1111",
        "1111e",
        "",
    };
    for (const char* integer : k_encoded)
    {
        auto Parsed = be::ParseInteger(integer);
        ASSERT_FALSE(Parsed);
    }
}
