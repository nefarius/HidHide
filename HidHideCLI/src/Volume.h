// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Volume.h
#pragma once

namespace HidHide
{
    typedef std::filesystem::path FullImageName;
    typedef std::set<FullImageName> FullImageNames;

    // Determine the full image name for storage of the file specified while considering mounted folder structures
    // Return an empty path when the specified file name can't be stored on any of the volumes present
    FullImageName FileNameToFullImageName(_In_ std::filesystem::path const& logicalFileName);

    // Determine the file name associated with a full image name
    // Return an empty path when the logical file name couldn't be located on any of the volumes present
    std::filesystem::path FullImageNameToFileName(_In_ FullImageName const& fullImageName);
}
