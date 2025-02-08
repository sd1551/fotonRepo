#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstdint>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

// Структура заголовка файла tilemap
struct FileHeader {
    uint64_t version = 1;
    uint64_t levels_count;
    uint64_t tiles_block_side;
};

// Структура заголовка уровня
struct LevelHeader {
    uint64_t offset; 
    uint64_t blocks_count;
};

// Структура записи тайла
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
    FileHeader header = { 1, levels_count, tbs };
    file.write(reinterpret_cast<char*>(&header), sizeof(header));

    std::vector<LevelHeader> level_headers(levels_count);
    uint64_t offset = sizeof(header) + levels_count * sizeof(LevelHeader);

    for (uint64_t z = 0; z < levels_count; ++z) {
        level_headers[z].offset = offset;
        level_headers[z].blocks_count = tile_files[z].size();
        offset += tile_files[z].size() * sizeof(TileEntry);
    }

    file.write(reinterpret_cast<char*>(level_headers.data()), level_headers.size() * sizeof(LevelHeader));

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

    for (const auto& [z, files] : tile_files) {
        for (const auto& [x, y, path, size] : files) {
            std::ifstream img_file(path, std::ios::binary);
            std::vector<char> buffer(size);
            img_file.read(buffer.data(), size);
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
    file.read(reinterpret_cast<char*>(level_headers.data()), header.levels_count * sizeof(LevelHeader));

    fs::create_directory(output_dir);
    file.seekg(0, std::ios::end);
    std::streampos file_size = file.tellg();

    for (size_t z = 0; z < header.levels_count; ++z) {
        std::vector<TileEntry> entries(level_headers[z].blocks_count);
        file.seekg(level_headers[z].offset, std::ios::beg);
        file.read(reinterpret_cast<char*>(entries.data()), entries.size() * sizeof(TileEntry));

        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].size == 0) continue;

            uint64_t x = i % (1ULL << z);
            uint64_t y = i / (1ULL << z);

            std::ostringstream output_filename;
            output_filename << output_dir << "/" << x << "_" << y << "_" << z << ".jpg";

            if (entries[i].offset + entries[i].size > file_size) {
                continue;
            }

            std::vector<char> buffer(entries[i].size);
            file.seekg(entries[i].offset, std::ios::beg);
            file.read(buffer.data(), entries[i].size);

            std::ofstream out_file(output_filename.str(), std::ios::binary);
            out_file.write(buffer.data(), buffer.size());
        }
    }
    std::cout << "Тайлы успешно извлечены в " << output_dir << "\n";
}

int main(int argc, char* argv[]) {
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
    else {
        std::cerr << "Неизвестный режим: " << mode << "\n";
        return 1;
    }
    return 0;
}