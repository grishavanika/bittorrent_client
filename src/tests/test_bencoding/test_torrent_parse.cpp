#include <bencoding/be_torrent_file_metainfo.h>
#include <bencoding/utils_read_file.h>
#include <gtest/gtest.h>

#include <TinySHA1.hpp>

using namespace be;

static std::string GetSHA1(const FileBuffer& buffer, ElementPosition position)
{
    const std::size_t length = (position.end_ - position.start_);
    const void* start = static_cast<const std::uint8_t*>(buffer.data_) + position.start_;
    sha1::SHA1 sha1;
    uint32_t digest[5];
    sha1.processBytes(start, length)
        .getDigest(digest);
    char tmp[50]{};
    (void)snprintf(tmp, sizeof(tmp), "%08x%08x%08x%08x%08x"
        , digest[0], digest[1], digest[2], digest[3], digest[4]);
    return tmp;
}

TEST(Torrent, NormalSingleFileTorrent)
{
    // #UUU: test on local data.
    const char k_file[] = R"(K:\debian-edu-10.6.0-amd64-netinst.iso.torrent)";
    const FileBuffer buffer = ReadAllFileAsBinary(k_file);
    ASSERT_NE(buffer.data_, nullptr);
    const std::string_view content(static_cast<const char*>(buffer.data_), buffer.size_);
    std::optional<ParseTorrentFileResult> result = ParseTorrentFileContent(content);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ("39e063338f06804d40cf1907b123b3a23d3cfd77"
        , GetSHA1(buffer, result->info_position_));
}

TEST(Torrent, NormalMultiFileTorrent)
{
    // #UUU: test on local data.
    const char k_file[] = R"(K:\32F6DBF1412D24A370C12CC90D289AFFB4806284.torrent)";
    const FileBuffer buffer = ReadAllFileAsBinary(k_file);
    ASSERT_NE(buffer.data_, nullptr);
    const std::string_view content(static_cast<const char*>(buffer.data_), buffer.size_);
    std::optional<ParseTorrentFileResult> result = ParseTorrentFileContent(content);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ("32f6dbf1412d24a370c12cc90d289affb4806284"
        , GetSHA1(buffer, result->info_position_));
}
