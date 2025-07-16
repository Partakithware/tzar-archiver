// === tzar_encrypt.cpp ===
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <limits> // For std::numeric_limits
#include <filesystem> // For fs::path

namespace fs = std::filesystem; // Alias for std::filesystem

// --- Basic SHA256 Implementation (for password hashing) ---
// This is a simplified, self-contained SHA256. NOT for production use.
// Based on public domain implementations and FIPS 180-4.

// Rotate right
#define ROTR(x, n) ((x >> n) | (x << (32 - n)))

// SHA256 functions
#define CH(x, y, z) ((x & y) ^ (~x & z))
#define MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define SIG0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SIG1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define CAP_SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ (x >> 3))
#define CAP_SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ (x >> 10))

// SHA256 K constants
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b94ca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// Initial hash values H0-H7
static uint32_t H[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

// SHA256 compression function
void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    uint32_t W[64];

    for (int i = 0; i < 16; ++i) {
        W[i] = (block[i * 4] << 24) | (block[i * 4 + 1] << 16) | (block[i * 4 + 2] << 8) | block[i * 4 + 3];
    }

    for (int i = 16; i < 64; ++i) {
        W[i] = CAP_SIG1(W[i - 2]) + W[i - 7] + CAP_SIG0(W[i - 15]) + W[i - 16];
    }

    for (int i = 0; i < 64; ++i) {
        uint32_t T1 = h + SIG1(e) + CH(e, f, g) + K[i] + W[i];
        uint32_t T2 = SIG0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

// Computes SHA256 hash of a byte vector. Returns 32-byte hash.
std::vector<uint8_t> sha256(const std::vector<uint8_t>& data) {
    uint32_t state[8];
    for(int i = 0; i < 8; ++i) state[i] = H[i]; // Reset state for each call

    std::vector<uint8_t> padded_data = data;
    uint64_t bit_len = data.size() * 8;

    // Pad with a '1' bit
    padded_data.push_back(0x80);

    // Pad with zeros until length is 448 mod 512 bits (56 mod 64 bytes)
    while ((padded_data.size() % 64) != 56) {
        padded_data.push_back(0x00);
    }

    // Append 64-bit message length
    for (int i = 7; i >= 0; --i) {
        padded_data.push_back((bit_len >> (8 * i)) & 0xFF);
    }

    for (size_t i = 0; i < padded_data.size(); i += 64) {
        sha256_transform(state, &padded_data[i]);
    }

    std::vector<uint8_t> hash(32);
    for (int i = 0; i < 8; ++i) {
        hash[i * 4] = (state[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (state[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (state[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = state[i] & 0xFF;
    }
    return hash;
}

// --- XOR Encryption/Decryption Function ---
// Key is repeated if data is longer than key.
std::vector<char> xor_cipher(const std::vector<char>& data, const std::vector<uint8_t>& key) {
    if (key.empty()) {
        // If key is empty, return data unchanged (or throw error, depending on desired behavior)
        return data;
    }
    std::vector<char> output = data;
    for (size_t i = 0; i < data.size(); ++i) {
        output[i] = data[i] ^ key[i % key.size()];
    }
    return output;
}

// --- File I/O Helpers ---
void writeString(std::ofstream& outFile, const std::string& str) {
    uint32_t len = str.length();
    outFile.write(reinterpret_cast<const char*>(&len), sizeof(len));
    outFile.write(str.c_str(), len);
}

void writeBinaryData(std::ofstream& outFile, const std::vector<char>& data) {
    uint64_t size = data.size();
    outFile.write(reinterpret_cast<const char*>(&size), sizeof(size));
    outFile.write(data.data(), size);
}

std::string readString(std::ifstream& inFile) {
    uint32_t len;
    inFile.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (!inFile) throw std::runtime_error("Error reading string length.");
    std::vector<char> buffer(len);
    inFile.read(buffer.data(), len);
    if (!inFile) throw std::runtime_error("Error reading string data.");
    return std::string(buffer.begin(), buffer.end());
}

std::vector<char> readBinaryData(std::ifstream& inFile) {
    uint64_t size;
    inFile.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (!inFile) throw std::runtime_error("Error reading binary data size.");
    std::vector<char> data(size);
    if (size > 0) {
        inFile.read(data.data(), size);
        if (!inFile) throw std::runtime_error("Error reading binary data.");
    }
    return data;
}

int main(int argc, char* argv[]) {
    // Usage: ./tzar_encrypt <input_tzar_file> <output_base_name> [password]
    // The output file will always have the .tzar2 extension.
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_tzar_file> <output_base_name> [password]\n";
        std::cerr << "If password is not provided, it will be prompted.\n";
        return 1;
    }

    std::string input_tzar_path = argv[1];
    
    // Get the base name from the second argument and hard-set .tzar2 extension
    fs::path provided_output_path(argv[2]);
    std::string output_tzar2_path = provided_output_path.stem().string() + ".tzar2";

    std::string password;

    if (argc == 4) {
        password = argv[3];
    } else {
        std::cout << "Enter password for encryption: ";
        std::getline(std::cin, password);
    }

    if (password.empty()) {
        std::cerr << "Error: Password cannot be empty for encryption.\n";
        return 1;
    }

    std::vector<uint8_t> password_bytes(password.begin(), password.end());
    std::vector<uint8_t> encryption_key = sha256(password_bytes);

    std::ifstream inFile(input_tzar_path, std::ios::binary);
    if (!inFile.is_open()) {
        std::cerr << "Error: Could not open input .tzar file: " << input_tzar_path << std::endl;
        return 1;
    }

    std::ofstream outFile(output_tzar2_path, std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open output .tzar2 file: " << output_tzar2_path << std::endl;
        inFile.close();
        return 1;
    }

    // Write encryption flag (0x01 for encrypted)
    outFile.put(0x01); 

    try {
        while (inFile.peek() != EOF) {
            std::string filename = readString(inFile);
            std::vector<char> file_content = readBinaryData(inFile);

            // Encrypt the file content
            std::vector<char> encrypted_content = xor_cipher(file_content, encryption_key);

            // Write filename and its size (unencrypted)
            writeString(outFile, filename);
            // Write encrypted content and its size
            writeBinaryData(outFile, encrypted_content);

            std::cout << "Encrypted: " << filename << " (" << file_content.size() << " bytes)\n";
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Error during encryption: " << e.what() << std::endl;
        inFile.close();
        outFile.close();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
        inFile.close();
        outFile.close();
        return 1;
    }

    inFile.close();
    outFile.close();
    std::cout << "Encryption complete. Encrypted archive saved to: " << output_tzar2_path << std::endl;

    return 0;
}
