#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFileInfo>
#include <QCryptographicHash>
#include "logininstance.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // 初始化进度条
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);

    // 登录成功 -> 设置当前用户的名字
    LoginInstance* ins = LoginInstance::getInstance();
    // 从登录界面获取用户名，而不是硬编码
    // ins->setUserName("Tom");
}

MainWindow::~MainWindow()
{
    delete ui;
}

QString MainWindow::getMd5(QString path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file for MD5 calculation:" << path;
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Md5);

    // 分块读取，避免大文件内存问题
    const qint64 bufferSize = 8192;
    char buffer[bufferSize];
    while (!file.atEnd()) {
        qint64 bytesRead = file.read(buffer, bufferSize);
        if (bytesRead > 0) {
            hash.addData(buffer, bytesRead);
        }
    }
    file.close();

    return hash.result().toHex();
}

void MainWindow::on_selFile_clicked()
{
    QString path = QFileDialog::getOpenFileName(this, "打开文件", "", "所有文件 (*.*)");
    if(path.isEmpty())
    {
        QMessageBox::warning(this, "警告", "文件路径为空!!!");
        return;
    }
    ui->filePath->setText(path);
    ui->recordMsg->append("已选择文件: " + QFileInfo(path).fileName());
}

void MainWindow::on_uploadBtn_clicked()
{
    QString filePath = ui->filePath->text();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先选择文件！");
        return;
    }

    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "错误", "无法打开文件！");
        delete file;
        return;
    }

    // 1. 创建 networkmanager 对象
    QNetworkAccessManager* pManager = new QNetworkAccessManager(this);

    // 2. 发送数据 - post
    QNetworkRequest request;
    // 修改点1：URL 使用正确的地址
    LoginInstance* ins = LoginInstance::getInstance();
    request.setUrl(QUrl(QString("http://%1:%2/upload").arg(ins->ip()).arg(ins->port())));

    // post数据块
    QFileInfo info(filePath);
    QString md5 = getMd5(filePath);

    // 修改点2：修正 Content-Disposition 格式
    QString disp = QString("form-data; name=\"file\"; filename=\"%1\"; md5=\"%2\"; size=%3")
                       .arg(info.fileName())
                       .arg(md5)
                       .arg(info.size());

    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentDispositionHeader, disp);
    part.setBodyDevice(file);
    file->setParent(nullptr);  // 避免提前释放

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType, this);
    multiPart->append(part);

    // 添加用户字段
    QHttpPart userPart;
    userPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"user\""));
    userPart.setBody(ins->getUserName().toUtf8());
    multiPart->append(userPart);

    ui->recordMsg->append("开始向服务器发送数据...");
    ui->recordMsg->append(QString("文件: %1, 大小: %2 KB, MD5: %3")
                              .arg(info.fileName())
                              .arg(info.size() / 1024.0, 0, 'f', 2)
                              .arg(md5));

    QNetworkReply* reply = pManager->post(request, multiPart);

    // 进度条
    connect(reply, &QNetworkReply::uploadProgress, this, [=](qint64 bytesSent, qint64 bytesTotal){
        if (bytesTotal > 0) {
            int percent = (bytesSent * 100) / bytesTotal;
            ui->progressBar->setValue(percent);
            qDebug() << "上传进度: " << percent << "% (" << bytesSent << "/" << bytesTotal << ")";
        }
    });

    // 接收响应
    connect(reply, &QNetworkReply::finished, this, [=](){
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray all = reply->readAll();
            ui->recordMsg->append("服务器响应: " + all);

            // 解析 JSON
            QJsonDocument doc = QJsonDocument::fromJson(all);
            if (!doc.isNull()) {
                QJsonObject obj = doc.object();
                QString code = obj.value("code").toString();
                QString message = obj.value("message").toString();

                if (code == "000") {
                    ui->progressBar->setValue(100);
                    ui->recordMsg->append("✅ 上传成功！");
                    QMessageBox::information(this, "成功", "文件上传成功！");
                } else {
                    ui->recordMsg->append("❌ 上传失败: " + message);
                    QMessageBox::warning(this, "失败", message);
                }
            }
        } else {
            ui->recordMsg->append("网络错误: " + reply->errorString());
            QMessageBox::warning(this, "错误", "网络连接失败！");
        }

        // 清理资源
        multiPart->deleteLater();
        file->deleteLater();
        reply->deleteLater();
        pManager->deleteLater();
    });
}