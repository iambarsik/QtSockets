#ifndef SERVER_H
#define SERVER_H

#include <QMainWindow>
#include <QDebug>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>


QT_BEGIN_NAMESPACE
namespace Ui { class Server; }
QT_END_NAMESPACE

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

class Server : public QMainWindow
{
    Q_OBJECT

public:
    Server(QMainWindow *parent = nullptr);
    ~Server();

    bool isStarted() { return m_started; }
    void addCommand(command_t command);

signals:
    void newMessage(QString);

private slots:
    void newConnection();
    void appendToSocketList(QTcpSocket* socket);
    void getErrorCode(QAbstractSocket::SocketError errorCode);

    void network();
    void discardSocket();

    void displayMessage(const QString& str);
    void sendMessage(QTcpSocket* socket, command_type type);

    void OnTimer();

    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

private:
    Ui::Server *ui;

    bool bInited;

    QTcpServer* m_server;
    bool m_started;
    QSet<QTcpSocket*> connection_set;
    QByteArray bufferRead, bufferSend, dataSend;

    QTimer *timer;
};
#endif // SERVER_H
