#include "mainwindow.h"

TapeDisplay::TapeDisplay(QWidget* parent)
    : QWidget(parent),
    targetHead(0),
    visualHead(0.0),
    tapeOffset(0.0),
    cellW(50),
    speed(1.0) {
    setMinimumHeight(120);
    setAttribute(Qt::WA_StaticContents);
    setFocusPolicy(Qt::StrongFocus);
    renderTimer = new QTimer(this);
    connect(renderTimer, &QTimer::timeout, this, &TapeDisplay::animationTick);
    renderTimer->start(16);
}

void TapeDisplay::setCellWidth(int w) { cellW = w; }
int TapeDisplay::getCellWidth() const { return cellW; }
void TapeDisplay::setSpeed(double s) { speed = s; }
void TapeDisplay::setHeadPos(int pos) { targetHead = pos; }
void TapeDisplay::updateData(const QMap<int, QChar>& d) { data = d; }
int TapeDisplay::getHeadPos() const { return targetHead; }

void TapeDisplay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int startIdx = std::floor(-tapeOffset / cellW);
    int visibleCells = width() / cellW + 3;
    for (int i = 0; i < visibleCells; i++) {
        int idx = startIdx + i;
        int x = static_cast<int>(tapeOffset + idx * cellW);
        QRect rect(x, 20, cellW - 2, 60);
        p.setBrush(QColor("#2b2b2b"));
        p.setPen(QColor("#666666"));
        p.drawRect(rect);
        p.setPen(Qt::white);
        p.drawText(rect, Qt::AlignCenter, QString(data.value(idx, '_')));
    }
    double px = tapeOffset + visualHead * cellW + cellW / 2.0;
    QPolygon arrow;
    arrow << QPoint(px - cellW / 4, 80) << QPoint(px + cellW / 4, 80)
          << QPoint(px, 65);
    p.setBrush(Qt::white);
    p.setPen(Qt::white);
    p.drawPolygon(arrow);
}

void TapeDisplay::mousePressEvent(QMouseEvent* event) {
    int pos = std::floor((event->pos().x() - tapeOffset) / cellW);
    targetHead = pos;
    emit cellClicked(pos);
    emit headPositionChanged(pos);
}

void TapeDisplay::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Left && targetHead > -1000) {
        targetHead--;
        emit headPositionChanged(targetHead);
    } else if (event->key() == Qt::Key_Right && targetHead < 1000) {
        targetHead++;
        emit headPositionChanged(targetHead);
    }
    QWidget::keyPressEvent(event);
}

void TapeDisplay::animationTick() {
    double factor = 0.2 * speed;
    visualHead += (targetHead - visualHead) * factor;

    double caretX = tapeOffset + visualHead * cellW + cellW / 2.0;
    double margin = width() * 0.25;
    double newTarget = tapeOffset;

    if (caretX < margin) {
        newTarget = margin - (visualHead * cellW + cellW / 2.0);
    } else if (caretX > width() - margin) {
        newTarget = (width() - margin) - (visualHead * cellW + cellW / 2.0);
    }

    tapeOffset += (newTarget - tapeOffset) * factor;
    update();
}

MainWindow::MainWindow() {
    setWindowTitle("Turing Machine Emulator");
    resize(1000, 580);
    emptySymbol = '_';
    setupUI();
    table->setEnabled(false);
    inputStrEdit->setEnabled(false);
    blockControls(false);
    applyBWStyle();
    table->setColumnCount(1);
    table->setHorizontalHeaderItem(0, new QTableWidgetItem(QString(emptySymbol)));
    table->setRowCount(1);
    table->setVerticalHeaderItem(0, new QTableWidgetItem("q0"));
    states.append("q0");
    table->setCellWidget(0, 0, createCellEditor());
    stepTimer = new QTimer(this);
    connect(stepTimer, &QTimer::timeout, this,
            [this]() { if (isRunning) processStep(); });
}

void MainWindow::onSetAlphabets() {
    QString a1 = alphEdit->text(), a2 = extraAlphEdit->text();
    if (!validateAlphabet(a1, "Tape alphabet") ||
        !validateAlphabet(a2, "Extra symbols")) {
        return;
    }

    QSet<QChar> newBase, newExtra;
    for (const QChar& c : a1) {
        if (!c.isSpace() && c != emptySymbol) {
            newBase.insert(c);
        }
    }
    for (const QChar& c : a2) {
        if (!c.isSpace() && c != emptySymbol) {
            newExtra.insert(c);
        }
    }

    QSet<QChar> overlap = newBase;
    overlap.intersect(newExtra);
    if (!overlap.isEmpty()) {
        QMessageBox::critical(this, "Error", "Overlap symbols!");
        return;
    }
    if (isRunning) return;
    baseAlphabet = newBase;
    extraAlphabetSet = newExtra;
    fullAlphabet = newBase + newExtra;
    updateTable();
    table->setEnabled(true);
    inputStrEdit->setEnabled(true);
}

void MainWindow::onSetString() {
    QString s = inputStrEdit->text();
    for (const QChar& c : s) {
        if (!baseAlphabet.contains(c) && c != emptySymbol) {
            QMessageBox::critical(this, "Error", "Invalid char");
            return;
        }
    }
    initialStr = s;
    initialHeadPos = headPos;

    tapeMap.clear();
    for (int i = 0; i < initialStr.length(); i++) {
        tapeMap[initialHeadPos + i] = initialStr[i];
    }

    double savedOffset = tapeView->tapeOffset;

    tapeView->updateData(tapeMap);
    tapeView->setHeadPos(headPos);

    tapeView->tapeOffset = savedOffset;
    tapeView->update();

    setStrBtn->setEnabled(false);
    startBtn->setEnabled(true);
}

void MainWindow::onStart() {
    if (isRunning) {
        return;
    }
    if (!hasHaltState()) {
        QMessageBox::critical(this, "Error", "No halt state");
        return;
    }
    isRunning = true;
    stepCounter = 0;
    startBtn->setDown(true);
    blockControls(true);
    currentState = initialState;
    highlightState(currentState);
    stepTimer->start(static_cast<int>(1000 / tapeView->speed));
    processStep();
}

void MainWindow::onStop() {
    isRunning = false;
    stepTimer->stop();
    startBtn->setDown(false);
    resetTMState();
    highlightState("");
    blockControls(false);
    stepBtn->setEnabled(true);
    setStrBtn->setEnabled(true);
    startBtn->setEnabled(true);
}

void MainWindow::onStep() {
    if (!isRunning) processStep();
}

void MainWindow::onReset() {
    onStop();
}

void MainWindow::onFaster() {
    tapeView->setSpeed(std::min(5.0, tapeView->speed + 0.5));
    if (isRunning) {
        stepTimer->stop();
        stepTimer->start(static_cast<int>(1000 / tapeView->speed));
    }
}

void MainWindow::onSlower() {
    tapeView->setSpeed(std::max(0.1, tapeView->speed - 0.5));
    if (isRunning) {
        stepTimer->stop();
        stepTimer->start(static_cast<int>(1000 / tapeView->speed));
    }
}

void MainWindow::onCellClicked(int pos) {
    headPos = pos;
    tapeView->setHeadPos(pos);
    tapeView->setFocus();
}

void MainWindow::onHeadPosSync(int pos) {
    headPos = pos;
}

bool MainWindow::validateAlphabet(const QString& str,
                                  const QString& fieldName) {
    QSet<QChar> chars;
    for (const QChar& c : str) {
        if (c.isSpace()) {
            continue;
        }
        if (chars.contains(c)) {
            QMessageBox::critical(this, "Error", "Duplicate");
            return false;
        }
        chars.insert(c);
    }
    return true;
}

bool MainWindow::validateCommand(const QString& cmd, QChar& writeSym,
                                 QString& direction, QString& nextState) {
    if (cmd.isEmpty()) return true;
    QStringList p = cmd.split(' ', Qt::SkipEmptyParts);
    if (p.size() != 3) {
        QMessageBox::critical(this, "Error", "Format Error");
        return false;
    }
    if (p[0].length() != 1 || !fullAlphabet.contains(p[0][0])) {
        QMessageBox::critical(this, "Error", "Symbol Error");
        return false;
    }
    writeSym = p[0][0];
    QString d = p[1].toUpper();
    if (d != "L" && d != "R" && d != "N") {
        QMessageBox::critical(this, "Error", "Dir Error");
        return false;
    }
    direction = d;
    if (!states.contains(p[2])) {
        QMessageBox::critical(this, "Error", "State Error");
        return false;
    }
    nextState = p[2];
    return true;
}

void MainWindow::setupUI() {
    QWidget* central = new QWidget(this);
    QVBoxLayout* mainLay = new QVBoxLayout(central);
    mainLay->setSpacing(6);
    mainLay->setContentsMargins(8, 8, 8, 8);

    QHBoxLayout* topLay = new QHBoxLayout();
    alphEdit = new QLineEdit();
    extraAlphEdit = new QLineEdit();
    setAlphBtn = new QPushButton("Set");
    topLay->addWidget(new QLabel("Alph:"));
    topLay->addWidget(alphEdit);
    topLay->addWidget(new QLabel("Extra:"));
    topLay->addWidget(extraAlphEdit);
    topLay->addWidget(setAlphBtn);
    mainLay->addLayout(topLay);

    table = new QTableWidget(1, 1);
    table->setAlternatingRowColors(false);
    mainLay->addWidget(new QLabel("Program"));
    mainLay->addWidget(table);

    QHBoxLayout* btnLay = new QHBoxLayout();
    addStateBtn = new QPushButton("+");
    remStateBtn = new QPushButton("-");
    btnLay->addWidget(addStateBtn);
    btnLay->addWidget(remStateBtn);
    mainLay->addLayout(btnLay);

    inputStrEdit = new QLineEdit();
    setStrBtn = new QPushButton("Set Str");
    QHBoxLayout* strLay = new QHBoxLayout();
    strLay->addWidget(inputStrEdit);
    strLay->addWidget(setStrBtn);
    mainLay->addLayout(strLay);

    scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setMaximumHeight(135);
    tapeView = new TapeDisplay();
    scrollArea->setWidget(tapeView);
    mainLay->addWidget(scrollArea);

    QHBoxLayout* ctrlLay = new QHBoxLayout();
    startBtn = new QPushButton("Start");
    stopBtn = new QPushButton("Stop");
    stepBtn = new QPushButton("Step");
    resetBtn = new QPushButton("Reset");
    fasterBtn = new QPushButton("F+");
    slowerBtn = new QPushButton("F-");
    ctrlLay->addWidget(startBtn);
    ctrlLay->addWidget(stopBtn);
    ctrlLay->addWidget(stepBtn);
    ctrlLay->addWidget(resetBtn);
    ctrlLay->addWidget(fasterBtn);
    ctrlLay->addWidget(slowerBtn);
    mainLay->addLayout(ctrlLay);

    connect(setAlphBtn, &QPushButton::clicked, this, &MainWindow::onSetAlphabets);
    connect(setStrBtn, &QPushButton::clicked, this, &MainWindow::onSetString);
    connect(startBtn, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(stepBtn, &QPushButton::clicked, this, &MainWindow::onStep);
    connect(resetBtn, &QPushButton::clicked, this, &MainWindow::onReset);
    connect(fasterBtn, &QPushButton::clicked, this, &MainWindow::onFaster);
    connect(slowerBtn, &QPushButton::clicked, this, &MainWindow::onSlower);
    connect(addStateBtn, &QPushButton::clicked, this, [this]() {
        int n = states.size();
        QString nm = "q" + QString::number(n);
        states.append(nm);
        table->insertRow(n);
        table->setVerticalHeaderItem(n, new QTableWidgetItem(nm));
        for (int c = 0; c < table->columnCount(); c++) {
            table->setCellWidget(n, c, createCellEditor());
        }
    });
    connect(remStateBtn, &QPushButton::clicked, this, [this]() {
        if (!states.isEmpty()) {
            states.removeLast();
            table->removeRow(table->rowCount() - 1);
        }
    });
    connect(tapeView, &TapeDisplay::cellClicked, this, &MainWindow::onCellClicked);
    connect(tapeView, &TapeDisplay::headPositionChanged, this, &MainWindow::onHeadPosSync);
    setCentralWidget(central);
}

void MainWindow::applyBWStyle() {
    setStyleSheet(
        "QWidget { background-color: #1e1e1e; color: #e0e0e0; } "
        "QLineEdit, QTableWidget { background-color: #1e1e1e; color: #ffffff; "
        "border: 1px solid #555555; selection-background-color: #0078d7; } "
        "QTableWidget::item { border: 1px solid #444444; background-color: #1e1e1e; } "
        "QTableWidget::item:selected { background-color: #3a3a3a; } "
        "QHeaderView::section { background-color: #2b2b2b; color: #ffffff; "
        "border: 1px solid #555555; font-weight: bold; padding: 2px; } "
        "QPushButton { background-color: #3c3c3c; color: #ffffff; "
        "border: 1px solid #555555; padding: 5px; min-width: 60px; } "
        "QPushButton:hover { background-color: #505050; } "
        "QPushButton:pressed { background-color: #222222; } "
        "QLabel { color: #aaaaaa; font-weight: bold; } ");
}

void MainWindow::updateTable() {
    table->setRowCount(states.isEmpty() ? 1 : states.size());
    table->setColumnCount(1 + fullAlphabet.size());
    QStringList syms;
    syms.append(QString(emptySymbol));
    for (const QChar& c : fullAlphabet) {
        syms.append(c);
    }
    table->setHorizontalHeaderLabels(syms);
    if (states.isEmpty()) states.append("q0");
    table->setVerticalHeaderLabels(states);
    for (int r = 0; r < table->rowCount(); r++) {
        for (int c = 0; c < table->columnCount(); c++) {
            if (!table->cellWidget(r, c)) {
                table->setCellWidget(r, c, createCellEditor());
            }
        }
    }
}

QLineEdit* MainWindow::createCellEditor() {
    QLineEdit* le = new QLineEdit();
    le->setAlignment(Qt::AlignCenter);
    le->setPlaceholderText("sym dir state");
    le->setStyleSheet("background:#1e1e1e; color:white; border:none;");
    return le;
}

void MainWindow::resetTMState() {
    tapeMap.clear();
    for (int i = 0; i < initialStr.length(); i++) {
        tapeMap[initialHeadPos + i] = initialStr[i];
    }
    headPos = initialHeadPos;
    currentState = initialState;
    tapeView->updateData(tapeMap);
    tapeView->setHeadPos(headPos);
    tapeView->update();
}

void MainWindow::processStep() {
    stepCounter++;
    if (stepCounter > STEP_LIMIT) {
        onStop();
        QMessageBox::warning(this, "Limit", "Step limit reached. Stopped.");
        return;
    }
    QChar sym = tapeMap.value(headPos, emptySymbol);
    QString trans = getTransition(currentState, sym);
    if (trans.isEmpty()) {
        onStop();
        return;
    }
    QChar writeSym;
    QString direction, nextStateStr;
    if (!validateCommand(trans, writeSym, direction, nextStateStr)) {
        onStop();
        return;
    }
    tapeMap[headPos] = writeSym;
    tapeView->updateData(tapeMap);
    if (direction == "L") {
        headPos--;
    } else if (direction == "R") {
        headPos++;
    }
    tapeView->setHeadPos(headPos);
    currentState = nextStateStr;
    highlightState(currentState);
    if (hasHaltStateFor(currentState)) {
        onStop();
    }
}

QString MainWindow::getTransition(const QString& state, QChar sym) {
    int r = states.indexOf(state);
    if (r < 0) {
        return "";
    }
    QStringList syms;
    syms.append(QString(emptySymbol));
    for (const QChar& ch : fullAlphabet) {
        syms.append(ch);
    }
    int c = syms.indexOf(QString(sym));
    if (c < 0) {
        return "";
    }
    QLineEdit* le = qobject_cast<QLineEdit*>(table->cellWidget(r, c));
    if (le) {
        return le->text().trimmed();
    }
    return "";
}

bool MainWindow::hasHaltState() {
    for (int r = 0; r < table->rowCount(); r++) {
        for (int c = 0; c < table->columnCount(); c++) {
            QLineEdit* le = qobject_cast<QLineEdit*>(table->cellWidget(r, c));
            if (!le || le->text().trimmed().isEmpty()) {
                return true;
            }
        }
    }
    return false;
}

bool MainWindow::hasHaltStateFor(const QString& s) {
    int r = states.indexOf(s);
    if (r < 0) {
        return true;
    }
    for (int c = 0; c < table->columnCount(); c++) {
        QLineEdit* le = qobject_cast<QLineEdit*>(table->cellWidget(r, c));
        if (le && !le->text().trimmed().isEmpty()) return false;
    }
    return true;
}

void MainWindow::highlightState(const QString& state) {
    for (int r = 0; r < table->rowCount(); r++) {
        bool a = (states[r] == state);
        for (int c = 0; c < table->columnCount(); c++) {
            if (QWidget* w = table->cellWidget(r, c)) {
                if (a) {
                    w->setStyleSheet("border:1px solid #666666; background:#2b2b2b; color:white;");
                } else {
                    w->setStyleSheet("border:none; background:#1e1e1e; color:white;");
                }
            }
        }
    }
}

void MainWindow::blockControls(bool block) {
    alphEdit->setReadOnly(block);
    extraAlphEdit->setReadOnly(block);
    setAlphBtn->setEnabled(!block);
    setStrBtn->setEnabled(!block);
    startBtn->setEnabled(!block);
    stepBtn->setEnabled(!block && !isRunning);
    addStateBtn->setEnabled(!block);
    remStateBtn->setEnabled(!block);
    for (int r = 0; r < table->rowCount(); r++) {
        for (int c = 0; c < table->columnCount(); c++) {
            if (QLineEdit* le = qobject_cast<QLineEdit*>(table->cellWidget(r, c))) {
                le->setReadOnly(block);
            }
        }
    }
}