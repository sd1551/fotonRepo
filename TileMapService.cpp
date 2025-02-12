#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstdint>
#include <map>
#include <cmath>
#include <sstream>

namespace fs = std::filesystem;

struct FileHeader {
    uint64_t version = 0;
    uint64_t levels_count;
    uint64_t tiles_block_side;
};

struct LevelHeader {
    std::vector<uint64_t> block_offsets;
};

struct TileEntry {
    uint64_t offset;
    uint64_t size;
};

void write_tilemap(const std::string& input_dir, const std::string& output_file, uint64_t tbs) {
    std::ofstream file(output_file, std::ios::binary);
    if (!file) {
        std::cerr << "Ошибка открытия " << output_file << " для записи.\n";
        return;
    }

    uint64_t max_z = 0;
    std::map<uint64_t, std::vector<std::tuple<uint64_t, uint64_t, std::string, uint64_t>>> tile_files;

    for (const auto& entry : fs::directory_iterator(input_dir)) {
        if (entry.path().extension() == ".jpg") {
            std::string filename = entry.path().stem().string();
            std::istringstream iss(filename);
            uint64_t x, y, z;
            char delim1, delim2;
            if (iss >> x >> delim1 >> y >> delim2 >> z && delim1 == '_' && delim2 == '_') {
                tile_files[z].emplace_back(x, y, entry.path().string(), fs::file_size(entry));
                max_z = std::max(max_z, z);
            }
        }
    }

    if (tile_files.empty()) {
        std::cerr << "Ошибка: нет валидных файлов .jpg в " << input_dir << "\n";
        return;
    }

    uint64_t levels_count = max_z + 1;
    FileHeader header = { 0, levels_count, tbs };
    file.write(reinterpret_cast<char*>(&header), sizeof(header));

    std::vector<LevelHeader> level_headers(levels_count);
    uint64_t offset = sizeof(header);

    for (uint64_t z = 0; z < levels_count; ++z) {
        uint64_t blocks_side = std::ceil((1ULL << z) / static_cast<double>(tbs));
        level_headers[z].block_offsets.resize(blocks_side * blocks_side, 0);
        offset += blocks_side * blocks_side * sizeof(uint64_t);
    }

    for (const auto& lh : level_headers) {
        file.write(reinterpret_cast<const char*>(lh.block_offsets.data()), lh.block_offsets.size() * sizeof(uint64_t));
    }

    std::map<uint64_t, std::vector<TileEntry>> tile_entries;
    for (const auto& [z, files] : tile_files) {
        std::vector<TileEntry> entries;
        for (const auto& [x, y, path, size] : files) {
            entries.push_back({ offset, size });
            offset += size;
        }
        tile_entries[z] = entries;
    }

    for (uint64_t z = 0; z < levels_count; ++z) {
        if (!tile_entries[z].empty()) {
            file.write(reinterpret_cast<char*>(tile_entries[z].data()), tile_entries[z].size() * sizeof(TileEntry));
        }
    }

    // Запись данных изображений
    for (const auto& [z, files] : tile_files) {
        for (const auto& [x, y, path, size] : files) {
            std::ifstream img_file(path, std::ios::binary);
            if (!img_file) {
                std::cerr << "Ошибка открытия файла изображения: " << path << "\n";
                continue;
            }

            std::vector<char> buffer(size);
            img_file.read(buffer.data(), size);

            if (!img_file) {
                std::cerr << "Ошибка чтения файла изображения: " << path << "\n";
                continue;
            }

            file.write(buffer.data(), size);
        }
    }
    std::cout << "Файл " << output_file << " успешно записан.\n";
}


void read_tilemap(const std::string& input_file, const std::string& output_dir) {
    std::ifstream file(input_file, std::ios::binary);
    if (!file) {
        std::cerr << "Ошибка открытия " << input_file << " для чтения.\n";
        return;
    }

    FileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    std::vector<LevelHeader> level_headers(header.levels_count);
    for (uint64_t z = 0; z < header.levels_count; ++z) {
        uint64_t blocks_side = std::ceil((1ULL << z) / static_cast<double>(header.tiles_block_side));
        level_headers[z].block_offsets.resize(blocks_side * blocks_side);
        file.read(reinterpret_cast<char*>(level_headers[z].block_offsets.data()), level_headers[z].block_offsets.size() * sizeof(uint64_t));
    }

    fs::create_directory(output_dir);

    // Чтение и извлечение изображений
    for (uint64_t z = 0; z < header.levels_count; ++z) {
        for (uint64_t i = 0; i < level_headers[z].block_offsets.size(); ++i) {
            uint64_t offset = level_headers[z].block_offsets[i];
            if (offset == 0) continue;

            file.seekg(offset, std::ios::beg);

            std::vector<TileEntry> tiles(header.tiles_block_side * header.tiles_block_side);
            file.read(reinterpret_cast<char*>(tiles.data()), tiles.size() * sizeof(TileEntry));

            for (uint64_t j = 0; j < tiles.size(); ++j) {
                if (tiles[j].size == 0) continue;

                std::ostringstream filename;
                filename << output_dir << "/tile_" << i << "_" << j << "_" << z << ".jpg";

                std::vector<char> buffer(tiles[j].size);
                file.seekg(offset + tiles[j].offset, std::ios::beg);
                file.read(buffer.data(), tiles[j].size);

                std::ofstream out_file(filename.str(), std::ios::binary);
                out_file.write(buffer.data(), buffer.size());
            }
        }
    }

    std::cout << "Тайлы успешно извлечены в " << output_dir << "\n";
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "russian");
    if (argc < 4) {
        std::cerr << "Использование:\n"
            << "  Запись: " << argv[0] << " write <папка_с_тайлами> <файл_tilemap> <размер_блока>\n"
            << "  Чтение: " << argv[0] << " read <файл_tilemap> <папка_выхода>\n";
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "write") {
        write_tilemap(argv[2], argv[3], std::stoull(argv[4]));
    }
    else if (mode == "read") {
        read_tilemap(argv[2], argv[3]);
    }
    return 0;
}