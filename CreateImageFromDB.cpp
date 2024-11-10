#include <opencv2/opencv.hpp>
#include <iostream>
#include <sqlite3.h>
#include <boost/program_options.hpp>
#include <vector>
#include <exception>

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

// Функция для получения названий таблиц
std::vector<std::string> getTableNames(sqlite3* db) {
    const char* sqlQuery = "SELECT name FROM sqlite_master WHERE type='table';";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sqlQuery, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error("Не удалось подготовить SQL-запрос для получения таблиц: " + std::string(sqlite3_errmsg(db)));

    std::vector<std::string> tableNames;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char* tableName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (tableName && std::string(tableName).find("tiles") == 0) {
            tableNames.emplace_back(tableName);
        }
    }
    if (rc != SQLITE_DONE) throw std::runtime_error("Ошибка при выполнении SQL-запроса для получения таблиц: " + std::string(sqlite3_errmsg(db)));

    sqlite3_finalize(stmt);
    return tableNames;
}

// Функция для обработки тайлов таблицы и заполнения изображения
void processTileTable(sqlite3* db, const std::string& tableName, cv::Mat& image, int imgSize) {
    std::string sqlTileQuery = "SELECT y, x, state FROM " + tableName;
    sqlite3_stmt* tileStmt;
    int rc = sqlite3_prepare_v2(db, sqlTileQuery.c_str(), -1, &tileStmt, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error("Не удалось подготовить SQL-запрос для получения тайлов: " + std::string(sqlite3_errmsg(db)));

    int zoomLevel = std::stoi(tableName.substr(5));
    int tileSize = imgSize / (1 << zoomLevel);

    while ((rc = sqlite3_step(tileStmt)) == SQLITE_ROW) {
        int y = sqlite3_column_int(tileStmt, 0);
        int x = sqlite3_column_int(tileStmt, 1);
        int state = sqlite3_column_int(tileStmt, 2);

        int imgY = y * tileSize;
        int imgX = x * tileSize;

        cv::Scalar color = getTileColor(state);
        cv::rectangle(image, cv::Point(imgX, imgY), cv::Point(imgX + tileSize, imgY + tileSize), color, cv::FILLED);
    }
    if (rc != SQLITE_DONE) throw std::runtime_error("Ошибка при выполнении SQL-запроса для получения тайлов: " + std::string(sqlite3_errmsg(db)));

    sqlite3_finalize(tileStmt);
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "russian");
    try {
        std::vector<std::string> dbFiles;
        std::string outputImage;
        int zoomOutLevel = 13;

        po::options_description desc("Допустимые параметры");
        desc.add_options()
            ("help,h", "Показать справку")
            ("databases,d", po::value<std::vector<std::string>>(&dbFiles)->multitoken()->required(), "Пути к файлам баз данных")
            ("output,o", po::value<std::string>(&outputImage)->default_value("C:\\opencv\\FotonProject\\stitched_image.png"), "Путь к выходному изображению")
            ("zoom,z", po::value<int>(&zoomOutLevel)->default_value(13), "Уровень масштаба (по умолчанию 13)");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        const int imgSize = 1 << zoomOutLevel;

        cv::Mat image(imgSize, imgSize, CV_8UC3);
        image.setTo(getTileColor(6)); // Фоновый цвет (серый)

        for (const auto& dbFile : dbFiles) {
            sqlite3* db;
            int rc = sqlite3_open_v2(dbFile.c_str(), &db, SQLITE_OPEN_READONLY, NULL);
            if (rc != SQLITE_OK) throw std::runtime_error("Не удалось открыть базу данных " + dbFile + ": " + std::string(sqlite3_errmsg(db)));

            auto tableNames = getTableNames(db);
            for (const auto& tableName : tableNames) {
                processTileTable(db, tableName, image, imgSize);
            }

            sqlite3_close(db);
        }

        if (!cv::imwrite(outputImage, image)) {
            throw std::runtime_error("Ошибка при сохранении изображения!");
        }

        std::cout << "Изображение сохранено в " << outputImage << std::endl;
    }
    catch (const po::error& e) {
        std::cerr << "Ошибка парсинга аргументов: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Произошла ошибка: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}