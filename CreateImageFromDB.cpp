#include <opencv2/opencv.hpp>
#include <iostream>
#include <sqlite3.h>
#include <boost/program_options.hpp>
#include <vector>
#include <unordered_map>

namespace po = boost::program_options;

// Функция для получения цвета тайла
cv::Scalar getTileColor(int state) {
    switch (state) {
    case 0: return cv::Scalar(0, 255, 0);   // Зеленый - success
    case 1: return cv::Scalar(0, 0, 255);   // Красный - queued
    case 2: return cv::Scalar(255, 0, 0);   // Синий - no_connection
    case 3: return cv::Scalar(0, 255, 255); // Желтый - general_http_error_code
    case 4: return cv::Scalar(255, 255, 0); // Циан - http_404_code
    case 5: return cv::Scalar(255, 0, 255); // Магента - other_query_error
    case 6: return cv::Scalar(128, 128, 128); // Серый - other
    case 7: return cv::Scalar(0, 128, 255);   // Оранжевый - connection_timeout
    default: return cv::Scalar(0, 0, 0);   // Черный - неизвестное состояние
    }
}

// Функция для обработки ошибки SQLite
void checkSQLiteError(int rc, sqlite3* db, const std::string& errorMsg) {
    if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE) {
        std::cerr << errorMsg << ": " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        exit(1);
    }
}

// Функция для получения данных всех тайлов в одном запросе
std::unordered_map<std::string, std::vector<std::tuple<int, int, int>>> getTilesData(sqlite3* db) {
    std::unordered_map<std::string, std::vector<std::tuple<int, int, int>>> tilesData;

    // SQL-запрос для получения всех тайлов
    const char* sqlQuery = "SELECT name FROM sqlite_master WHERE type='table';";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sqlQuery, -1, &stmt, nullptr);
    checkSQLiteError(rc, db, "Не удалось подготовить SQL-запрос для получения таблиц");

    // Получаем названия таблиц
    std::vector<std::string> tableNames;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char* tableName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (tableName && std::string(tableName).find("tiles") == 0) {
            tableNames.emplace_back(tableName);
        }
    }
    checkSQLiteError(rc, db, "Ошибка при выполнении SQL-запроса для получения таблиц");
    sqlite3_finalize(stmt);

    // Запрашиваем данные для всех тайлов в одном запросе

    for (const auto& tableName : tableNames) {
        std::string sqlTileQuery = "SELECT y, x, state FROM " + tableName;
        sqlite3_stmt* tileStmt;
        rc = sqlite3_prepare_v2(db, sqlTileQuery.c_str(), -1, &tileStmt, nullptr);
        checkSQLiteError(rc, db, "Не удалось подготовить SQL-запрос для получения тайлов");

        while ((rc = sqlite3_step(tileStmt)) == SQLITE_ROW) {
            int y = sqlite3_column_int(tileStmt, 0);
            int x = sqlite3_column_int(tileStmt, 1);
            int state = sqlite3_column_int(tileStmt, 2);
            tilesData[tableName].emplace_back(y, x, state);
        }
        checkSQLiteError(rc, db, "Ошибка при выполнении SQL-запроса для получения тайлов");
        sqlite3_finalize(tileStmt);
    }

    return tilesData;
}

// Функция для заполнения изображения
void fillImageFromTiles(const std::unordered_map<std::string, std::vector<std::tuple<int, int, int>>>& tilesData, cv::Mat& image, int imgSize) {
    for (const auto& [tableName, tiles] : tilesData) {
        // Размер одного тайла на текущем уровне
        int zoomLevel = std::stoi(tableName.substr(5));
        int tileSize = imgSize / (1 << zoomLevel);

        // Заполняем изображение цветами тайлов
        for (const auto& [y, x, state] : tiles) {
            // Вычисляем позицию в конечном изображении
            int imgY = y * tileSize;
            int imgX = x * tileSize;

            // Устанавливаем цвет пикселя, соответствующий состоянию
            cv::Scalar color = getTileColor(state);

            // Заливаем соответствующий квадрат в изображении
            cv::rectangle(image, cv::Point(imgX, imgY), cv::Point(imgX + tileSize, imgY + tileSize), color, cv::FILLED);
        }
    }
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "russian");
    std::string dbFile, outputImage;

    po::options_description desc("Допустимые параметры");
    desc.add_options()
        ("help,h", "Показать справку")
        ("database,d", po::value<std::string>(&dbFile)->default_value("C:\\opencv\\FotonProject\\stat-antarctica.db"), "Путь к файлу базы данных")
        ("output,o", po::value<std::string>(&outputImage)->default_value("C:\\opencv\\FotonProject\\stitched_image1.png"), "Путь к выходному изображению");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }

    sqlite3* db;
    int rc = sqlite3_open(dbFile.c_str(), &db);
    checkSQLiteError(rc, db, "Не удалось открыть базу данных");

    // Получаем данные всех тайлов
    auto tilesData = getTilesData(db);

    // Размер выходного изображения: 2^13 = 8192 пикселей на каждую сторону
    const int zoomOutLevel = 13;
    const int imgSize = 1 << zoomOutLevel;  // 2^zoomOutLevel

    // Создаем изображение и заполняем его фоном
    cv::Mat image(imgSize, imgSize, CV_8UC3);
    image.setTo(getTileColor(6)); // Фоновый цвет (серый)

    // Заполняем изображение тайлами
    fillImageFromTiles(tilesData, image, imgSize);

    // Сохраняем изображение в файл
    if (!cv::imwrite(outputImage, image)) {
        std::cerr << "Ошибка при сохранении изображения!" << std::endl;
    }

    // Закрываем базу данных
    sqlite3_close(db);
    std::cout << "Изображение сохранено в " << outputImage << std::endl;
    return 0;
}