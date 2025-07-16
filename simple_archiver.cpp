// === simple_archiver.cpp ===
#include <iostream>  // For input/output operations (cout, cerr)
#include <fstream>   // For file stream operations (ifstream, ofstream)
#include <vector>    // For dynamic arrays (e.g., storing file content)
#include <string>    // For string manipulation
#include <cstdint>   // For fixed-width integer types (uint32_t, uint64_t)
#include <filesystem> // For directory traversal (C++17)
#include <map>       // For mapping items to their base paths

namespace fs = std::filesystem; // Alias for std::filesystem

// Function to write a string to an output file stream.
// It first writes the length of the string (as uint32_t), then the string data itself.
void writeString(std::ofstream& outFile, const std::string& str) {
    uint32_t len = str.length(); // Get the length of the string
    // Write the length (4 bytes)
    outFile.write(reinterpret_cast<const char*>(&len), sizeof(len));
    // Write the string data
    outFile.write(str.c_str(), len);
}

// Function to write binary data (from a vector of chars) to an output file stream.
// It first writes the size of the data (as uint64_t), then the data itself.
void writeBinaryData(std::ofstream& outFile, const std::vector<char>& data) {
    uint64_t size = data.size(); // Get the size of the binary data
    // Write the size (8 bytes)
    outFile.write(reinterpret_cast<const char*>(&size), sizeof(size));
    // Write the binary data
    outFile.write(data.data(), size);
}

// Function to archive a single file or an empty directory.
// It takes the output archive stream, the full path to the item, and the base path
// to calculate the relative path.
void archiveItem(std::ofstream& outputArchive, const fs::path& itemPath, const fs::path& basePath) {
    // Calculate the relative path of the item within the base directory.
    // This is crucial for recreating the directory structure during unarchiving.
    // Use fs::relative with the correct base path.
    fs::path relativePath = fs::relative(itemPath, basePath);
    
    // Ensure relativePath is not empty for the root item if basePath is its parent
    // If relativePath is ".", convert it to the item's name
    if (relativePath.empty() || relativePath == ".") {
        relativePath = itemPath.filename();
    }

    if (fs::is_regular_file(itemPath)) {
        // Handle regular files
        std::ifstream inputFile(itemPath, std::ios::binary | std::ios::ate); // Open in binary and at end for size
        if (!inputFile.is_open()) {
            std::cerr << "Warning: Could not open input file: " << itemPath << ". Skipping.\n";
            return;
        }

        uint64_t fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg); // Go back to the beginning of the file

        std::vector<char> fileContent(fileSize);
        if (fileSize > 0) {
            inputFile.read(fileContent.data(), fileSize);
            if (!inputFile) {
                std::cerr << "Warning: Error reading file: " << itemPath << ". Data might be incomplete. Skipping.\n";
                inputFile.close();
                return;
            }
        }
        inputFile.close();

        std::cout << "Archiving file: " << relativePath.string() << " (" << fileSize << " bytes)\n";
        writeString(outputArchive, relativePath.string()); // Write relative filename
        writeBinaryData(outputArchive, fileContent);       // Write file content
    } else if (fs::is_directory(itemPath)) {
        // Handle directories: write an empty content to signify a directory entry.
        // This is important for recreating empty directories or parent directories.
        std::cout << "Archiving directory: " << relativePath.string() << "\n";
        writeString(outputArchive, relativePath.string()); // Write relative directory path
        writeBinaryData(outputArchive, {}); // Write empty content for directories
    }
}

int main(int argc, char* argv[]) {
    // Usage: ./simple_archiver <output_archive_name> <input_path1> [input_path2 ...]
    // The output_archive_name will always have the .tzar extension.
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <output_archive_base_name> <input_file_or_directory1> [input_file_or_directory2 ...]\n";
        return 1;
    }

    // Get the base name from the first argument (e.g., "my_archive" from "my_archive" or "my_archive.zip")
    fs::path providedOutputPath(argv[1]);
    std::string outputArchiveName = providedOutputPath.stem().string() + ".tzar";
    
    // Vector to store paths of items that will actually be archived
    std::vector<fs::path> itemsToArchive;
    // Map to store the base path for each item, crucial for correct relative path calculation
    std::map<fs::path, fs::path> itemBasePaths;

    // First pass: Collect all valid files and directories to be archived
    for (int i = 2; i < argc; ++i) {
        fs::path inputPath = argv[i];
        
        if (!fs::exists(inputPath)) {
            std::cerr << "Warning: Input path does not exist: " << inputPath << ". Skipping.\n";
            continue;
        }

        // Determine the base path for relative path calculation for this top-level input.
        fs::path basePath;
        if (inputPath.has_parent_path()) {
            basePath = inputPath.parent_path();
        } else {
            basePath = fs::current_path();
        }
        basePath = fs::canonical(basePath); // Ensure basePath is canonical

        if (fs::is_regular_file(inputPath)) {
            itemsToArchive.push_back(inputPath);
            itemBasePaths[inputPath] = basePath;
        } else if (fs::is_directory(inputPath)) {
            itemsToArchive.push_back(inputPath); // Add the directory itself
            itemBasePaths[inputPath] = basePath;

            // Iterate recursively through the directory and add all its contents
            for (const auto& entry : fs::recursive_directory_iterator(inputPath)) {
                itemsToArchive.push_back(entry.path());
                itemBasePaths[entry.path()] = basePath; // All items in a dir share the same top-level basePath
            }
        } else {
            std::cerr << "Warning: Skipping unsupported item: " << inputPath << " (not a regular file or directory).\n";
        }
    }

    // If no valid items were found to archive, exit without creating the .tzar file
    if (itemsToArchive.empty()) {
        std::cout << "No valid files or directories found to archive. No .tzar file created.\n";
        return 0; // Exit successfully, but without creating an archive
    }

    // If there are items to archive, proceed to open the output file and write
    std::ofstream outputArchive(outputArchiveName, std::ios::binary);
    if (!outputArchive.is_open()) {
        std::cerr << "Error: Could not open output archive file: " << outputArchiveName << std::endl;
        return 1;
    }

    // Process each collected item and write it to the archive
    for (const auto& itemPath : itemsToArchive) {
        // Retrieve the correct basePath for this item from the map
        // Note: We need to ensure that itemPath exists as a key in itemBasePaths.
        // It should always exist if it was added to itemsToArchive.
        archiveItem(outputArchive, itemPath, itemBasePaths.at(itemPath));
    }

    outputArchive.close();
    std::cout << "Archiving complete. Archive saved to: " << outputArchiveName << std::endl;

    return 0;
}
