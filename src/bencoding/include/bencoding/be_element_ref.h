#pragma once
#include <vector>
#include <map>
#include <utility>

#include <nonstd/variant.hpp>
#include <nonstd/string_view.hpp>
#include <nonstd/expected.hpp>

#include <cctype>

namespace be
{

	class BEElementRef
	{
	public:
		using Number   = nonstd::string_view;
		using String   = nonstd::string_view;
		using List     = std::vector<BEElementRef>;
		using Map      = std::vector<std::pair<String, BEElementRef>>;
		using Storage  = nonstd::variant<Number, String, List, Map>;

	public:

	private:
		Storage storage_;
	};

	nonstd::expected<std::vector<BEElementRef>, int/*dummy*/>
		decode(nonstd::string_view bencoded);

} // namespace be
