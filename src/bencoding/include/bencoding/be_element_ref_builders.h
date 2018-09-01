#pragma once
#include <bencoding/be_element_ref.h>

namespace be
{

	class ListRefBuilder
	{
	public:
		ListRefBuilder& add(BEElementRef&& element)
		{
			list_.push_back(std::move(element));
			return *this;
		}

		BEElementRef build_once()
		{
			return BEElementRef(BEElementRef::Storage(
				nonstd::in_place_index_t<2>(), std::move(std::move(list_))));
		}

	private:
		BEElementRef::List list_;
	};

	class DictionaryRefBuilder
	{
	public:
		DictionaryRefBuilder& add(BEElementRef::String&& key, BEElementRef&& value)
		{
			dict_.emplace_back(std::piecewise_construct
				, std::make_tuple(std::move(key))
				, std::make_tuple(std::move(value)));
			return *this;
		}

		BEElementRef build_once()
		{
			return BEElementRef(BEElementRef::Storage(
				nonstd::in_place_index_t<3>(), std::move(std::move(dict_))));
		}

	private:
		BEElementRef::Dictionary dict_;
	};

	class NumberRefBuilder
	{
	public:
		NumberRefBuilder& set(nonstd::string_view number)
		{
			number_ = std::move(number);
			return *this;
		}

		BEElementRef build_once()
		{
			return BEElementRef(BEElementRef::Storage(
				nonstd::in_place_index_t<0>(), std::move(std::move(number_))));
		}

	private:
		nonstd::string_view number_;
	};

	class StringRefBuilder
	{
	public:
		StringRefBuilder& set(nonstd::string_view str)
		{
			str_ = std::move(str);
			return *this;
		}

		BEElementRef build_once()
		{
			return BEElementRef(BEElementRef::Storage(
				nonstd::in_place_index_t<1>(), std::move(std::move(str_))));
		}

	private:
		nonstd::string_view str_;
	};

} // namespace be

