#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <cstring>  // For std::strlen
#include "./libs/hypersonic-rle-kit/src/rle.h"

using std::cout;
using std::endl;

int main() {
    const char* text_to_encode = "FooooooooBar";  // C-style string

    uint8_t* pUncompressedData = (uint8_t*)text_to_encode;
    uint32_t fileSize = static_cast<uint32_t>(std::strlen(text_to_encode)); // Use strlen to get the size

    // Get Compress Bounds
    const uint32_t compressedBufferSize = rle_compress_bounds(fileSize);
    uint8_t* pCompressedData = (uint8_t*)malloc(compressedBufferSize);

    // Check for memory allocation failure
    if (!pCompressedData) {
        throw std::runtime_error("Memory allocation failed for compressed data");
    }

    // Compress
    const uint32_t compressedSize = rle8_multi_compress(pUncompressedData, fileSize, pCompressedData, compressedBufferSize);

    // Allocate Output Buffer
    uint8_t* pDecompressedData = (uint8_t*)malloc(fileSize + rle_decompress_additional_size());

    // Check for memory allocation failure
    if (!pDecompressedData) {
        free(pCompressedData);  // Clean up already allocated memory
        throw std::runtime_error("Memory allocation failed for decompressed data");
    }

    // Decompress
    const uint32_t decompressedSize = rle8_decompress(pCompressedData, compressedSize, pDecompressedData, fileSize);


    // Print decompressed data
    cout << "decompressed data: ";
    for (uint32_t i = 0; i < decompressedSize; ++i) {
        cout << std::hex << static_cast<int>(pDecompressedData[i]) << " ";
    }
    cout << endl;


    // Print compressed data
    cout << "Compressed data: ";
    for (uint32_t i = 0; i < compressedSize; ++i) {
        cout << std::hex << static_cast<int>(pCompressedData[i]) << " ";
    }
    cout << endl;

    // Cleanup
    free(pCompressedData);
    free(pDecompressedData);

    return 0;
}