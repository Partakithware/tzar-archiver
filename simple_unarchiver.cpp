// === simple_unarchiver.cpp ===
#include <iostream>  // For input/output operations (cout, cerr)
#include <fstream>   // For file stream operations (ifstream, ofstream)
#include <vector>    // For dynamic arrays (e.g., storing file content)
#include <string>    // For string manipulation
#include <cstdint>   // For fixed-width integer types (uint32_t, uint64_t)
#include <filesystem> // For directory creation (C++17)
#include <stdexcept> // For std::runtime_error
#include <set>       // For efficient lookup of files to extract

namespace fs = std::filesystem; // Alias for std::filesystem

// Function to read a string from an input file stream.
// It first reads the length (as uint32_t), then reads that many characters to form the string.
std::string readString(std::ifstream& inFile) {
    uint32_t len;
    // Read the length (4 bytes)
    inFile.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (!inFile) { // Check for read error or EOF
        throw std::runtime_error("Error reading string length from archive.");
    }
    
    std::vector<char> buffer(len); // Create a buffer for the string data
    // Read the string data
    inFile.read(buffer.data(), len);
    if (!inFile) { // Check for read error or EOF
        throw std::runtime_error("Error reading string data from archive.");
    }
    return std::string(buffer.begin(), buffer.end()); // Construct string from buffer
}

// Function to read binary data (into a vector of chars) from an input file stream.
// It first reads the size (as uint64_t). If 'read_content' is true, it reads the data
// into a vector. Otherwise, it just skips the data.
std::vector<char> readBinaryData(std::ifstream& inFile, bool read_content = true) {
    uint64_t size;
    // Read the size (8 bytes)
    inFile.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (!inFile) { // Check for read error or EOF
        throw std::runtime_error("Error reading binary data size from archive.");
    }

    std::vector<char> data;
    if (read_content) {
        data.resize(size); // Resize vector to hold the binary data
        if (size > 0) { // Only read if there's data to read
            // Read the binary data
            inFile.read(data.data(), size);
            if (!inFile) { // Check for read error or EOF
                throw std::runtime_error("Error reading binary data from archive.");
            }
        }
    } else {
        // If not reading content, just skip the bytes
        inFile.seekg(size, std::ios_base::cur);
        if (!inFile) {
            throw std::runtime_error("Error skipping binary data content in archive.");
        }
    }
    return data; // Return the vector (empty if content was skipped)
}

int main(int argc, char* argv[]) {
    // Usage: ./simple_unarchiver <input_archive_name> [file_to_extract1] [file_to_extract2 ...]
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_archive_name> [file_to_extract1] [file_to_extract2 ...]\n";
        return 1;
    }

    std::string inputArchiveName = argv[1];
    std::ifstream inputArchive(inputArchiveName, std::ios::binary);
    if (!inputArchive.is_open()) {
        std::cerr << "Error: Could not open input archive file: " << inputArchiveName << std::endl;
        return 1;
    }

    // Collect paths of files to extract if provided
    std::set<std::string> files_to_extract;
    bool extract_all = true;
    if (argc > 2) {
        extract_all = false;
        for (int i = 2; i < argc; ++i) {
            files_to_extract.insert(argv[i]);
        }
    }

    // Use a try-catch block to handle potential errors during reading (e.g., corrupted archive).
    try {
        int extracted_count = 0;
        int skipped_count = 0;

        // Loop to read files until the end of the archive is reached.
        while (inputArchive.peek() != EOF) {
            std::string relativePathStr = readString(inputArchive); // Read relative path

            bool should_extract_current_item = extract_all || files_to_extract.count(relativePathStr);
            
            std::vector<char> fileContent;
            if (should_extract_current_item) {
                fileContent = readBinaryData(inputArchive, true); // Read content
            } else {
                readBinaryData(inputArchive, false); // Skip content
            }

            if (should_extract_current_item) {
                fs::path outputPath = relativePathStr; // Convert string to filesystem path

                // Create parent directories if they don't exist, for both files and directories
                if (outputPath.has_parent_path()) {
                    fs::create_directories(outputPath.parent_path());
                }

                // Handle directory entries (empty content)
                if (fileContent.empty()) { // This entry represents a directory
                    if (fs::exists(outputPath)) {
                        if (fs::is_directory(outputPath)) {
                            std::cout << "Directory already exists: " << relativePathStr << "\n";
                        } else {
                            // Conflict: a file exists where a directory should be
                            std::cerr << "Warning: Cannot create directory '" << relativePathStr << "' because a file with that name already exists. Skipping.\n";
                            continue; // Skip this entry to prevent further errors
                        }
                    } else {
                        // Directory does not exist, create it
                        fs::create_directories(outputPath);
                        std::cout << "Extracted directory: " << relativePathStr << "\n";
                    }
                } else { // This entry represents a file (non-empty content)
                    // This is a file, write its content
                    std::ofstream outputFile(outputPath, std::ios::binary);
                    if (!outputFile.is_open()) {
                        std::cerr << "Warning: Could not create output file: " << outputPath << ". Skipping.\n";
                        continue; 
                    }

                    outputFile.write(fileContent.data(), fileContent.size());
                    outputFile.close();
                    std::cout << "Extracted file: " << relativePathStr << " (" << fileContent.size() << " bytes)\n";
                }
                extracted_count++;
            } else {
                skipped_count++;
            }
        }
        if (!extract_all && extracted_count == 0 && !files_to_extract.empty()) {
            std::cerr << "Warning: No specified files were found in the archive to extract.\n";
        } else if (!extract_all) {
            std::cout << "Extracted " << extracted_count << " items, skipped " << skipped_count << " items.\n";
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Error during unarchiving: " << e.what() << std::endl;
        std::cerr << "Archive might be corrupted or incomplete.\n";
        inputArchive.close();
        return 1; // Indicate error
    } catch (const std::exception& e) {
        std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
        inputArchive.close();
        return 1; // Indicate error
    }

    inputArchive.close(); // Close the input archive file
    std::cout << "Unarchiving complete.\n";

    return 0; // Indicate successful execution
}