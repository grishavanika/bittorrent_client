#include <bencoding/be_torrent_file_metainfo.h>
#include <bencoding/utils_read_file.h>
#include <gtest/gtest.h>

using namespace be;

TEST(Torrent, NormalSingleFileTorrent)
{
    std::optional<TorrentFile> metainfo;
    {
        // #UUU: test on local data.
        const char k_file[] = R"(K:\debian-edu-10.6.0-amd64-netinst.iso.torrent)";
        const FileBuffer buffer = ReadAllFileAsBinary(k_file);
        ASSERT_NE(buffer.data_, nullptr);
        const std::string_view content(static_cast<const char*>(buffer.data_), buffer.size_);
        metainfo = ParseTorrentFileContent(content);
    }
    ASSERT_TRUE(metainfo.has_value());
}

TEST(Torrent, NormalMultiFileTorrent)
{
    std::optional<TorrentFile> metainfo;
    {
        // #UUU: test on local data.
        const char k_file[] = R"(K:\32F6DBF1412D24A370C12CC90D289AFFB4806284.torrent)";
        const FileBuffer buffer = ReadAllFileAsBinary(k_file);
        ASSERT_NE(buffer.data_, nullptr);
        const std::string_view content(static_cast<const char*>(buffer.data_), buffer.size_);
        metainfo = ParseTorrentFileContent(content);
    }
    ASSERT_TRUE(metainfo.has_value());
}
