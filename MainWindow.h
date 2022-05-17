//
// Created by ellmac on 14.05.2022.
//

#pragma once

#ifndef ZFCD_MAINWINDOW_H
#define ZFCD_MAINWINDOW_H

#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <stdexcept>
#include <fstream>
#include <array>

#include <QMainWindow>
#include <QPushButton>
#include <QGridLayout>
#include <QGraphicsView>
#include <QLabel>
#include <QFileDialog>
#include <QSplitter>
#include <QProgressBar>
#include <QMenu>
#include <QContextMenuEvent>

struct BIT_FILE {
    /*
     * Структура побитового доступа к файлу
     * */

    FILE *file;
    unsigned char mask;
    int rack;
    int pacifier_counter;
};

class MainWindow final : public QMainWindow {
Q_OBJECT
public:
    MainWindow();

    ~MainWindow() override;

public slots:

    void openFileDialog();

    void connectMethodDependMode();

private:
    const qint32 WINDOW_WIDTH = 300, WINDOW_HEIGHT = 200;
    const QString WINDOW_TITLE = "ZipFile";
    QGraphicsView *centralWidget;
    QGridLayout *centralLayout;
    QLabel *fileLabel;
    QLabel *selectedFileName;
    QPushButton *selectFileButton;
    QPushButton *startButton;
    QPushButton *closeButton;
    QLabel *elapsedTimeLabel;
    QLabel *elapsedTimeTextValue;
    QLabel *compressionRatioLabel;
    QLabel *compressionRatioTextValue;
    QProgressBar *progressBar;
    QLabel *sourceFileSize;
    QLabel *sourceFileSizeValue;
    QLabel *receivedFileSize;
    QLabel *receivedFileSizeValue;

    enum WORKING_MODES {
        ENCODE, DECODE
    };
    uint32_t MODE = ENCODE;

    void encode(FILE *input, const std::string &filename);

    void decode(BIT_FILE *input, const std::string &filename);

    void setWorkingModeDependFileExt(const QString &ext);

    QString humanFileSize(const uint_fast32_t &bytes,
                          const bool &si,
                          const uint_fast32_t &precision);

    void setElapsedTime(std::chrono::time_point<std::chrono::high_resolution_clock> start,
                        std::chrono::time_point<std::chrono::high_resolution_clock> end);
};


#endif //ZFCD_MAINWINDOW_H
