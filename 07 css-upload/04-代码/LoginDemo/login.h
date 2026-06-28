// login.h
#ifndef LOGIN_H
#define LOGIN_H

#include <QDialog>
#include <QNetworkAccessManager>
#include <QPushButton>
#include <QLabel>

namespace Ui {
class Login;
}

class Login : public QDialog
{
    Q_OBJECT

public:
    explicit Login(QWidget *parent = nullptr);
    ~Login();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void on_regAccount_clicked();
    void on_regButton_clicked();
    void on_loginButton_clicked();
    void on_settingSaveBtn_clicked();

private:
    Ui::Login *ui;
    QNetworkAccessManager *m_networkManager;
    QWidget *m_titleBar;

    void clearRegForm();
    void clearLoginForm();
    bool validateUserName(const QString &userName);
    bool validatePassword(const QString &password);
    bool validateEmail(const QString &email);
    bool validatePhone(const QString &phone);

    void handleRegisterSuccess();
    void handleRegisterFailed(const QString &message);
    void handleLoginSuccess(const QString &token);
    void handleLoginFailed(const QString &message);
    void handleNetworkError(const QString &error);
};

#endif // LOGIN_H