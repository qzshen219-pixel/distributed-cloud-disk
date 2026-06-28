#include "login.h"
#include "ui_login.h"
#include <QPainter>
#include <QRegularExpression>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

Login::Login(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Login)
{
    ui->setupUi(this);
    // 去边框
    this->setWindowFlags(Qt::FramelessWindowHint | windowFlags());

    // 给titbar对象设置父亲
    ui->myToolBar->setMyParent(this);

    // 处理接受的titlebar的信号
    connect(ui->myToolBar, &MyTitleBar::showSetWindow, [=]()
            {
                ui->stackedWidget->setCurrentWidget(ui->setPage);
            });
    connect(ui->myToolBar, &MyTitleBar::showMinWindow, [=]()
            {
                // 窗口最小化
                this->showMinimized();
            });
    connect(ui->myToolBar, &MyTitleBar::closeMyWindow, [=]()
            {
                if(ui->stackedWidget->currentWidget() == ui->setPage)
                {
                    ui->stackedWidget->setCurrentIndex(0);  // 登录窗口
                    // 清空控件中的用户数据
                }
                else if(ui->stackedWidget->currentWidget() == ui->regPage)
                {
                    ui->stackedWidget->setCurrentIndex(0);  // 登录窗口
                    // 清空控件中的用户数据
                }
                else
                {
                    this->close();
                }
            });
}

Login::~Login()
{
    delete ui;
}

void Login::paintEvent(QPaintEvent *event)
{
    // 画背景图的操作
    QPainter p(this);   // 绘图设备为当前窗口
    p.drawPixmap(0, 0, this->width(), this->height(), QPixmap(":/images/login_bk.jpg"));
}

void Login::on_regAccount_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}

void Login::on_regButton_clicked()
{
    // 1. 从控件中取出用户输入的数据
    QString userName = ui->reg_userName->text();
    QString nickName = ui->reg_NickName->text();
    QString passwd = ui->reg_passwd->text();
    QString confirmPwd = ui->reg_confirmPwd->text();
    QString email = ui->reg_mail->text();
    QString phone = ui->reg_phone->text();

    // 2. 数据校验 - 用户名
    QString USER_REG = "^[a-zA-Z0-9_@#\\-*]{3,16}$";
    QRegularExpression regexp(USER_REG);
    if (!regexp.match(userName).hasMatch())
    {
        QMessageBox::warning(this, "ERROR", "用户名格式不正确!\n要求:3-16位，可由字母、数字、下划线、@、#、-、*组成");
        return;
    }

    // 校验昵称
    if (nickName.isEmpty())
    {
        QMessageBox::warning(this, "ERROR", "昵称不能为空!");
        return;
    }

    // 校验密码长度
    if (passwd.length() < 6)
    {
        QMessageBox::warning(this, "ERROR", "密码长度不能少于6位!");
        return;
    }

    // 校验两次密码是否一致
    if (passwd != confirmPwd)
    {
        QMessageBox::warning(this, "ERROR", "两次输入的密码不一致!");
        return;
    }

    // 校验邮箱（可选）
    QString EMAIL_REG = "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$";
    QRegularExpression emailRegex(EMAIL_REG);
    if (!email.isEmpty() && !emailRegex.match(email).hasMatch())
    {
        QMessageBox::warning(this, "ERROR", "邮箱格式不正确!");
        return;
    }

    // 校验手机号（可选）
    QString PHONE_REG = "^\\d{11}$";
    QRegularExpression phoneRegex(PHONE_REG);
    if (!phone.isEmpty() && !phoneRegex.match(phone).hasMatch())
    {
        QMessageBox::warning(this, "ERROR", "手机号格式不正确!\n要求:11位数字");
        return;
    }

    // 3. 获取服务器配置（需要先在UI中设置objectName为 serverAddress 和 serverPort）
    QString serverIp = ui->serverAddress->text();
    QString serverPort = ui->serverPort->text();

    if (serverIp.isEmpty())
    {
        QMessageBox::warning(this, "ERROR", "请先在服务器设置页面填写服务器地址!");
        return;
    }

    if (serverPort.isEmpty())
    {
        QMessageBox::warning(this, "ERROR", "请先在服务器设置页面填写服务器端口!");
        return;
    }

    // 4. 创建网络管理器
    QNetworkAccessManager* pManager = new QNetworkAccessManager(this);

    // 5. 创建请求对象并设置URL
    QNetworkRequest request;
    QString url = QString("http://%1:%2/reg").arg(serverIp).arg(serverPort);
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 6. 构建 JSON 数据
    QJsonObject obj;
    obj.insert("userName", userName);
    obj.insert("nickName", nickName);
    obj.insert("firstPwd", passwd);
    obj.insert("phone", phone);
    obj.insert("email", email);

    QJsonDocument doc(obj);
    QByteArray json = doc.toJson();

    // 7. 发送 POST 请求
    QNetworkReply* reply = pManager->post(request, json);

    // 8. 接收服务器响应
    connect(reply, &QNetworkReply::readyRead, this, [=](){
        QByteArray all = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(all);
        QJsonObject myobj = doc.object();
        QString status = myobj.value("code").toString();

        if (status == "002")
        {
            QMessageBox::information(this, "提示", "注册成功!");
            ui->stackedWidget->setCurrentIndex(0);
            // 清空注册表单
            ui->reg_userName->clear();
            ui->reg_NickName->clear();
            ui->reg_passwd->clear();
            ui->reg_confirmPwd->clear();
            ui->reg_mail->clear();
            ui->reg_phone->clear();
        }
        else if (status == "003")
        {
            QMessageBox::warning(this, "ERROR", "用户名已存在!");
        }
        else
        {
            QMessageBox::warning(this, "ERROR", "注册失败，请稍后重试!");
        }
    });

    // 9. 错误处理
    connect(reply, &QNetworkReply::errorOccurred, this, [=](QNetworkReply::NetworkError error){
        Q_UNUSED(error);
        QMessageBox::warning(this, "ERROR", QString("网络错误: %1").arg(reply->errorString()));
    });

    // 10. 结束后释放 reply
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}
