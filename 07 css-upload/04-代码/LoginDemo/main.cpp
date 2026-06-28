// main.cpp
#include <QApplication>
#include "login.h"
#include "mainwindow.h"
#include "logininstance.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    Login login;
    if (login.exec() == QDialog::Accepted) {
        MainWindow w;
        w.show();
        return a.exec();
    }

    return 0;
}