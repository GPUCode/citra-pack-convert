#include <fstream>
#include <string>
#include <string_view>
#include <cstdio>
#include <span>
#include <vector>
#include <filesystem>
#include <lodepng.h>
#include <fmt/format.h>
#include "common_types.h"
#include "texture_codec.h"
#include "cityhash.h"

using namespace VideoCore;

bool DecodePNG(std::span<const u8> src, std::vector<u8>& dst, u32& width,
          u32& height) {
    const u32 lodepng_ret = lodepng::decode(dst, width, height, src.data(), src.size());
    if (lodepng_ret) {
        printf("Failed to decode because %s\n", lodepng_error_text(lodepng_ret));
        return false;
    }
    return true;
}

bool EncodePNG(const std::string& path, std::span<const u8> src, u32 width, u32 height) {
    std::vector<u8> out;
    const u32 lodepng_ret = lodepng::encode(out, src.data(), width, height);
    if (lodepng_ret) {
        printf("Failed to encode %s because %s", path.c_str(), lodepng_error_text(lodepng_ret));
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::out);
    file.write((char*)out.data(), out.size());
    file.flush();

    return true;
}

static inline u64 ComputeHash64(const void* data, std::size_t len) noexcept {
    return Common::CityHash64(static_cast<const char*>(data), len);
}

void FlipRGBA8Texture(std::span<u8> tex, u32 width, u32 height) {
    const u32 line_size = width * 4;
    for (u32 line = 0; line < height / 2; line++) {
        const u32 offset_1 = line * line_size;
        const u32 offset_2 = (height - line - 1) * line_size;
        // Swap lines
        std::swap_ranges(tex.begin() + offset_1, tex.begin() + offset_1 + line_size,
                         tex.begin() + offset_2);
    }
}

u64 GenerateOldHash(u32 format, u32 width, u32 height, std::span<u8> source) {
    FlipRGBA8Texture(source, width, height);
    const size_t target_bpp = GetFormatBytesPerPixel(static_cast<PixelFormat>(format));
    const size_t size = width * height * target_bpp;
    std::vector<u8> converted(size);
    for (u32 i = 0; i < width * height; i++) {
        const auto rgba = *(Common::Vec4<u8>*)(source.data() + i * 4);
        u8* output = converted.data() + i * target_bpp;
        switch (format) {
        case 1:
            Common::Color::EncodeRGB8(rgba, output);
            break;
        case 2:
            Common::Color::EncodeRGB5A1(rgba, output);
            break;
        case 3:
            Common::Color::EncodeRGB565(rgba, output);
            break;
        case 4:
            Common::Color::EncodeRGBA4(rgba, output);
            break;
        default:
            std::memcpy(output, rgba.AsArray(), 4);
            break;
        }
    }

    return ComputeHash64(converted.data(), converted.size());
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fmt::print("Usage: convert <new-stock-dump> <legacy-pack> <output>");
        return 0;
    }

    const std::string new_stock_dump_dir{argv[1]};
    const std::string legacy_pack_dir{argv[2]};
    const std::string output_dir{argv[3]};

    for (const auto& entry : std::filesystem::directory_iterator(new_stock_dump_dir)) {
        u32 width;
        u32 height;
        u32 format;
        unsigned long long hash{};
        u32 mip{};
        char ext[3];
        const std::string filename = entry.path().filename().string();
        if (std::sscanf(filename.c_str(), "tex1_%ux%u_%llX_%u_mip%u.%s", &width, &height, &hash,
                        &format, &mip, ext) != 6) {
            fmt::print("Failed to parse filename {}\n", filename.c_str());
            continue;
        }
        if (std::string_view{ext} != "png") {
            continue;
        }

        fmt::print("Converting file {}\n", filename.c_str());

        std::ifstream file{entry.path(), std::ios::binary | std::ios::in};
        std::vector<u8> input((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        std::vector<u8> data;
        DecodePNG(input, data, width, height);
        if (data.size() != width * height * 4) {
            fmt::print("Loaded png size is bad\n");
            continue;
        }

        const u64 old_hash = GenerateOldHash(format, width, height, data);

        const auto old_filename = fmt::format("tex1_{}x{}_{:016X}_{}.png",
                                              width, height, old_hash, format);
        const auto new_filename = fmt::format("tex1_{}x{}_{:016X}_{}_mip{}.png",
                                              width, height, hash, format, mip);
        const auto src_file = legacy_pack_dir + "/" + old_filename;
        const auto dest_file = output_dir + "/" + new_filename;

        try {
            std::filesystem::copy_file(src_file, dest_file, std::filesystem::copy_options::overwrite_existing);
        } catch(std::filesystem::filesystem_error& e) {
            fmt::print("Could not copy: {}\n", e.what());
        }
    }
}
