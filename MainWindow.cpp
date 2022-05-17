#include "MainWindow.h"

const uint_fast32_t END_OF_STREAM = 256; /* Маркер конца потока */
const uint_fast32_t ESCAPE = 257;        /* Маркер начала ESCAPE последовательности */
const uint_fast32_t SYMBOL_COUNT = 258;  /* Максимально возможное количество листьев дерева (256+2 маркера) */

#define NODE_TABLE_COUNT ((SYMBOL_COUNT * 2) - 1)
#define ROOT_NODE 0
const uint_fast32_t MAX_WEIGHT = 0x8000; /* Вес корня, при котором начинается масштабирование веса */

#define PACIFIER_COUNT 2047 // Шаг индикатора выполнения для периодического консольного вывода

struct Node {
    /*
     * Узел дерева
     * */

    uint_fast32_t weight; /* Вес символа */
    uint_fast32_t parent; /* Номер родителя в массиве узлов */
    bool child_is_leaf;   /* Флаг листа (TRUE, если лист) */
    uint_fast32_t child;
};

struct Tree {
    /*
     * Структура дерева
     * */

    uint_fast32_t leaf[SYMBOL_COUNT]; /* Массив листьев дерева */
    uint_fast32_t next_free_node; /* Номер следующего свободного элемента массива листьев */
    std::array<Node, NODE_TABLE_COUNT> nodes; /* Массив узлов */
};

/*
 * Побитовый файловый доступ
 * */

BIT_FILE *open_input_bit_file(const char *name) {
    /*
     * Открытие файла для побитового ввода
     * */
    BIT_FILE *compressed_file;

    compressed_file = (BIT_FILE *)
            calloc(1, sizeof(BIT_FILE));
    if (compressed_file == NULL)
        return (compressed_file);
    compressed_file->file = fopen(name, "rb");
    compressed_file->rack = 0;
    compressed_file->mask = 0x80;
    compressed_file->pacifier_counter = 0;
    return (compressed_file);
}

BIT_FILE *open_output_bit_file(const char *name) {
    /*
     * Открытие файла для побитового вывода
     * */
    BIT_FILE *compressed_file;

    compressed_file = (BIT_FILE *)
            calloc(1, sizeof(BIT_FILE));
    if (compressed_file == NULL)
        return (compressed_file);
    compressed_file->file = fopen(name, "wb");
    compressed_file->rack = 0;
    compressed_file->mask = 0x80;
    compressed_file->pacifier_counter = 0;
    return (compressed_file);
}

void output_bit(BIT_FILE *compressed_file, int bit) {
    /*
     * Вывод одного бита в файл
     * */

    if (bit)
        compressed_file->rack |= compressed_file->mask;
    compressed_file->mask >>= 1;
    if (compressed_file->mask == 0) {
        if (putc(compressed_file->rack, compressed_file->file) !=
            compressed_file->rack)
            throw std::runtime_error("Error on output_bit!\n");
        else if ((compressed_file->pacifier_counter++ &
                  PACIFIER_COUNT) == 0)
            putc('.', stdout);
        compressed_file->rack = 0;
        compressed_file->mask = 0x80;
    }
}

void output_bits(BIT_FILE *compressed_file, unsigned long code, int bit_count) {
    /*
     * Вывод bit_count бит в файл
     * */

    unsigned long mask;

    mask = 1L << (bit_count - 1);
    while (mask != 0) {
        if (mask & code)
            compressed_file->rack |= compressed_file->mask;
        compressed_file->mask >>= 1;
        if (compressed_file->mask == 0) {
            if (putc(compressed_file->rack, compressed_file->file) !=
                compressed_file->rack)
                throw std::runtime_error("Error on output_bits!\n");

            else if ((compressed_file->pacifier_counter++ &
                      PACIFIER_COUNT) == 0)
                putc('.', stdout);
            compressed_file->rack = 0;
            compressed_file->mask = 0x80;
        }
        mask >>= 1;
    }
}

int input_bit(BIT_FILE *compressed_file) {
    /*
     * Ввод одного бита в файл
     * */
    int value;

    if (compressed_file->mask == 0x80) {
        compressed_file->rack = getc(compressed_file->file);
        if (compressed_file->rack == EOF)
            throw std::runtime_error("Error on input_bit!\n");

        if ((compressed_file->pacifier_counter++ &
             PACIFIER_COUNT) == 0)
            putc('.', stdout);
    }
    value = compressed_file->rack & compressed_file->mask;
    compressed_file->mask >>= 1;
    if (compressed_file->mask == 0)
        compressed_file->mask = 0x80;
    return (value ? 1 : 0);
}

unsigned long input_bits(BIT_FILE *compressed_file, int bit_count) {
    /*
     * Ввод bit_count бит в файл
     * */
    uint_fast32_t mask;
    uint_fast32_t return_value;

    mask = 1L << (bit_count - 1);
    return_value = 0;
    while (mask != 0) {
        if (compressed_file->mask == 0x80) {
            compressed_file->rack = getc(compressed_file->file);
            if (compressed_file->rack == EOF)
                throw std::runtime_error("Error on input_bits!\n");

            if ((compressed_file->pacifier_counter++ &
                 PACIFIER_COUNT) == 0)
                putc('.', stdout);
        }
        if (compressed_file->rack & compressed_file->mask)
            return_value |= mask;
        mask >>= 1;
        compressed_file->mask >>= 1;
        if (compressed_file->mask == 0)
            compressed_file->mask = 0x80;
    }

    return return_value;
}

void close_input_bit_file(BIT_FILE *compressed_file) {
    /*
     * Закрытие файла, открытого для побитового ввода
     * */

    fclose(compressed_file->file);
    free((char *) compressed_file);
}

void close_output_bit_file(BIT_FILE *compressed_file) {
    /*
     * Закрытие файла, открытого для побитового вывода
     * */

    if (compressed_file->mask != 0x80)
        if (putc(compressed_file->rack, compressed_file->file) !=
            compressed_file->rack)
            throw std::runtime_error("Error on close compressed file.\n");
    fflush(compressed_file->file);
    fclose(compressed_file->file);
    free((char *) compressed_file);
}

/*
 * Сервисные функции
 * */

uint_fast32_t file_size(const char *name) {
    /*
     * Возвращает размер указанного файла в байтах
     * */
    std::ifstream f(name, std::ios::ate);
    if (!f.is_open())
        throw std::runtime_error("Can't open file\n");

    uint_fast32_t size = f.tellg();

    f.close();

    return size;
}

void print_results(char *input, char *output) {
    /*
     * Вывод результатов
     * */

    uint_fast32_t input_size = file_size(input);
    if (input_size == 0)
        input_size = 1;

    printf("\nSource filesize:\t%ld\n", input_size);

    uint_fast32_t output_size = file_size(output);
    printf("Target Filesize:\t%ld\n", output_size);

    int ratio = 100 - (int) (output_size * 100L / input_size);
    printf("Compression ratio:\t\t%d%%\n", ratio);
}

void print_fatal_error(char *fmt) {
    /*
     * Вывод сообщения об ошибке
     * */
    printf("Fatal error: ");
    printf("%s", fmt);
    exit(-1);
}

void help() {
    /*
     * Вывод подсказки по использованию программой
     * */
    printf("HuffAdapt e(encoding)|d(decoding) input output\n");
}

Tree model_tree; // Модель кодирования

/*
 * Основные функции адаптивного алгоритма Хаффмана
 * */

void initialize_tree(Tree *tree);

void encode_symbol(Tree *tree, unsigned int c, BIT_FILE *output);

int decode_symbol(Tree *tree, BIT_FILE *input);

void update_model(Tree *tree, int c);

void rebuild_tree(Tree *tree);

void swap_nodes(Tree *tree, int i, int j);

void add_new_node(Tree *tree, int c);

void MainWindow::encode(FILE *input, const std::string &filename) {
    auto source_file_size = file_size(filename.c_str());
    sourceFileSizeValue->setText(humanFileSize(source_file_size, true, 2));

    std::filesystem::path path(filename);
    auto outFilename = (std::stringstream() << path.stem().string() << ".ahf").str().c_str();

    BIT_FILE *output = open_output_bit_file(outFilename);
    if (output == nullptr)
        throw std::runtime_error("Error open target file.\n");

    auto fileExtToEncode = path.extension().string().erase(0, 1);
    for (unsigned char c: fileExtToEncode)
        output_bits(output, c, 8);
    output_bits(output, 0, 8);

    uint_fast32_t processed_bytes = 0;
    int c;

    initialize_tree(&model_tree);
    while ((c = getc(input)) != EOF) {
        ++processed_bytes;
        progressBar->setValue(ceil(processed_bytes * 100.0 / source_file_size));

        encode_symbol(&model_tree, c, output);
        update_model(&model_tree, c);
    }
    encode_symbol(&model_tree, END_OF_STREAM, output);

    close_output_bit_file(output);

    auto created_file_size = file_size(outFilename);
    receivedFileSizeValue->setText(humanFileSize(created_file_size, true, 2));
    auto ratio = ceil(created_file_size * 100.0 / source_file_size);
    compressionRatioTextValue->setText(QString::number(ratio) + " %");
}

void MainWindow::decode(BIT_FILE *input, const std::string &filename) {
    auto source_file_size = file_size(filename.c_str());
    sourceFileSizeValue->setText(humanFileSize(source_file_size, true, 2));

    std::filesystem::path path(filename);

    std::string ext;
    unsigned char ch;
    while (true) {
        ch = input_bits(input, 8);

        if (ch == '\0')
            break;

        ext += ch;
    }

    auto outFilename = (std::stringstream() << path.stem().string() << "." << ext).str().c_str();

    FILE *output = fopen(outFilename, "wb");
    if (output == nullptr)
        throw std::runtime_error("Error open target file.\n");

    uint_fast32_t processed_bytes = 0;
    int c;

    initialize_tree(&model_tree);
    while ((c = decode_symbol(&model_tree, input)) != END_OF_STREAM) {
        ++processed_bytes;
        progressBar->setValue(ceil(processed_bytes * 100.0 / source_file_size));

        if (putc(c, output) == EOF)
            throw std::runtime_error("Error on output.\n");
        update_model(&model_tree, c);
    }

    fflush(output);
    fclose(output);

    auto created_file_size = file_size(outFilename);
    receivedFileSizeValue->setText(humanFileSize(created_file_size, true, 2));
    auto ratio = ceil(created_file_size * 100.0 / source_file_size);
    compressionRatioTextValue->setText(QString::number(ratio) + " %");
}

void initialize_tree(Tree *tree) {
    /*
     * Функция инициализации дерева.
     * Перед началом работы алгоритма дерево кодирования
     * инициализируется двумя специальными (не ASCII) символами:
     * ESCAPE и END_OF_STREAM.
     * Также инициализируется корень дерева.
     * Все листья инициализируются -1, так как они еще
     * не присутствуют в дереве кодирования.
     * */

    tree->nodes[ROOT_NODE].child = ROOT_NODE + 1;
    tree->nodes[ROOT_NODE].child_is_leaf = false;
    tree->nodes[ROOT_NODE].weight = 2;
    tree->nodes[ROOT_NODE].parent = -1;

    tree->nodes[ROOT_NODE + 1].child = END_OF_STREAM;
    tree->nodes[ROOT_NODE + 1].child_is_leaf = true;
    tree->nodes[ROOT_NODE + 1].weight = 1;
    tree->nodes[ROOT_NODE + 1].parent = ROOT_NODE;
    tree->leaf[END_OF_STREAM] = ROOT_NODE + 1;

    tree->nodes[ROOT_NODE + 2].child = ESCAPE;
    tree->nodes[ROOT_NODE + 2].child_is_leaf = true;
    tree->nodes[ROOT_NODE + 2].weight = 1;
    tree->nodes[ROOT_NODE + 2].parent = ROOT_NODE;
    tree->leaf[ESCAPE] = ROOT_NODE + 2;

    tree->next_free_node = ROOT_NODE + 3;

    for (int i = 0; i < END_OF_STREAM; ++i)
        tree->leaf[i] = -1;
}

void encode_symbol(Tree *tree, unsigned int c, BIT_FILE *output) {
    /*
     * Преобразует входной символ в последовательность
     * битов на основе текущего состояния дерева кодирования.
     * Некоторое неудобство состоит в том, что, обходя дерево
     * от листа к корню, мы получаем последовательность битов
     * в обратном порядке, и поэтому необходимо аккумулировать биты
     * в INTEGER переменной и выдавать их после того, как обход
     * дерева закончен.
     * */

    unsigned long code = 0;
    unsigned long current_bit = 1;
    int code_size = 0;
    int current_node = tree->leaf[c];

    if (current_node == -1)
        current_node = tree->leaf[ESCAPE];

    while (current_node != ROOT_NODE) {
        if ((current_node & 1) == 0)
            code |= current_bit;
        current_bit <<= 1;
        ++code_size;
        current_node = tree->nodes[current_node].parent;
    }

    output_bits(output, code, code_size);

    if (tree->leaf[c] == -1) {
        output_bits(output, (unsigned long) c, 8);
        add_new_node(tree, c);
    }
}

int decode_symbol(Tree *tree, BIT_FILE *input) {
    /*
     * Процедура декодирования очень проста. Начиная от корня, мы
     * обходим дерево, пока не дойдем до листа. Затем проверяем
     * не прочитали ли мы ESCAPE код. Если да, то следующие 8 битов
     * соответствуют незакодированному символу, который немедленно
     * считывается и добавляется к таблице.
     * */

    int current_node;
    int c;

    current_node = ROOT_NODE;
    while (!tree->nodes[current_node].child_is_leaf) {
        current_node = tree->nodes[current_node].child;
        current_node += input_bit(input);
    }
    c = tree->nodes[current_node].child;
    if (c == ESCAPE) {
        c = (int) input_bits(input, 8);
        add_new_node(tree, c);
    }
    return (c);
}

void update_model(Tree *tree, int c) {
    /*
     * Процедура обновления модели кодирования для данного символа.
     * */

    int current_node;
    int new_node;

    if (tree->nodes[ROOT_NODE].weight == MAX_WEIGHT)
        rebuild_tree(tree);

    current_node = tree->leaf[c];
    while (current_node != -1) {
        tree->nodes[current_node].weight++;

        for (new_node = current_node; new_node > ROOT_NODE; new_node--)
            if (tree->nodes[new_node - 1].weight >=
                tree->nodes[current_node].weight)
                break;

        if (current_node != new_node) {
            swap_nodes(tree, current_node, new_node);
            current_node = new_node;
        }

        current_node = tree->nodes[current_node].parent;
    }
}

void rebuild_tree(Tree *tree) {
    /*
     * Процедура перестроения дерева вызывается тогда, когда
     * вес корня дерева достигает пороговой величины. Она
     * начинается с простого деления весов узлов на 2. Но из-за
     * ошибок округления при этом может быть нарушено свойство
     * упорядоченности дерева кодирования, и необходимы
     * дополнительные усилия, чтобы привести его в корректное
     * состояние.
     * */

    int i;
    int j;
    int k;
    unsigned int weight;

    printf("R");
    j = tree->next_free_node - 1;
    for (i = j; i >= ROOT_NODE; i--) {
        if (tree->nodes[i].child_is_leaf) {
            tree->nodes[j] = tree->nodes[i];
            tree->nodes[j].weight =
                    (tree->nodes[j].weight + 1) / 2;
            j--;
        }
    }

    for (i = tree->next_free_node - 2; j >= ROOT_NODE; i -= 2, j--) {
        k = i + 1;
        tree->nodes[j].weight =
                tree->nodes[i].weight + tree->nodes[k].weight;
        weight = tree->nodes[j].weight;
        tree->nodes[j].child_is_leaf = 0;
        for (k = j + 1; weight < tree->nodes[k].weight; k++);
        k--;
        memmove(&tree->nodes[j], &tree->nodes[j + 1],
                (k - j) * sizeof(struct Node));
        tree->nodes[k].weight = weight;
        tree->nodes[k].child = i;
        tree->nodes[k].child_is_leaf = 0;
    }

    for (i = tree->next_free_node - 1; i >= ROOT_NODE; i--) {
        if (tree->nodes[i].child_is_leaf) {
            k = tree->nodes[i].child;
            tree->leaf[k] = i;
        } else {
            k = tree->nodes[i].child;
            tree->nodes[k].parent =
            tree->nodes[k + 1].parent = i;
        }
    }
}

void swap_nodes(Tree *tree, int i, int j) {
    /*
     * Процедура перестановки узлов дерева вызывается тогда, когда
     * очередное увеличение веса узла привело к нарушению свойства
     * упорядоченности.
     * */

    Node temp{};

    if (tree->nodes[i].child_is_leaf)
        tree->leaf[tree->nodes[i].child] = j;
    else {
        tree->nodes[tree->nodes[i].child].parent = j;
        tree->nodes[tree->nodes[i].child + 1].parent = j;
    }
    if (tree->nodes[j].child_is_leaf)
        tree->leaf[tree->nodes[j].child] = i;
    else {
        tree->nodes[tree->nodes[j].child].parent = i;
        tree->nodes[tree->nodes[j].child + 1].parent = i;
    }
    temp = tree->nodes[i];
    tree->nodes[i] = tree->nodes[j];
    tree->nodes[i].parent = temp.parent;
    temp.parent = tree->nodes[j].parent;
    tree->nodes[j] = temp;
}

void add_new_node(Tree *tree, int c) {
    /*
     * Для добавления самый легкий узел дерева разбивается на 2,
     * один из которых и есть тот новый узел.
     * Новому узлу присваивается вес 0, который будет изменен потом,
     * при нормальном процессе обновления дерева.
     * */

    uint_fast32_t lightest_node = tree->next_free_node - 1;
    uint_fast32_t new_node = tree->next_free_node;
    uint_fast32_t zero_weight_node = tree->next_free_node + 1;
    tree->next_free_node += 2;

    tree->nodes[new_node] = tree->nodes[lightest_node];
    tree->nodes[new_node].parent = lightest_node;
    tree->leaf[tree->nodes[new_node].child] = new_node;

    tree->nodes[lightest_node].child = new_node;
    tree->nodes[lightest_node].child_is_leaf = false;

    tree->nodes[zero_weight_node].child = c;
    tree->nodes[zero_weight_node].child_is_leaf = true;
    tree->nodes[zero_weight_node].weight = 0;
    tree->nodes[zero_weight_node].parent = lightest_node;
    tree->leaf[c] = zero_weight_node;
}

/*
 * Графический интерфейс пользователя
 * */

MainWindow::MainWindow() {
    setFixedSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    setWindowTitle(WINDOW_TITLE);

    centralWidget = new QGraphicsView(this);
    centralWidget->setFixedSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    centralLayout = new QGridLayout(centralWidget);

    fileLabel = new QLabel(tr("File: "));
    centralLayout->addWidget(fileLabel, 0, 0);
    selectedFileName = new QLabel("not selected");
    centralLayout->addWidget(selectedFileName, 0, 1);

    selectFileButton = new QPushButton(tr("Select"));
    centralLayout->addWidget(selectFileButton, 1, 0, 1, 2);
    connect(selectFileButton, SIGNAL(clicked(bool)), this, SLOT(openFileDialog()));

    startButton = new QPushButton(tr("Start"));
    centralLayout->addWidget(startButton, 2, 0);

    closeButton = new QPushButton(tr("Close"));
    centralLayout->addWidget(closeButton, 2, 1);
    connect(closeButton, &QPushButton::clicked, this,
            [this]() {
                this->close();
            });

    elapsedTimeLabel = new QLabel(tr("Elapsed time: "));
    centralLayout->addWidget(elapsedTimeLabel, 3, 0);

    elapsedTimeTextValue = new QLabel(tr("00:00:00:00"));
    centralLayout->addWidget(elapsedTimeTextValue, 3, 1);

    sourceFileSize = new QLabel(tr("Source size: "));
    centralLayout->addWidget(sourceFileSize, 4, 0);
    sourceFileSizeValue = new QLabel(tr("0 B"));
    centralLayout->addWidget(sourceFileSizeValue, 4, 1);

    receivedFileSize = new QLabel(tr("Received size: "));
    centralLayout->addWidget(receivedFileSize, 5, 0);
    receivedFileSizeValue = new QLabel(tr("0 B"));
    centralLayout->addWidget(receivedFileSizeValue, 5, 1);

    compressionRatioLabel = new QLabel(tr("Compression ratio: "));
    centralLayout->addWidget(compressionRatioLabel, 6, 0);

    compressionRatioTextValue = new QLabel(tr("0 %"));
    centralLayout->addWidget(compressionRatioTextValue, 6, 1);

    progressBar = new QProgressBar;
    progressBar->setMaximum(100);
    centralLayout->addWidget(progressBar, 7, 0, 1, 2);
    QPalette p = palette();
    p.setColor(QPalette::Highlight, Qt::darkCyan);
    setPalette(p);

    connectMethodDependMode();
}

MainWindow::~MainWindow() {
    delete sourceFileSize;
    delete sourceFileSizeValue;
    delete receivedFileSize;
    delete receivedFileSizeValue;
    delete progressBar;
    delete compressionRatioTextValue;
    delete compressionRatioLabel;
    delete elapsedTimeTextValue;
    delete elapsedTimeLabel;
    delete selectedFileName;
    delete selectFileButton;
    delete closeButton;
    delete centralLayout;
    delete centralWidget;
}

void MainWindow::openFileDialog() {
    auto dialog = QFileDialog(this);
    dialog.setFileMode(QFileDialog::AnyFile);

    auto filename = dialog.getOpenFileName();
    QFileInfo fi(filename);
    filename = fi.fileName();
    selectedFileName->setText(filename.isEmpty() ? "Not Selected" : filename);
    auto ext = fi.completeSuffix();
    setWorkingModeDependFileExt(ext);
}

void MainWindow::connectMethodDependMode() {
    startButton->disconnect();

    if (MODE == ENCODE)
        connect(startButton, &QPushButton::clicked, this,
                [this]() {
                    auto start = std::chrono::high_resolution_clock::now();

                    FILE *input = fopen(selectedFileName->text().toStdString().c_str(), "rb");
                    if (input == nullptr)
                        throw std::runtime_error("Error open source file.\n");

                    encode(input, selectedFileName->text().toStdString());
                    fclose(input);

                    auto end = std::chrono::high_resolution_clock::now();
                    setElapsedTime(start, end);
                });
    else
        connect(startButton, &QPushButton::clicked, this,
                [this]() {
                    auto start = std::chrono::high_resolution_clock::now();

                    BIT_FILE *input = open_input_bit_file(selectedFileName->text().toStdString().c_str());
                    if (input == nullptr)
                        throw std::runtime_error("Error open source file.\n");

                    decode(input, selectedFileName->text().toStdString());
                    close_input_bit_file(input);

                    auto end = std::chrono::high_resolution_clock::now();
                    setElapsedTime(start, end);
                });
}

void MainWindow::setWorkingModeDependFileExt(const QString &ext) {
    if (ext == "ahf")
        MODE = DECODE;
    else
        MODE = ENCODE;

    connectMethodDependMode();
}

QString MainWindow::humanFileSize(const uint_fast32_t &bytes,
                                  const bool &si,
                                  const uint_fast32_t &precision) {
    double size = bytes;
    uint_fast32_t thresh = si ? 1000 : 1024;

    if (std::abs(size) < thresh) {
        return QString::number(size) + " B";
    }

    std::array<std::array<QString, 8>, 2> suffixes{
            "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB",
            "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"
    };

    uint_fast32_t u = -1;
    const uint_fast32_t r = pow(10, precision);

    do {
        size /= thresh;
        ++u;
    } while (std::round(std::abs(size) * r) / r >= thresh && u < 7);


    return QString::number(size) + " " + suffixes[si][u];
}

void MainWindow::setElapsedTime(std::chrono::time_point<std::chrono::high_resolution_clock> start,
                                std::chrono::time_point<std::chrono::high_resolution_clock> end) {
    std::chrono::duration<double> diff = end - start;
    int ms = diff.count() * 1000;
    int x = ms / 1000;
    int s = x % 60;
    x /= 60;
    int m = x % 60;
    x /= 60;
    int h = x % 24;

    elapsedTimeTextValue->setText(
            QString::number(h) + ":" +
            QString::number(m) + ":" +
            QString::number(s) + ":" +
            QString::number(ms % 1000)
    );
}
