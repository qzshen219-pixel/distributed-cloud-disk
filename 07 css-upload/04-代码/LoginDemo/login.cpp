// login.cpp
#include "login.h"
#include "ui_login.h"
#include "logininstance.h"
#include <QPainter>
#include <QRegularExpression>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>

Login::Login(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Login),
    m_networkManager(new QNetworkAccessManager(this)),
    m_titleBar(nullptr)
{
    ui->setupUi(this);

    // 去掉系统默认的标题栏
    this->setWindowFlags(Qt::FramelessWindowHint);
    this->setFixedSize(450, 550);
    this->setStyleSheet("QDialog { background-color: #f0f0f0; }");

    // ========== 创建自定义标题栏 ==========
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName("titleBar");
    m_titleBar->setFixedHeight(40);
    m_titleBar->setStyleSheet(
        "QWidget#titleBar { background-color: #2c3e50; border-top-left-radius: 8px; border-top-right-radius: 8px; }"
        );

    QLabel *titleLabel = new QLabel("刘派云盘", m_titleBar);
    titleLabel->setGeometry(10, 5, 150, 30);
    titleLabel->setStyleSheet("color: white; font-size: 14px; font-weight: bold;");

    QPushButton *btnMin = new QPushButton("－", m_titleBar);
    QPushButton *btnSetting = new QPushButton("⚙", m_titleBar);
    QPushButton *btnClose = new QPushButton("✕", m_titleBar);

    btnMin->setGeometry(width() - 105, 5, 35, 30);
    btnSetting->setGeometry(width() - 70, 5, 35, 30);
    btnClose->setGeometry(width() - 35, 5, 35, 30);

    btnMin->setStyleSheet(
        "QPushButton { background-color: transparent; color: white; border: none; font-size: 16px; }"
        "QPushButton:hover { background-color: #34495e; }"
        );
    btnSetting->setStyleSheet(
        "QPushButton { background-color: transparent; color: white; border: none; font-size: 16px; }"
        "QPushButton:hover { background-color: #34495e; }"
        );
    btnClose->setStyleSheet(
        "QPushButton { background-color: transparent; color: white; border: none; font-size: 16px; }"
        "QPushButton:hover { background-color: #e74c3c; }"
        );

    // 连接标题栏按钮
    connect(btnMin, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(btnSetting, &QPushButton::clicked, [=]() {
        if (ui->stackedWidget) {
            ui->stackedWidget->setCurrentWidget(ui->setPage);
        }
    });
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);

    // 让标题栏可拖动
    m_titleBar->installEventFilter(this);

    // 调整内容区域位置
    if (ui->stackedWidget) {
        ui->stackedWidget->setGeometry(0, 40, width(), height() - 40);
    }

    // ========== 加载设置 ==========
    LoginInstance* ins = LoginInstance::getInstance();
    if (ui->ip) ui->ip->setText(ins->getServerIP());
    if (ui->port) ui->port->setText(ins->getServerPort());
    if (ui->ip) ui->ip->setText(ins->getServerIP());
    if (ui->port) ui->port->setText(ins->getServerPort());

    // 设置默认测试账号
    if (ui->login_user) ui->login_user->setText("alice");
    if (ui->login_passwd) ui->login_passwd->setText("123456");
}

Login::~Login()
{
    delete ui;
}

void Login::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
}

void Login::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);

    // 调整标题栏大小
    if (m_titleBar) {
        m_titleBar->setGeometry(0, 0, width(), 40);

        // 调整按钮位置
        QList<QPushButton*> btns = m_titleBar->findChildren<QPushButton*>();
        if (btns.size() >= 3) {
            btns[0]->setGeometry(width() - 105, 5, 35, 30);
            btns[1]->setGeometry(width() - 70, 5, 35, 30);
            btns[2]->setGeometry(width() - 35, 5, 35, 30);
        }
    }

    // 调整内容区域
    if (ui->stackedWidget) {
        ui->stackedWidget->setGeometry(0, 40, width(), height() - 40);
    }
}

bool Login::eventFilter(QObject *obj, QEvent *event)
{
    static QPoint dragPosition;
    static bool dragging = false;

    if (obj == m_titleBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                dragging = true;
                dragPosition = mouseEvent->globalPos() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && dragging) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            move(mouseEvent->globalPos() - dragPosition);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && dragging) {
            dragging = false;
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

void Login::on_regAccount_clicked()
{
    if (ui->stackedWidget) {
        ui->stackedWidget->setCurrentIndex(1);
        clearRegForm();
    }
}

void Login::on_okBtn_clicked()
{
    if (!ui->ip || !ui->port) return;

    QString serverIP = ui->ip->text();
    QString serverPort = ui->port->text();

    if (serverIP.isEmpty()) {
        QMessageBox::warning(this, "警告", "服务器地址不能为空！");
        ui->ip->setFocus();
        return;
    }

    if (serverPort.isEmpty()) {
        QMessageBox::warning(this, "警告", "服务器端口不能为空！");
        ui->port->setFocus();
        return;
    }

    LoginInstance* ins = LoginInstance::getInstance();
    ins->setServerIP(serverIP);
    ins->setServerPort(serverPort);
    ins->saveToFile();

    if (ui->ip) ui->ip->setText(serverIP);
    if (ui->port) ui->port->setText(serverPort);

    QMessageBox::information(this, "成功", "服务器设置已保存！");
    if (ui->stackedWidget) {
        ui->stackedWidget->setCurrentIndex(0);
    }
}

void Login::on_loginButton_clicked()
{
    if (!ui->login_user || !ui->login_passwd || !ui->ip || !ui->port) {
        QMessageBox::warning(this, "错误", "界面初始化不完整！");
        return;
    }

    QString userName = ui->login_user->text();
    QString password = ui->login_passwd->text();

    if(userName.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入用户名！");
        ui->login_user->setFocus();
        return;
    }

    if(password.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入密码！");
        ui->login_passwd->setFocus();
        return;
    }

    // 显示登录中提示
    QMessageBox *waitBox = new QMessageBox(QMessageBox::Information, "提示", "正在登录...", QMessageBox::NoButton, this);
    waitBox->show();

    QString url = QString("http://%1:%2/login").arg(ui->ip->text()).arg(ui->port->text());

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    obj.insert("username", userName);
    obj.insert("password", password);

    QJsonDocument doc(obj);
    // 关键修改：使用 Compact 格式，去掉换行和空格
    QByteArray json = doc.toJson(QJsonDocument::Compact);

    qDebug() << "发送的JSON:" << json;

    QNetworkReply* reply = m_networkManager->post(request, json);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        waitBox->accept();

        if(reply->error() == QNetworkReply::NoError) {
            QByteArray all = reply->readAll();
            qDebug() << "Server response:" << all;

            QJsonDocument resDoc = QJsonDocument::fromJson(all);
            if (resDoc.isNull()) {
                handleLoginFailed("服务器返回数据格式错误");
                reply->deleteLater();
                return;
            }

            QJsonObject resObj = resDoc.object();
            QString status = resObj.value("code").toString();
            QString message = resObj.value("message").toString();
            QJsonObject data = resObj.value("data").toObject();
            QString token = data.value("token").toString();
            int userId = data.value("id").toInt();

            if(status == "000") {
                if (token.isEmpty() || userId <= 0) {
                    handleLoginFailed("服务器返回的登录凭证无效");
                } else {
                    handleLoginSuccess(token, userId);
                }
            } else {
                handleLoginFailed(message);
            }
        } else {
            handleNetworkError(reply->errorString());
        }
        reply->deleteLater();
    });
}

void Login::on_regButton_clicked()
{
    if (!ui->reg_userName || !ui->reg_passwd || !ui->ip || !ui->port) return;

    QString userName = ui->reg_userName->text();
    QString nickName = ui->reg_NickName->text();
    QString passwd = ui->reg_passwd->text();
    QString confirmPwd = ui->reg_confirmPwd->text();
    QString email = ui->reg_mail->text();
    QString phone = ui->reg_phone->text();

    if(userName.isEmpty()) {
        QMessageBox::warning(this, "警告", "用户名不能为空！");
        ui->reg_userName->setFocus();
        return;
    }

    if(passwd.isEmpty()) {
        QMessageBox::warning(this, "警告", "密码不能为空！");
        ui->reg_passwd->setFocus();
        return;
    }

    if(passwd != confirmPwd) {
        QMessageBox::warning(this, "错误", "两次输入的密码不一致！");
        ui->reg_confirmPwd->clear();
        ui->reg_passwd->clear();
        ui->reg_passwd->setFocus();
        return;
    }

    if(!validateUserName(userName)) {
        QMessageBox::warning(this, "错误", "用户名格式不正确！\n要求：3-16位字符");
        ui->reg_userName->clear();
        ui->reg_userName->setFocus();
        return;
    }

    if(!validatePassword(passwd)) {
        QMessageBox::warning(this, "错误", "密码格式不正确！\n要求：6-20位字符");
        ui->reg_passwd->clear();
        ui->reg_confirmPwd->clear();
        ui->reg_passwd->setFocus();
        return;
    }

    QString url = QString("http://%1:%2/register").arg(ui->ip->text()).arg(ui->port->text());

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    obj.insert("username", userName);
    obj.insert("nickname", nickName);
    obj.insert("password", passwd);
    obj.insert("email", email);
    obj.insert("phone", phone);

    QJsonDocument doc(obj);
    QByteArray json = doc.toJson();
    QNetworkReply* reply = m_networkManager->post(request, json);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if(reply->error() == QNetworkReply::NoError) {
            QByteArray all = reply->readAll();
            QJsonDocument resDoc = QJsonDocument::fromJson(all);
            QJsonObject resObj = resDoc.object();
            QString status = resObj.value("code").toString();
            QString message = resObj.value("message").toString();

            if(status == "002") {
                handleRegisterSuccess();
            } else {
                handleRegisterFailed(message);
            }
        } else {
            handleNetworkError(reply->errorString());
        }
        reply->deleteLater();
    });
}

void Login::handleRegisterSuccess()
{
    QMessageBox::information(this, "成功", "注册成功！请登录");
    if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(0);
    clearRegForm();
}

void Login::handleRegisterFailed(const QString &message)
{
    QMessageBox::warning(this, "失败", message.isEmpty() ? "用户名已存在！" : message);
    if (ui->reg_userName) ui->reg_userName->clear();
    if (ui->reg_userName) ui->reg_userName->setFocus();
}

void Login::handleLoginSuccess(const QString &token, int userId)
{
    LoginInstance *ins = LoginInstance::getInstance();
    if (ui->ip) ins->setServerIP(ui->ip->text());
    if (ui->port) ins->setServerPort(ui->port->text());
    if (ui->login_user) ins->setUserName(ui->login_user->text());
    ins->setUserToken(token);
    ins->setUserId(userId);

    QMessageBox::information(this, "成功", "登录成功！");
    this->accept();
}

void Login::handleLoginFailed(const QString &message)
{
    QMessageBox::warning(this, "失败", message.isEmpty() ? "用户名或密码错误！" : message);
    if (ui->login_passwd) ui->login_passwd->clear();
    if (ui->login_passwd) ui->login_passwd->setFocus();
}

void Login::handleNetworkError(const QString &error)
{
    QMessageBox::warning(this, "错误", QString("网络连接失败！\n\n请检查：\n1. 服务器地址是否正确\n2. 服务器是否运行\n3. 网络是否通畅\n\n详细错误：%1").arg(error));
}

void Login::clearRegForm()
{
    if (ui->reg_userName) ui->reg_userName->clear();
    if (ui->reg_NickName) ui->reg_NickName->clear();
    if (ui->reg_passwd) ui->reg_passwd->clear();
    if (ui->reg_confirmPwd) ui->reg_confirmPwd->clear();
    if (ui->reg_mail) ui->reg_mail->clear();
    if (ui->reg_phone) ui->reg_phone->clear();
}

void Login::clearLoginForm()
{
    if (ui->login_user) ui->login_user->clear();
    if (ui->login_passwd) ui->login_passwd->clear();
}

bool Login::validateUserName(const QString &userName)
{
    QRegularExpression regexp("^[a-zA-Z0-9_@*]{3,16}$");
    return regexp.match(userName).hasMatch();
}

bool Login::validatePassword(const QString &password)
{
    QRegularExpression regexp("^[a-zA-Z0-9_@*]{6,20}$");
    return regexp.match(password).hasMatch();
}

bool Login::validateEmail(const QString &email)
{
    if(email.isEmpty()) return true;
    QRegularExpression regexp("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
    return regexp.match(email).hasMatch();
}

bool Login::validatePhone(const QString &phone)
{
    if(phone.isEmpty()) return true;
    QRegularExpression regexp("^1[3-9]\\d{9}$");
    return regexp.match(phone).hasMatch();
}
