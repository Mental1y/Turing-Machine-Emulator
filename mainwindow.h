#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QMessageBox>
#include <QPainter>
#include <QMap>
#include <QSet>
#include <QScrollArea>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTableWidgetItem>
#include <QPolygon>

class TapeDisplay : public QWidget {
    Q_OBJECT
public:
    explicit TapeDisplay(QWidget* parent = nullptr);
    void setCellWidth(int w);
    int getCellWidth() const;
    void setSpeed(double s);
    void setHeadPos(int pos);
    void updateData(const QMap<int, QChar>& d);
    int getHeadPos() const;
    double speed;
    double tapeOffset;

signals:
    void cellClicked(int pos);
    void headPositionChanged(int pos);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void animationTick();

private:
    int targetHead;
    double visualHead;
    QMap<int, QChar> data;
    int cellW;
    QTimer* renderTimer;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow();

private slots:
    void onSetAlphabets();
    void onSetString();
    void onStart();
    void onStop();
    void onStep();
    void onReset();
    void onFaster();
    void onSlower();
    void onCellClicked(int pos);
    void onHeadPosSync(int pos);

private:
    bool validateAlphabet(const QString& str, const QString& fieldName);
    bool validateCommand(const QString& cmd, QChar& writeSym, QString& direction,
                         QString& nextState);
    void setupUI();
    void applyBWStyle();
    void updateTable();
    QLineEdit* createCellEditor();
    void resetTMState();
    void processStep();
    QString getTransition(const QString& state, QChar sym);
    bool hasHaltState();
    bool hasHaltStateFor(const QString& s);
    void highlightState(const QString& state);
    void blockControls(bool block);

    QLineEdit* alphEdit;
    QLineEdit* extraAlphEdit;
    QPushButton* setAlphBtn;
    QTableWidget* table;
    QLineEdit* inputStrEdit;
    QPushButton* setStrBtn;
    QPushButton* startBtn;
    QPushButton* stopBtn;
    QPushButton* stepBtn;
    QPushButton* resetBtn;
    QPushButton* fasterBtn;
    QPushButton* slowerBtn;
    QPushButton* addStateBtn;
    QPushButton* remStateBtn;
    TapeDisplay* tapeView;
    QScrollArea* scrollArea;

    QSet<QChar> baseAlphabet;
    QSet<QChar> extraAlphabetSet;
    QSet<QChar> fullAlphabet;
    QStringList states;
    QString initialState = "q0";
    QString currentState;
    QString nextState;
    QMap<int, QChar> tapeMap;
    int headPos = 0;
    QString initialStr;
    int initialHeadPos = 0;
    bool isRunning = false;
    QChar emptySymbol;
    QTimer* stepTimer;

    int stepCounter = 0;
    const int STEP_LIMIT = 5000;
};

#endif