#include <bencoding/be_element_ref.h>

namespace be
{

	nonstd::expected<std::vector<BEElementRef>, int/*dummy*/>
		decode(nonstd::string_view bencoded)
	{
		(void)bencoded;
		return std::vector<BEElementRef>();
	}

} // namespace be

