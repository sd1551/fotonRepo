#include <opencv2/opencv.hpp>
#include <iostream>
#include <sqlite3.h>
#include <unordered_map>

const std::string dbFile = "C:\\opencv\\fotonApp\\experiment.db";
const std::string outputImage = "C:\\opencv\\fotonApp\\tiles_image.png";

// Цвета для разных состояний
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

int main() {
    setlocale(LC_ALL, "russian");
    sqlite3* db;
    sqlite3_stmt* stmt;

    // Открываем базу данных
    int rc = sqlite3_open(dbFile.c_str(), &db);
    checkSQLiteError(rc, db, "Не удалось открыть базу данных");

    // SQL-запрос для получения состояния тайлов
    const char* sqlQuery = "SELECT y, x, state FROM tiles11_states";

    // Подготавливаем SQL-запрос
    rc = sqlite3_prepare_v2(db, sqlQuery, -1, &stmt, nullptr);
    checkSQLiteError(rc, db, "Не удалось подготовить SQL-запрос");

    // Размер изображения: 2^11 = 2048 пикселей на каждую сторону
    const int z = 11;
    const int imgSize = 1 << z;  // 2^z
    const int tileSize = 1;

    // Создаем изображение
    cv::Mat image = cv::Mat::zeros(imgSize, imgSize, CV_8UC3);

    // Выполняем запрос и заполняем изображение цветами тайлов
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int y = sqlite3_column_int(stmt, 0);
        int x = sqlite3_column_int(stmt, 1);
        int state = sqlite3_column_int(stmt, 2);

        // Устанавливаем цвет пикселя, соответствующий состоянию
        cv::Scalar color = getTileColor(state);
        image.at<cv::Vec3b>(y, x) = cv::Vec3b(color[0], color[1], color[2]);
    }
    checkSQLiteError(rc, db, "Ошибка при выполнении SQL-запроса");

    // Сохраняем изображение в файл
    if (!cv::imwrite(outputImage, image)) {
        std::cerr << "Ошибка при сохранении изображения!" << std::endl;
    }

    // Закрываем запрос и базу данных
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::cout << "Изображение сохранено в " << outputImage << std::endl;
    return 0;
}
