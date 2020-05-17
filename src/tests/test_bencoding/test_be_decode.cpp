#include <bencoding/be_element_ref.h>
#include <bencoding/be_element_ref_builders.h>
#include <bencoding/be_element_ref_decoder.h>
#include <gtest/gtest.h>

using namespace be;

namespace
{
    class StringsListRefBuilder
    {
    public:
        StringsListRefBuilder& add(std::string_view str)
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
        StringsDictionaryRefBuilder& add(std::string_view key,
            std::string_view value)
        {
            dict_.add(
                std::move(key), StringRefBuilder().set(value).build_once());
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
    auto decoded = be::Decode("");
    ASSERT_FALSE(decoded);
    const DecodeError& error = decoded.error();
    ASSERT_EQ(0u, error.pos);
    ASSERT_EQ(ElementId::None, error.element);
    ASSERT_EQ(error.kind, DecodeErrorKind::UnexpectedEnd);
}

TEST(BEElementRefDecode, ZeroString_IsValid)
{
    auto decoded = be::DecodeString("0:");
    ASSERT_TRUE(decoded);
    ASSERT_EQ(std::string_view(), *decoded);
}

TEST(BEElementRefDecode, String_Is_NonNegative_Number_Sepatated_With_Colon)
{
    auto decoded = be::DecodeString("3:str");
    ASSERT_TRUE(decoded);
    ASSERT_EQ(std::string_view("str"), *decoded);
}

TEST(BEElementRefDecode, Too_Long_String_Fails_With_OutOfBound_Error)
{
    auto decoded = be::DecodeString("10:s");
    ASSERT_FALSE(decoded);
    const DecodeError& error = decoded.error();
    ASSERT_GT(error.pos, 0u);
    ASSERT_EQ(ElementId::String, error.element);
    ASSERT_EQ(error.kind, DecodeErrorKind::StringOutOfBound);
}

TEST(BEElementRefDecode, Missing_Colon_For_String_Fails)
{
    auto decoded = be::Decode("10");
    ASSERT_FALSE(decoded);
    const DecodeError& error = decoded.error();
    ASSERT_GT(error.pos, 0u);
    ASSERT_EQ(ElementId::String, error.element);
    ASSERT_EQ(error.kind, DecodeErrorKind::MissingStringStart);
}

TEST(BEElementRefDecode, Strings_List)
{
    auto decoded = be::DecodeList("l4:spam4:eggse");
    const auto expected =
        StringsListRefBuilder().add("spam").add("eggs").build_once();
    ASSERT_TRUE(decoded);

    ASSERT_EQ(expected, *decoded);
}

TEST(BEElementRefDecode, Strings_Dictionary)
{
    auto decoded = be::DecodeDictionary("d3:cow3:moo4:spam4:eggse");
    ASSERT_TRUE(decoded);
    const auto expected = StringsDictionaryRefBuilder()
                              .add("cow", "moo")
                              .add("spam", "eggs")
                              .build_once();

    ASSERT_EQ(expected, *decoded);
}

TEST(BEElementRefDecode, Dictionary_With_List_Value)
{
    auto decoded = be::DecodeDictionary("d4:spaml1:a1:bee");
    ASSERT_TRUE(decoded);
    const auto expected =
        DictionaryRefBuilder()
            .add("spam",
                ListRefBuilder()
                    .add(StringRefBuilder().set("a").build_once())
                    .add(StringRefBuilder().set("b").build_once())
                    .build_once())
            .build_once()
            .as_dictionary();

    ASSERT_EQ(expected, *decoded);
}

TEST(BEElementRefDecode,
    Integers_With_Unary_Minus_Parsed_Only_Without_Leading_Zeroes)
{
    {
        auto decoded = be::DecodeInteger("i-0e");
        ASSERT_FALSE(decoded);
        ASSERT_EQ(DecodeErrorKind::BadInteger, decoded.error().kind);
        ASSERT_EQ(ElementId::Integer, decoded.error().element);
    }

    {
        auto decoded = be::DecodeInteger("i-e");
        ASSERT_FALSE(decoded);
        ASSERT_EQ(DecodeErrorKind::BadInteger, decoded.error().kind);
        ASSERT_EQ(ElementId::Integer, decoded.error().element);
    }

    {
        auto decoded = be::DecodeInteger("i-00000e");
        ASSERT_FALSE(decoded);
        ASSERT_EQ(DecodeErrorKind::BadInteger, decoded.error().kind);
        ASSERT_EQ(ElementId::Integer, decoded.error().element);
    }
}

TEST(BEElementRefDecode, Only_Single_Zero_Integers_Are_Parsed)
{
    {
        auto decoded = be::DecodeInteger("i0e");
        ASSERT_TRUE(decoded);
        ASSERT_EQ(std::string_view("0"), *decoded);
    }

    {
        auto decoded = be::DecodeInteger("i00e");
        ASSERT_FALSE(decoded);
        ASSERT_EQ(DecodeErrorKind::BadInteger, decoded.error().kind);
        ASSERT_EQ(ElementId::Integer, decoded.error().element);
    }
}

TEST(BEElementRefDecode, Only_Negative_Integers_Parsed)
{
    auto bad = be::DecodeInteger("i-0e");
    ASSERT_FALSE(bad.has_value());

    const char* k_integers[] = {"-3", "-123", "-1000000000"};

    auto encode = [](const char* i) {
        return std::string("i") + i + std::string("e");
    };

    for (const char* integer : k_integers)
    {
        auto encoded = encode(integer);
        auto decoded = be::DecodeInteger(encoded);
        ASSERT_TRUE(decoded);
        ASSERT_EQ(std::string_view(integer), *decoded);
    }
}

TEST(BEElementRefDecode, Integers_With_Leading_Zeroes_Invalid)
{
    auto only_zero = be::DecodeInteger("i0e");
    ASSERT_TRUE(only_zero);

    {
        auto decoded = be::DecodeInteger("i03e");
        ASSERT_FALSE(decoded);
    }

    {
        auto decoded = be::DecodeInteger("i00e");
        ASSERT_FALSE(decoded);
    }

    {
        auto decoded = be::DecodeInteger("i0099e");
        ASSERT_FALSE(decoded);
    }
}

TEST(BEElementRefDecode, Only_Digits_Expected_Between_Integer_Start_And_End)
{
    const char* k_encoded[] = {
        "i-e",
        "i-10xfe",
        "i1111",
        "1111e",
        "",
    };
    for (const char* integer : k_encoded)
    {
        auto decoded = be::DecodeInteger(integer);
        ASSERT_FALSE(decoded);
    }
}
