#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstdint>
#include <map>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

// Структура заголовка файла tilemap
struct FileHeader {
    uint64_t version = 0;         // Версия формата
    uint64_t levels_count;        // Число уровней (z-уровни пирамиды)
    uint64_t tiles_block_side;    // Размер блока тайлов (TBS), степень 2
};

// Структура заголовка уровня
struct LevelHeader {
    uint64_t z;       // Уровень пирамиды
    uint64_t offset;  // Смещение к блокам тайлов
};

// Структура записи тайла
struct TileEntry {
    uint64_t offset;  // Смещение тайла от начала блока
    uint64_t size;    // Размер тайла в байтах (0, если тайл отсутствует)
};

// Структура блока тайлов
struct TileBlock {
    std::vector<TileEntry> tiles; // Список тайлов
};

// Функция записи tilemap-файла
void write_tilemap(const std::string& input_dir, const std::string& output_file, uint64_t tbs) {
    std::cout << "Начинаем процесс записи tilemap...\n";

    std::ofstream file(output_file, std::ios::binary);
    if (!file) {
        std::cerr << "Ошибка открытия " << output_file << " для записи.\n";
        return;
    }

    uint64_t max_z = 0;
    std::map<uint64_t, std::vector<std::pair<std::string, uint64_t>>> tile_files;

    // Сканируем файлы
    for (const auto& entry : fs::directory_iterator(input_dir)) {
        if (entry.path().extension() == ".jpg") {
            std::string filename = entry.path().stem().string();
            std::istringstream iss(filename);
            uint64_t x, y, z;
            char delim1, delim2;
            if (iss >> x >> delim1 >> y >> delim2 >> z && delim1 == '_' && delim2 == '_') {
                tile_files[z].emplace_back(entry.path().string(), fs::file_size(entry));
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
    uint64_t offset = sizeof(header) + levels_count * sizeof(LevelHeader);

    // Заполняем заголовки уровней
    for (uint64_t z = 0; z < levels_count; ++z) {
        level_headers[z] = { z, offset };
        offset += tile_files[z].size() * sizeof(TileEntry);
    }
    
    file.write(reinterpret_cast<char*>(level_headers.data()), level_headers.size() * sizeof(LevelHeader));

    std::map<uint64_t, TileBlock> tile_blocks;
    // Создаем блоки тайлов
    for (const auto& [z, files] : tile_files) {
        TileBlock block;
        for (const auto& [path, size] : files) {
            block.tiles.push_back({ offset, size });
            offset += size;
        }
        tile_blocks[z] = block;
    }

    // Записываем блоки тайлов
    for (uint64_t z = 0; z < levels_count; ++z) {
        if (tile_blocks.find(z) != tile_blocks.end()) {
            file.write(reinterpret_cast<char*>(tile_blocks[z].tiles.data()), tile_blocks[z].tiles.size() * sizeof(TileEntry));
        }
    }

    // Записываем данные тайлов
    for (const auto& [z, files] : tile_files) {
        for (const auto& [path, size] : files) {
            std::ifstream img_file(path, std::ios::binary);
            std::vector<char> buffer(size);
            img_file.read(buffer.data(), size);
            file.write(buffer.data(), size);
        }
    }

    std::cout << "Файл " << output_file << " успешно записан.\n";
}

// Функция чтения tilemap-файла
void read_tilemap(const std::string& input_file, const std::string& output_dir) {
    std::cout << "Начинаем процесс чтения tilemap...\n";
    std::ifstream file(input_file, std::ios::binary);
    if (!file) {
        std::cerr << "Ошибка открытия " << input_file << " для чтения.\n";
        return;
    }

    FileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    std::vector<LevelHeader> level_headers(header.levels_count);
    file.read(reinterpret_cast<char*>(level_headers.data()), level_headers.size() * sizeof(LevelHeader));

    fs::create_directory(output_dir);
    file.seekg(0, std::ios::end);
    std::streampos file_size = file.tellg();

    for (const auto& level : level_headers) {
        std::vector<TileEntry> entries(header.tiles_block_side * header.tiles_block_side);
        file.seekg(level.offset, std::ios::beg);
        file.read(reinterpret_cast<char*>(entries.data()), entries.size() * sizeof(TileEntry));

        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].size == 0) continue;

            uint64_t x = i % (static_cast<unsigned long long>(1) << level.z);
            uint64_t y = i / (static_cast<unsigned long long>(1) << level.z);

            // Формируем уникальное имя для файла, включая координаты и уровень
            std::ostringstream output_filename;
            output_filename << output_dir << "/" << x << "_" << y << "_" << level.z << ".jpg";

            // Проверяем, что смещение и размер не выходят за пределы файла
            if (entries[i].offset + entries[i].size > file_size) {
                continue;
            }

            std::vector<char> buffer(entries[i].size);
            file.seekg(entries[i].offset, std::ios::beg);
            file.read(buffer.data(), entries[i].size);

            // Записываем данные в файл с уникальным именем
            std::ofstream out_file(output_filename.str(), std::ios::binary);
            out_file.write(buffer.data(), buffer.size());
        }
    }

    std::cout << "Тайлы успешно извлечены в " << output_dir << "\n";
}


// Главная функция
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
    else {
        std::cerr << "Неизвестный режим: " << mode << "\n";
        return 1;
    }

    return 0;
}
