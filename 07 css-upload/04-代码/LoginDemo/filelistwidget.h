// filelistwidget.h
#ifndef FILELISTWIDGET_H
#define FILELISTWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QNetworkAccessManager>
#include <QProgressBar>

class FileListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FileListWidget(const QString &token, int userId, QWidget *parent = nullptr);
    ~FileListWidget();

private slots:
    void refreshFileList();
    void downloadFile();
    void deleteFile();
    void shareFile();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    QTableWidget *m_tableWidget;
    QNetworkAccessManager *m_networkManager;
    QProgressBar *m_progressBar;
    QString m_token;
    int m_userId;

    void fetchFileList();
    void downloadFileById(int fileId, const QString &filename);
    void deleteFileById(int fileId, int row);
    void shareFileById(int fileId);
};

#endif // FILELISTWIDGET_H