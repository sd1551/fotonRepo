#include <opencv2/opencv.hpp>
#include <iostream>
#include <sqlite3.h>
#include <boost/program_options.hpp>
#include <vector>
#include <exception>

namespace po = boost::program_options;

struct TableInfo {
    std::string tableName;
    int zoomLevel;
    std::string dbFile;
};

// Функция для получения цвета уровня масштаба
cv::Scalar getZoomLevelColor(int zoomLevel) {
    switch (zoomLevel) {
    case 9: return cv::Scalar(0, 255, 255);   // Желтый
    case 10: return cv::Scalar(255, 255, 0);  // Голубой
    case 11: return cv::Scalar(255, 0, 255);  // Магента
    case 12: return cv::Scalar(128, 128, 128); // Серый
    case 13: return cv::Scalar(0, 128, 255);  // Оранжевый
    case 14: return cv::Scalar(0, 255, 0);    // Зеленый
    case 15: return cv::Scalar(255, 0, 0);    // Красный
    case 16: return cv::Scalar(0, 0, 255);    // Синий
    default: return cv::Scalar(0, 0, 0);      // Черный (нет пикселей)
    }
}

// Функция для получения отсортированных названий таблиц
std::vector<TableInfo> getTableInfos(sqlite3* db, const std::string& dbFile) {
    const char* sqlQuery = "SELECT name FROM sqlite_master WHERE type='table';";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sqlQuery, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error("Не удалось подготовить SQL-запрос для получения таблиц: " + std::string(sqlite3_errmsg(db)));

    std::vector<TableInfo> tableInfos;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char* tableName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (tableName && std::string(tableName).find("tiles") == 0) {
            int zoomLevel = std::stoi(std::string(tableName).substr(5));
            tableInfos.push_back({ tableName, zoomLevel, dbFile });
        }
    }
    if (rc != SQLITE_DONE) throw std::runtime_error("Ошибка при выполнении SQL-запроса для получения таблиц: " + std::string(sqlite3_errmsg(db)));

    sqlite3_finalize(stmt);
    return tableInfos;
}

// Функция для обработки таблиц
void processTileTables(sqlite3* db, const std::vector<TableInfo>& tableInfos, cv::Mat& image, int imgSize) {
    for (const auto& tableInfo : tableInfos) {
        std::string sqlTileQuery = "SELECT y, x, state FROM " + tableInfo.tableName;
        sqlite3_stmt* tileStmt;
        int rc = sqlite3_prepare_v2(db, sqlTileQuery.c_str(), -1, &tileStmt, nullptr);
        if (rc != SQLITE_OK) throw std::runtime_error("Не удалось подготовить SQL-запрос для получения тайлов: " + std::string(sqlite3_errmsg(db)));

        int zoomLevel = tableInfo.zoomLevel;
        int tileSize = imgSize / (1 << zoomLevel);

        while ((rc = sqlite3_step(tileStmt)) == SQLITE_ROW) {
            int y = sqlite3_column_int(tileStmt, 0);
            int x = sqlite3_column_int(tileStmt, 1);
            int state = sqlite3_column_int(tileStmt, 2);

            int imgY = y * tileSize;
            int imgX = x * tileSize;

            // Окрашиваем пиксель цветом текущего уровня масштаба
            cv::rectangle(image, cv::Point(imgX, imgY), cv::Point(imgX + tileSize, imgY + tileSize), getZoomLevelColor(zoomLevel), cv::FILLED);
        }

        sqlite3_finalize(tileStmt);
    }
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
        image.setTo(getZoomLevelColor(0)); // Фоновый цвет

        // Сбор таблиц из всех БД
        std::vector<TableInfo> allTableInfos;
        for (const auto& dbFile : dbFiles) {
            sqlite3* db;
            int rc = sqlite3_open_v2(dbFile.c_str(), &db, SQLITE_OPEN_READONLY, NULL);
            if (rc != SQLITE_OK) throw std::runtime_error("Не удалось открыть базу данных " + dbFile + ": " + std::string(sqlite3_errmsg(db)));

            auto tableInfos = getTableInfos(db, dbFile);
            allTableInfos.insert(allTableInfos.end(), tableInfos.begin(), tableInfos.end());

            sqlite3_close(db);
        }

        // Сортировка всех таблиц по уровню масштаба
        std::sort(allTableInfos.begin(), allTableInfos.end(), [](const TableInfo& a, const TableInfo& b) {
            return a.zoomLevel < b.zoomLevel;
            });

        // Обработка таблиц и создание изображения
        for (const auto& tableInfo : allTableInfos) {
            sqlite3* db;
            int rc = sqlite3_open_v2(tableInfo.dbFile.c_str(), &db, SQLITE_OPEN_READONLY, NULL);
            if (rc != SQLITE_OK) throw std::runtime_error("Не удалось открыть базу данных " + tableInfo.dbFile + ": " + std::string(sqlite3_errmsg(db)));

            processTileTables(db, { tableInfo }, image, imgSize);

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