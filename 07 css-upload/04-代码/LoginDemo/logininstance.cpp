// logininstance.cpp
#include "logininstance.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

LoginInstance* LoginInstance::m_instance = nullptr;
QMutex LoginInstance::m_mutex;

LoginInstance::LoginInstance()
    : m_serverIP("192.168.226.128")
    , m_serverPort("80")
    , m_userId(0)
    , m_loggedIn(false)
{
    loadFromFile();  // 构造函数中加载配置
}

LoginInstance::~LoginInstance()
{
    saveToFile();    // 析构函数中保存配置
}

LoginInstance* LoginInstance::getInstance()
{
    if (m_instance == nullptr) {
        QMutexLocker locker(&m_mutex);
        if (m_instance == nullptr) {
            m_instance = new LoginInstance();
        }
    }
    return m_instance;
}

void LoginInstance::setServerIP(const QString &ip)
{
    m_serverIP = ip;
}

QString LoginInstance::getServerIP() const
{
    return m_serverIP;
}

void LoginInstance::setServerPort(const QString &port)
{
    m_serverPort = port;
}

QString LoginInstance::getServerPort() const
{
    return m_serverPort;
}

void LoginInstance::setUserId(int id)
{
    m_userId = id;
}

int LoginInstance::getUserId() const
{
    return m_userId;
}

void LoginInstance::setUserName(const QString &name)
{
    m_userName = name;
}

QString LoginInstance::getUserName() const
{
    return m_userName;
}

void LoginInstance::setUserToken(const QString &token)
{
    m_userToken = token;
    m_loggedIn = !token.isEmpty();
}

QString LoginInstance::getUserToken() const
{
    return m_userToken;
}

bool LoginInstance::isLoggedIn() const
{
    return m_loggedIn && !m_userToken.isEmpty();
}

void LoginInstance::logout()
{
    m_userToken.clear();
    m_userId = 0;
    m_userName.clear();
    m_loggedIn = false;
    saveToFile();
}

// 保存配置到文件
void LoginInstance::saveToFile()
{
    QJsonObject obj;
    obj["serverIP"] = m_serverIP;
    obj["serverPort"] = m_serverPort;
    obj["userId"] = m_userId;
    obj["userName"] = m_userName;
    obj["userToken"] = m_userToken;

    QJsonDocument doc(obj);
    QFile file("config.json");
    if (file.open(QFile::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        qDebug() << "Config saved";
    }
}

// 从文件加载配置
void LoginInstance::loadFromFile()
{
    QFile file("config.json");
    if (file.open(QFile::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull()) {
            QJsonObject obj = doc.object();
            m_serverIP = obj["serverIP"].toString("192.168.226.128");
            m_serverPort = obj["serverPort"].toString("80");
            m_userId = obj["userId"].toInt(0);
            m_userName = obj["userName"].toString();
            m_userToken = obj["userToken"].toString();
            m_loggedIn = !m_userToken.isEmpty();
            qDebug() << "Config loaded";
        }
    }
}