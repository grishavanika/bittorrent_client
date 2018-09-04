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

			DecodedBEElement decode_integer()
			{
				if (!consume(k_integer_start))
				{
					return make_error(ElementId::Integer
						, DecodeErrorKind::MissingIntegerStart);
				}
				const char* const begin = current_;
				const bool sign = (has_data() && (*begin == '-'));
				if (sign)
				{
					++current_;
				}

				while (has_data() && (*current_ != k_element_end))
				{
					if (std::isdigit(*current_))
					{
						++current_;
					}
					else
					{
						return make_error(ElementId::Integer
							, DecodeErrorKind::BadInteger);
					}
				}

				const char* const last = current_;
				if (!consume(k_element_end))
				{
					return make_error(ElementId::Integer
						, DecodeErrorKind::BadInteger);
				}

				if (begin == last)
				{
					// Missing number's digits: "ie"
					return make_error(ElementId::Integer
						, DecodeErrorKind::BadInteger);
				}
				else if (sign && (last == (begin + 1)))
				{
					// Only `-` was specified: "i-e"
					return make_error(ElementId::Integer
						, DecodeErrorKind::BadInteger);
				}
				else if (sign && *(begin + 1) == '0')
				{
					// "i-0e" case
					return make_error(ElementId::Integer
						, DecodeErrorKind::BadInteger);
				}
				else if ((*begin == '0') && (last > (begin + 1)))
				{
					// "i03e" case
					return make_error(ElementId::Integer
						, DecodeErrorKind::BadInteger);
				}
				return IntegerRefBuilder()
					.set(nonstd::string_view(begin, last - begin))
					.build_once();
			}

			DecodedBEElement decode_list()
			{
				if (!consume(k_list_start))
				{
					return make_error(ElementId::String
						, DecodeErrorKind::MissingListStart);
				}
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
				if (!consume(k_element_end))
				{
					return make_error(ElementId::String
						, DecodeErrorKind::MissingListEnd);
				}
				return builder.build_once();
			}

			DecodedBEElement decode_dictionary()
			{
				if (!consume(k_dictionary_start))
				{
					return make_error(ElementId::Dictionary
						, DecodeErrorKind::MissingDictionaryStart);
				}
				DictionaryRefBuilder builder;
				while (has_data() && (*current_ != k_element_end))
				{
					auto key = decode_element(ElementId::Dictionary);
					if (!key)
					{
						return key;
					}
					if (!key->is_string())
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
				if (!consume(k_element_end))
				{
					return make_error(ElementId::Dictionary
						, DecodeErrorKind::MissingDictionaryEnd);
				}
				return builder.build_once();
			}

			DecodedBEElement decode_string()
			{
				auto length_element = decode_string_length();
				if (!length_element)
				{
					return length_element;
				}

				const auto& length_str = length_element->as_integer();
				const char* length_end = length_str.data() + length_str.size();
				std::size_t length = 0;
				const auto result = std::from_chars(length_str.data()
					, length_end, length, 10);
				if ((result.ptr != length_end) || (result.ec != std::errc()))
				{
					return make_error(ElementId::String
						, DecodeErrorKind::BadStringLength);
				}

				const char* const begin = current_;
				if (!consume(length))
				{
					return make_error(ElementId::String
						, DecodeErrorKind::StringOutOfBound);
				}
				return StringRefBuilder()
					.set(nonstd::string_view(begin, length))
					.build_once();
			}

			[[nodiscard]] bool has_data() const
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

			[[nodiscard]] bool consume(char ch)
			{
				if (has_data() && (*current_ == ch))
				{
					++current_;
					return true;
				}
				return false;
			}

			[[nodiscard]] bool consume(std::size_t count)
			{
				if ((start_ + count) < end_)
				{
					current_ += count;
					return true;
				}
				return false;
			}

			DecodedBEElement decode_string_length()
			{
				const char* begin = current_;
				while (has_data())
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

				if (begin == current_)
				{
					// Missing number's digits: ":str" or empty string ""
					return make_error(ElementId::String
						, DecodeErrorKind::UnexpectedStringLength);
				}

				if (!consume(k_string_start))
				{
					return make_error(ElementId::String
						, DecodeErrorKind::MissingStringStart);
				}

				return IntegerRefBuilder()
					.set(nonstd::string_view(begin, current_ - begin - 1))
					.build_once();
			}

		private:
			const char* current_;
			const char* end_;
			const char* start_;
		};
	} // namespace

	Decoded<std::vector<BEElementRef>> Decode(nonstd::string_view bencoded)
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

	Decoded<BEElementRef::String> DecodeString(nonstd::string_view bencoded)
	{
		Decoder decoder(bencoded.data(), bencoded.size());
		auto element = decoder.decode_string();
		if (element)
		{
			return std::move(element->as_string());
		}
		return nonstd::make_unexpected(element.error());
	}

	Decoded<BEElementRef::Integer> DecodeInteger(nonstd::string_view bencoded)
	{
		Decoder decoder(bencoded.data(), bencoded.size());
		auto element = decoder.decode_integer();
		if (element)
		{
			return std::move(element->as_integer());
		}
		return nonstd::make_unexpected(element.error());
	}

	Decoded<BEElementRef::List> DecodeList(nonstd::string_view bencoded)
	{
		Decoder decoder(bencoded.data(), bencoded.size());
		auto element = decoder.decode_list();
		if (element)
		{
			return std::move(element->as_list());
		}
		return nonstd::make_unexpected(element.error());
	}

	Decoded<BEElementRef::Dictionary> DecodeDictionary(nonstd::string_view bencoded)
	{
		Decoder decoder(bencoded.data(), bencoded.size());
		auto element = decoder.decode_dictionary();
		if (element)
		{
			return std::move(element->as_dictionary());
		}
		return nonstd::make_unexpected(element.error());
	}


} // namespace be

