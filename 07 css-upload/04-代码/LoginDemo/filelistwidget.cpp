// filelistwidget.cpp
#include "filelistwidget.h"
#include "logininstance.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QClipboard>
#include <QApplication>
#include <QPushButton>

FileListWidget::FileListWidget(const QString &token, int userId, QWidget *parent)
    : QWidget(parent), m_token(token), m_userId(userId),
    m_networkManager(new QNetworkAccessManager(this))
{
    // 设置窗口大小
    resize(850, 550);
    setMinimumSize(750, 450);

    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    m_progressBar->setFixedHeight(15);
    mainLayout->addWidget(m_progressBar);

    // 按钮工具栏
    QHBoxLayout *toolBar = new QHBoxLayout();
    toolBar->setSpacing(8);

    QPushButton *refreshBtn = new QPushButton("刷新", this);
    QPushButton *downloadBtn = new QPushButton("下载选中", this);
    QPushButton *deleteBtn = new QPushButton("删除选中", this);
    QPushButton *shareBtn = new QPushButton("分享选中", this);

    // 设置按钮样式
    QString btnStyle = "QPushButton { padding: 5px 10px; background-color: #5cb85c; color: white; border: none; border-radius: 3px; }"
                       "QPushButton:hover { background-color: #4cae4c; }";
    refreshBtn->setStyleSheet(btnStyle);
    downloadBtn->setStyleSheet(btnStyle);
    deleteBtn->setStyleSheet("QPushButton { padding: 5px 10px; background-color: #d9534f; color: white; border: none; border-radius: 3px; }"
                             "QPushButton:hover { background-color: #c9302c; }");
    shareBtn->setStyleSheet(btnStyle);

    toolBar->addWidget(refreshBtn);
    toolBar->addWidget(downloadBtn);
    toolBar->addWidget(deleteBtn);
    toolBar->addWidget(shareBtn);
    toolBar->addStretch();

    mainLayout->addLayout(toolBar);

    // 文件列表表格
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(5);

    QStringList headers;
    headers << "ID" << "文件名" << "大小(KB)" << "上传时间" << "操作";
    m_tableWidget->setHorizontalHeaderLabels(headers);

    // 设置列宽
    m_tableWidget->setColumnWidth(0, 50);
    m_tableWidget->setColumnWidth(2, 80);
    m_tableWidget->setColumnWidth(3, 140);
    m_tableWidget->setColumnWidth(4, 130);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    mainLayout->addWidget(m_tableWidget);

    // 连接信号
    connect(refreshBtn, &QPushButton::clicked, this, &FileListWidget::refreshFileList);
    connect(downloadBtn, &QPushButton::clicked, this, &FileListWidget::downloadFile);
    connect(deleteBtn, &QPushButton::clicked, this, &FileListWidget::deleteFile);
    connect(shareBtn, &QPushButton::clicked, this, &FileListWidget::shareFile);

    // 自动刷新
    refreshFileList();
}

FileListWidget::~FileListWidget()
{
}

void FileListWidget::refreshFileList()
{
    fetchFileList();
}

void FileListWidget::fetchFileList()
{
    LoginInstance *ins = LoginInstance::getInstance();
    QString url = QString("http://%1:%2/filelist?user_id=%3")
                      .arg(ins->getServerIP())
                      .arg(ins->getServerPort())
                      .arg(m_userId);

    QNetworkRequest request(url);
    request.setRawHeader("X-Auth-Token", m_token.toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull()) {
                QJsonObject obj = doc.object();
                if (obj.value("code").toString() == "000") {
                    QJsonArray files = obj.value("files").toArray();
                    m_tableWidget->setRowCount(files.size());

                    for (int i = 0; i < files.size(); i++) {
                        QJsonObject file = files[i].toObject();
                        int fileId = file.value("id").toInt();
                        QString fileName = file.value("name").toString();
                        int fileSize = file.value("size").toInt() / 1024;
                        QString uploadTime = file.value("time").toString();

                        m_tableWidget->setItem(i, 0, new QTableWidgetItem(QString::number(fileId)));
                        m_tableWidget->setItem(i, 1, new QTableWidgetItem(fileName));
                        m_tableWidget->setItem(i, 2, new QTableWidgetItem(QString::number(fileSize)));
                        m_tableWidget->setItem(i, 3, new QTableWidgetItem(uploadTime));

                        // 操作按钮容器
                        QWidget *btnContainer = new QWidget();
                        QHBoxLayout *btnLayout = new QHBoxLayout(btnContainer);
                        btnLayout->setContentsMargins(2, 2, 2, 2);
                        btnLayout->setSpacing(5);

                        QPushButton *dBtn = new QPushButton("下载");
                        QPushButton *delBtn = new QPushButton("删除");
                        QPushButton *sBtn = new QPushButton("分享");

                        dBtn->setFixedSize(45, 25);
                        delBtn->setFixedSize(45, 25);
                        sBtn->setFixedSize(45, 25);

                        connect(dBtn, &QPushButton::clicked, [=]() { downloadFileById(fileId, fileName); });
                        connect(delBtn, &QPushButton::clicked, [=]() { deleteFileById(fileId, i); });
                        connect(sBtn, &QPushButton::clicked, [=]() { shareFileById(fileId); });

                        btnLayout->addWidget(dBtn);
                        btnLayout->addWidget(delBtn);
                        btnLayout->addWidget(sBtn);
                        btnContainer->setLayout(btnLayout);
                        m_tableWidget->setCellWidget(i, 4, btnContainer);
                    }
                }
            }
        }
        reply->deleteLater();
    });
}

void FileListWidget::downloadFile()
{
    int currentRow = m_tableWidget->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "提示", "请先选择一个文件");
        return;
    }

    int fileId = m_tableWidget->item(currentRow, 0)->text().toInt();
    QString fileName = m_tableWidget->item(currentRow, 1)->text();

    downloadFileById(fileId, fileName);
}

void FileListWidget::downloadFileById(int fileId, const QString &filename)
{
    LoginInstance *ins = LoginInstance::getInstance();
    QString url = QString("http://%1:%2/download?file_id=%3")
                      .arg(ins->getServerIP())
                      .arg(ins->getServerPort())
                      .arg(fileId);

    QNetworkRequest request(url);
    request.setRawHeader("X-Auth-Token", m_token.toUtf8());

    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);

    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::downloadProgress, this, &FileListWidget::onDownloadProgress);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        m_progressBar->setVisible(false);

        if (reply->error() == QNetworkReply::NoError) {
            QString savePath = QFileDialog::getSaveFileName(this, "保存文件", filename);
            if (!savePath.isEmpty()) {
                QFile file(savePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(reply->readAll());
                    file.close();
                    QMessageBox::information(this, "成功", "文件下载完成！");
                }
            }
        } else {
            QMessageBox::warning(this, "错误", "下载失败: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

void FileListWidget::deleteFile()
{
    int currentRow = m_tableWidget->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "提示", "请先选择一个文件");
        return;
    }

    int fileId = m_tableWidget->item(currentRow, 0)->text().toInt();
    QString fileName = m_tableWidget->item(currentRow, 1)->text();

    QMessageBox::StandardButton reply = QMessageBox::question(this, "确认删除",
                                                              QString("确定要删除文件 \"%1\" 吗？").arg(fileName),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        deleteFileById(fileId, currentRow);
    }
}

void FileListWidget::deleteFileById(int fileId, int row)
{
    LoginInstance *ins = LoginInstance::getInstance();
    QString url = QString("http://%1:%2/delete")
                      .arg(ins->getServerIP())
                      .arg(ins->getServerPort());

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-Auth-Token", m_token.toUtf8());

    QJsonObject obj;
    obj.insert("file_id", fileId);
    QJsonDocument doc(obj);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_networkManager->post(request, data);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument resDoc = QJsonDocument::fromJson(response);
            QJsonObject resObj = resDoc.object();

            if (resObj.value("code").toString() == "000") {
                m_tableWidget->removeRow(row);
                QMessageBox::information(this, "成功", "文件删除成功！");
            } else {
                QMessageBox::warning(this, "失败", resObj.value("message").toString());
            }
        } else {
            QMessageBox::warning(this, "错误", "删除失败: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

void FileListWidget::shareFile()
{
    int currentRow = m_tableWidget->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "提示", "请先选择一个文件");
        return;
    }

    int fileId = m_tableWidget->item(currentRow, 0)->text().toInt();
    shareFileById(fileId);
}

void FileListWidget::shareFileById(int fileId)
{
    LoginInstance *ins = LoginInstance::getInstance();
    QString url = QString("http://%1:%2/share")
                      .arg(ins->getServerIP())
                      .arg(ins->getServerPort());

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-Auth-Token", m_token.toUtf8());

    QJsonObject obj;
    obj.insert("file_id", fileId);
    obj.insert("expire_days", 7);
    QJsonDocument doc(obj);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_networkManager->post(request, data);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument resDoc = QJsonDocument::fromJson(response);
            QJsonObject resObj = resDoc.object();

            if (resObj.value("code").toString() == "000") {
                QString shareUrl = resObj.value("share_url").toString();
                QClipboard *clipboard = QApplication::clipboard();
                clipboard->setText(shareUrl);

                QMessageBox::information(this, "分享链接",
                                         QString("分享链接已复制到剪贴板！\n\n链接: %1\n\n有效期: 7天").arg(shareUrl));
            } else {
                QMessageBox::warning(this, "失败", resObj.value("message").toString());
            }
        } else {
            QMessageBox::warning(this, "错误", "创建分享失败: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

void FileListWidget::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        int percent = (bytesReceived * 100) / bytesTotal;
        m_progressBar->setValue(percent);
    }
}