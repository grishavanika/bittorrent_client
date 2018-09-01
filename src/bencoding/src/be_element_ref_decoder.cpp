#include <bencoding/be_element_ref_decoder.h>
#include <bencoding/be_element_ref_builders.h>

#include <charconv>

#include <cassert>

namespace be
{
	namespace
	{
		constexpr char k_integer_start     = 'i';
		constexpr char k_list_start        = 'l';
		constexpr char k_dictionary_start  = 'd';
		constexpr char k_element_end       = 'e';
		constexpr char k_string_start      = ':';

		using DecodedBEElement = nonstd::expected<BEElementRef, DecodeError>;

		struct Decoder
		{
			Decoder(const char* start, std::size_t length)
				: current_(start)
				, end_(start + length)
				, start_(start)
			{
			}

			DecodedBEElement decode_element(ElementId parent_id = ElementId::None)
			{
				if (current_ >= end_)
				{
					return make_error(parent_id, DecodeErrorKind::UnexpectedEnd);
				}
				switch (*current_)
				{
				case k_integer_start    : return decode_integer();
				case k_list_start       : return decode_list();
				case k_dictionary_start : return decode_dictionary();
				default                 : return decode_string();
				}
			}

			bool has_data() const
			{
				return (current_ < end_);
			}

		private:
			DecodedBEElement make_error(ElementId element, DecodeErrorKind kind) const
			{
				DecodeError error;
				error.pos = (current_ - start_);
				error.element = element;
				error.kind = kind;
				return nonstd::make_unexpected(std::move(error));
			}

			void consume(char ch)
			{
				assert(has_data());
				assert(*current_ == ch);
				(void)ch;
				++current_;
			}

			void consume(std::size_t count)
			{
				assert((start_ + count) < end_);
				current_ += count;
			}

			DecodedBEElement decode_integer()
			{
				consume(k_integer_start);
				return make_error(ElementId::String, DecodeErrorKind::Unknown);
			}

			DecodedBEElement decode_list()
			{
				consume(k_list_start);
				ListRefBuilder builder;
				while (has_data() && (*current_ != k_element_end))
				{
					auto element = decode_element(ElementId::List);
					if (!element)
					{
						return element;
					}
					builder.add(std::move(*element));
				}
				consume(k_element_end);
				return builder.build_once();
			}

			DecodedBEElement decode_dictionary()
			{
				consume(k_dictionary_start);
				DictionaryRefBuilder builder;
				while (has_data() && (*current_ != k_element_end))
				{
					auto key = decode_element(ElementId::Dictionary);
					if (!key)
					{
						return key;
					}
					if (key->element_id() != ElementId::String)
					{
						return make_error(ElementId::Dictionary
							, DecodeErrorKind::NonStringAsDictionaryKey);
					}
					auto value = decode_element(ElementId::Dictionary);
					if (!value)
					{
						return value;
					}
					builder.add(std::move(key->as_string()), std::move(*value));
				}
				consume(k_element_end);
				return builder.build_once();
			}

			DecodedBEElement decode_string()
			{
				assert(has_data());
				auto length_element = decode_string_length();
				if (!length_element)
				{
					return length_element;
				}

				const auto& length_str = length_element->as_number();
				const char* length_end = length_str.data() + length_str.size();
				std::size_t length = 0;
				const auto result = std::from_chars(length_str.data()
					, length_end, length, 10);
				if ((result.ptr != length_end) || (result.ec != std::errc()))
				{
					return make_error(ElementId::String
						, DecodeErrorKind::BadStringLength);
				}

				if ((current_ + length) > end_)
				{
					return make_error(ElementId::String
						, DecodeErrorKind::StringOutOfBound);
				}
				const char* begin = current_;
				consume(length);
				return StringRefBuilder()
					.set(nonstd::string_view(begin, length))
					.build_once();
			}

			DecodedBEElement decode_string_length()
			{
				assert(has_data());
				const char* begin = current_;
				do 
				{
					if (std::isdigit(*current_))
					{
						++current_;
						continue;
					}
					else if (*current_ == k_string_start)
					{
						break;
					}
					else
					{
						return make_error(ElementId::String
							, DecodeErrorKind::UnexpectedStringLength);
					}
				}
				while (has_data());

				if (begin == current_)
				{
					// Missing number's digits: ":str"
					return make_error(ElementId::String
						, DecodeErrorKind::UnexpectedStringLength);
				}

				NumberRefBuilder builder;
				builder.set(nonstd::string_view(begin, current_ - begin));
				consume(k_string_start);
				return builder.build_once();
			}

		private:
			const char* current_;
			const char* end_;
			const char* start_;
		};
	} // namespace

	Decoded decode(nonstd::string_view bencoded)
	{
		Decoder decoder(bencoded.data(), bencoded.size());

		std::vector<BEElementRef> elements;
		do
		{
			auto element = decoder.decode_element();
			if (element)
			{
				elements.push_back(std::move(*element));
				continue;
			}
			return nonstd::make_unexpected(std::move(element).error());
		}
		while (decoder.has_data());

		return elements;
	}

} // namespace be

