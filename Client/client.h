#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <QAbstractSocket>
#include <QDebug>
#include <QMessageBox>
#include <QTcpSocket>
#include <QQueue>
#include <QTimer>

typedef struct  {
    qint32 code;
    qint32 par1;
    qint32 par2;
    qint32 time;
} command_t;

enum command_type   {
    com_init,
    com_buffer,
    com_ping
};

QT_BEGIN_NAMESPACE
namespace Ui { class Client; }
QT_END_NAMESPACE

class Client : public QMainWindow
{
    Q_OBJECT

public:
    Client(QMainWindow *parent = nullptr);
    ~Client();

    void sendMessage(command_type type);
    void addCommand(command_t command);

    QString sHost;

signals:
    void newMessage(QString);

private slots:
    void network();
    void discardSocket();
    void getErrorCode(QAbstractSocket::SocketError errorCode);

    void displayMessage(const QString& str);
    void on_pushButton_clicked();

    void OnTimer();

    void slotReConnect();
    void slotStopReConnect();
    void slotStartReConnect();

    void on_pushButton_2_clicked();

private:
    Ui::Client *ui;

    bool bInited;
    QTcpSocket* socket;

    QByteArray bufferRead, bufferSend, dataSend;

    QQueue<command_t> m_commands;

    int messageCount;
    bool bConnected;
    bool bInProcess;
    QTimer *timer;
    QTimer *reconnect_timer;
};
#endif // CLIENT_H
