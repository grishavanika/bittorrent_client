#pragma once
#include <bencoding/be_element_ref.h>

namespace be
{

	enum class DecodeErrorKind
	{
		Unknown,
		UnexpectedEnd,
		UnexpectedStringLength,
		BadStringLength,
		StringOutOfBound,
		NonStringAsDictionaryKey,
		MissingListStart,
		MissingListEnd,
		MissingDictionaryStart,
		MissingDictionaryEnd,
		MissingIntegerStart,
		MissingIntegerEnd,
		MissingStringStart,
	};

	struct DecodeError
	{
		std::size_t pos = 0u;
		ElementId element = ElementId::None;
		DecodeErrorKind kind = DecodeErrorKind::Unknown;
	};

	using Decoded = nonstd::expected<
		std::vector<BEElementRef>,
		DecodeError>;

	Decoded decode(nonstd::string_view bencoded);

} // namespace be
