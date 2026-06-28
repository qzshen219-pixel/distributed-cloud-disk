// mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLineEdit;
class QPushButton;
class QProgressBar;
class QTextEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_selFile_clicked();
    void on_uploadBtn_clicked();
    void on_fileListBtn_clicked();

private:
    QLineEdit *m_filePath;
    QPushButton *m_selFileBtn;
    QPushButton *m_uploadBtn;
    QProgressBar *m_progressBar;
    QTextEdit *m_recordMsg;

    QString getFileMd5(const QString &path);
    void appendLog(const QString &msg);
    void setProgress(int percent);
};

#endif // MAINWINDOW_H