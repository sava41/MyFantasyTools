#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace mft
{

    bool write_binary( const std::filesystem::path& binPath, const std::vector<char>& buffer );

    std::vector<char> read_binary( const std::filesystem::path& binPath );

    // -------------------------------------------------------------------------
    // .mflevel binary format
    //
    // Layout:
    //   [0..3]    uint32 LE — byte length of the FlatBuffer section (size prefix)
    //   [4..N+3]  FlatBuffer bytes — "MFLV" file_identifier at bytes [8..11]
    //   [N+4..]   Image blob (JXL files concatenated; offsets stored in FlatBuffer)
    //
    // Schema evolution is handled by FlatBuffers vtables: fields absent in an
    // older file simply return their default values (null / 0) in new readers.
    // -------------------------------------------------------------------------

    inline constexpr char MFLEVEL_MAGIC[] = "MFLV";

    // Read the header and FlatBuffer section of a .mflevel file.
    // On success flatbuffer_out contains the FlatBuffer bytes and blob_start_out
    // is set to the byte offset within the file where the image blob begins.
    // Images are NOT loaded into memory — callers seek to
    //   blob_start_out + entry->offset()  and read  entry->size()  bytes on demand.
    bool read_mflevel( const std::filesystem::path& path,
                       std::vector<char>&           flatbuffer_out,
                       size_t&                      blob_start_out );

} // namespace mft