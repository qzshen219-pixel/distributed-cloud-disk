// logininstance.h
#ifndef LOGININSTANCE_H
#define LOGININSTANCE_H

#include <QString>
#include <QMutex>

class LoginInstance
{
public:
    static LoginInstance* getInstance();

    // 服务器配置
    void setServerIP(const QString &ip);
    QString getServerIP() const;
    void setServerPort(const QString &port);
    QString getServerPort() const;

    // 用户信息
    void setUserId(int id);
    int getUserId() const;
    void setUserName(const QString &name);
    QString getUserName() const;
    void setUserToken(const QString &token);
    QString getUserToken() const;

    // 登录状态
    bool isLoggedIn() const;
    void logout();

    // 保存/加载配置
    void saveToFile();
    void loadFromFile();

private:
    LoginInstance();
    ~LoginInstance();

    static LoginInstance* m_instance;
    static QMutex m_mutex;

    // 服务器配置
    QString m_serverIP;
    QString m_serverPort;

    // 用户信息
    int m_userId;
    QString m_userName;
    QString m_userToken;
    bool m_loggedIn;
};

#endif // LOGININSTANCE_H