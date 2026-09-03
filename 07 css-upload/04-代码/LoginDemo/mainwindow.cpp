// mainwindow.cpp - 修复布局重叠问题
#include "mainwindow.h"
#include "filelistwidget.h"
#include "logininstance.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTime>
#include <QAction>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 创建中央控件
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    // 创建主布局 - 设置边距和间距
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // ========== 第一行：文件路径 ==========
    QHBoxLayout *fileLayout = new QHBoxLayout();
    fileLayout->setSpacing(5);

    QLabel *label = new QLabel("文件路径：", this);
    label->setFixedWidth(70);

    m_filePath = new QLineEdit(this);
    m_selFileBtn = new QPushButton("选择文件", this);
    m_selFileBtn->setFixedWidth(80);

    fileLayout->addWidget(label);
    fileLayout->addWidget(m_filePath);
    fileLayout->addWidget(m_selFileBtn);
    mainLayout->addLayout(fileLayout);

    // ========== 第二行：进度条 ==========
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedHeight(20);
    mainLayout->addWidget(m_progressBar);

    // ========== 第三行：日志文本框 ==========
    m_recordMsg = new QTextEdit(this);
    m_recordMsg->setReadOnly(true);
    m_recordMsg->setMinimumHeight(150);
    mainLayout->addWidget(m_recordMsg);

    // ========== 第四行：上传按钮 ==========
    m_uploadBtn = new QPushButton("上传文件", this);
    m_uploadBtn->setFixedHeight(35);
    m_uploadBtn->setFixedWidth(120);

    // 按钮居中
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(m_uploadBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // ========== 创建菜单栏 ==========
    QMenuBar *menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    // 文件菜单
    QMenu *fileMenu = menuBar->addMenu("文件(&F)");

    QAction *uploadAction = new QAction("上传文件(&U)", this);
    QAction *fileListAction = new QAction("我的文件(&L)", this);
    QAction *exitAction = new QAction("退出(&X)", this);

    uploadAction->setShortcut(QKeySequence("Ctrl+U"));
    fileListAction->setShortcut(QKeySequence("Ctrl+L"));
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));

    fileMenu->addAction(uploadAction);
    fileMenu->addAction(fileListAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    // 帮助菜单
    QMenu *helpMenu = menuBar->addMenu("帮助(&H)");
    QAction *aboutAction = new QAction("关于(&A)", this);
    helpMenu->addAction(aboutAction);

    // ========== 连接信号槽 ==========
    connect(m_selFileBtn, &QPushButton::clicked, this, &MainWindow::on_selFile_clicked);
    connect(m_uploadBtn, &QPushButton::clicked, this, &MainWindow::on_uploadBtn_clicked);
    connect(uploadAction, &QAction::triggered, this, &MainWindow::on_uploadBtn_clicked);
    connect(fileListAction, &QAction::triggered, this, &MainWindow::on_fileListBtn_clicked);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, "关于", "云盘客户端 v1.0\n\n支持文件上传、下载、分享功能");
    });

    // 显示当前用户
    LoginInstance *ins = LoginInstance::getInstance();
    setWindowTitle(QString("云盘客户端 - 欢迎 %1").arg(ins->getUserName()));
    setMinimumSize(650, 450);
    resize(650, 450);

    appendLog("程序启动，当前用户: " + ins->getUserName());
    appendLog("服务器地址: " + ins->getServerIP() + ":" + ins->getServerPort());
}

MainWindow::~MainWindow()
{
}

void MainWindow::appendLog(const QString &msg)
{
    QString timeStr = QTime::currentTime().toString("hh:mm:ss");
    m_recordMsg->append(QString("[%1] %2").arg(timeStr, msg));
}

void MainWindow::setProgress(int percent)
{
    m_progressBar->setValue(percent);
}

QString MainWindow::getFileMd5(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    if (hash.addData(&file)) {
        file.close();
        return hash.result().toHex();
    }
    file.close();
    return QString();
}

void MainWindow::on_selFile_clicked()
{
    QString path = QFileDialog::getOpenFileName(this, "选择文件", "", "所有文件 (*.*)");
    if (path.isEmpty()) return;

    m_filePath->setText(path);
    QFileInfo info(path);
    appendLog(QString("已选择文件: %1 (大小: %2 KB)").arg(info.fileName()).arg(info.size() / 1024.0, 0, 'f', 2));
}

void MainWindow::on_uploadBtn_clicked()
{
    QString filePath = m_filePath->text();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先选择文件！");
        return;
    }

    LoginInstance *ins = LoginInstance::getInstance();
    QString token = ins->getUserToken();
    if (token.isEmpty()) {
        QMessageBox::warning(this, "提示", "登录凭证无效，请重新登录！");
        return;
    }

    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "错误", "无法打开文件！");
        delete file;
        return;
    }

    QFileInfo info(filePath);
    QString md5 = getFileMd5(filePath);

    appendLog(QString("文件MD5: %1").arg(md5));
    appendLog("开始上传文件...");
    setProgress(0);

    QString url = QString("http://%1:%2/upload")
                      .arg(ins->getServerIP())
                      .arg(ins->getServerPort());

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("X-Auth-Token", token.toUtf8());

    // 文件部分
    QHttpPart filePart;
    QString disp = QString("form-data; name=\"file\"; filename=\"%1\"; md5=\"%2\"; size=%3")
                       .arg(info.fileName()).arg(md5).arg(info.size());
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, disp);
    filePart.setBodyDevice(file);

    // 用户部分
    QHttpPart userPart;
    userPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"user\""));
    userPart.setBody(ins->getUserName().toUtf8());

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType, this);
    multiPart->append(userPart);
    multiPart->append(filePart);

    QNetworkReply *reply = manager->post(request, multiPart);

    // 上传进度
    connect(reply, &QNetworkReply::uploadProgress, this, [=](qint64 bytesSent, qint64 bytesTotal){
        if (bytesTotal > 0) {
            int percent = (bytesSent * 100) / bytesTotal;
            setProgress(percent);
        }
    });

    // 完成响应
    connect(reply, &QNetworkReply::finished, this, [=](){
        setProgress(100);

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            appendLog("服务器响应: " + response);

            QJsonDocument doc = QJsonDocument::fromJson(response);
            if (!doc.isNull()) {
                QJsonObject obj = doc.object();
                QString code = obj.value("code").toString();

                if (code == "000") {
                    appendLog("✅ 文件上传成功！");
                    QMessageBox::information(this, "成功", "文件上传成功！");
                    m_filePath->clear();
                    setProgress(0);
                } else {
                    appendLog("❌ 上传失败: " + obj.value("message").toString());
                    QMessageBox::warning(this, "失败", obj.value("message").toString());
                }
            }
        } else {
            appendLog("❌ 网络错误: " + reply->errorString());
            QMessageBox::warning(this, "错误", "网络连接失败: " + reply->errorString());
        }

        multiPart->deleteLater();
        file->deleteLater();
        reply->deleteLater();
        manager->deleteLater();
    });
}

void MainWindow::on_fileListBtn_clicked()
{
    LoginInstance *ins = LoginInstance::getInstance();

    appendLog("打开文件列表...");

    if (!ins->isLoggedIn()) {
        QMessageBox::warning(this, "提示", "请先登录！");
        return;
    }

    FileListWidget *fileList = new FileListWidget(ins->getUserToken(), ins->getUserId(), nullptr);
    fileList->setAttribute(Qt::WA_DeleteOnClose);
    fileList->setWindowTitle("我的文件 - " + ins->getUserName());
    fileList->show();

    appendLog("文件列表窗口已打开");
}
