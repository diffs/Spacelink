//
// Created by Daniel on 2/26/2022.
//

#include "inc/filescanner.h"

// Constructor
FileScanner::FileScanner(string source, string target) {
    this->source = source;
    this->target = target;
}

unordered_map<string, pair<string, uintmax_t>> FileScanner::Scan() {
    // 1) Map all files in "source" directory
    // In the map, add full path (key), Pair<fileSize, inode> (value)
    unordered_map<string, pair<uintmax_t, ino_t>> files;
    for (const auto & entry : filesystem::recursive_directory_iterator(source)) {
        if(!entry.is_directory())
            files.insert(pair<string, pair<uintmax_t, ino_t>>(entry.path().filename().string(),
                pair<uintmax_t, ino_t>(entry.file_size(), getInode(entry.path().string().c_str()))));
    }

    // 1.5) Create a map to store our results
    unordered_map<string, pair<string, uintmax_t>> duplicates;

    // 2) Iterate over every file in the "target" / scan directory
    for (const auto & entry : filesystem::recursive_directory_iterator(target)) {
        // we don't need to check directories
        if(entry.is_directory())
            continue;
        // Get an iterator object
        auto file = files.find (entry.path().filename().string());
        // If the exact file name was found in the earlier "source" directory
        if(file != files.end()) {
            // Get the file size of this file
            uintmax_t compareSize = entry.file_size();

            // Get the inode of this file by calling getInode()
            int inode = getInode(entry.path().string().c_str());
            if(inode == 0) // error checking getInode()
                continue;

            // Iterate over every file that may have the same name in the "source" directory
            for (; file!=files.end(); ++file)
                // Check the file size to make sure it matches
                if(file->second.first == compareSize) {
                    // check and compare the inode of this file to see if it matches the inode from the file in the source directory
                    // if it doesn't match, it could be hard-linked to save on space since the file is likely identical
                    if(file->second.second != 0 && inode != file->second.second) {
                        cout << "Discrepancy: " << entry.path().string() << " vs " << file->first << " inodes: " << inode << " vs " << file->second.second << endl;

                        // Add to duplicates list
                        auto p = pair<string, uintmax_t>(entry.path().string(), entry.file_size());
                        duplicates.insert(pair<string, pair<string, uintmax_t>>(this->source + "/" + file->first, p));
                    }

                    // If we found a file that matched the name & size, we can stop looking
                    // We are not covering the edge case that multiple files with the same name + size in bytes exist
                    // The use case of this application (connecting a media directory to a downloads directory) makes this possibility unlikely
                    break;
                }
        }
    }

    return duplicates;
}

// Get the inode of a file using the stat function()
ino_t FileScanner::getInode(const char *path) {
    struct stat fileStat{};
    if(stat(path, &fileStat) == 0) {
        return fileStat.st_ino;
    }

    return 0;
}


// Prints the commands to remove all duplicate files and instead use hardlinks
// Returns the total amount of bytes that could be saved
uintmax_t FileScanner::PrintShellScript(const unordered_map<string, pair<string, uintmax_t>>& duplicates, const string& fileName) {
    uintmax_t totalSaved = 0;
    for(const auto &entry : duplicates) {
        printf("rm %s\n", entry.second.first.c_str());
        printf("ln '%s' '%s'\n", entry.first.c_str(), entry.second.first.c_str());

        totalSaved += entry.second.second;
    }

    return totalSaved;
}
