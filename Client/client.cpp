#include "client.h"
#include "ui_client.h"
#include <QHostAddress>

Client::Client(QMainWindow *parent)
    : QMainWindow(parent)
    , sHost("127.0.0.1")
    , ui(new Ui::Client)
    , bConnected(false)
    , bInProcess(false)
{
    ui->setupUi(this);
    setCentralWidget(ui->centralwidget);

    socket = new QTcpSocket(this);

    messageCount = 0;

    connect(this, &Client::newMessage, this, &Client::displayMessage);
    connect(socket, SIGNAL(readyRead()), this, SLOT(network()));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(getErrorCode(QAbstractSocket::SocketError)));

    timer = new QTimer;
    timer->setInterval(100);
    connect(timer,SIGNAL(timeout()),this,SLOT(OnTimer()));

    bInited = false;
}

Client::~Client()   {
    if(socket->isOpen())
        socket->close();
    delete ui;
}

void Client::network()   {

    if( socket->state() == QAbstractSocket::ConnectedState )    {
        bufferRead = socket->readAll();
        //qDebug() << bufferRead;
        qDebug() << bufferRead.size();

        while(bufferRead.length() >= 24)   {
            bool status;
            QByteArray header;
            header = bufferRead.mid(0,8);
            int iSize = header.mid( 1, 1).toHex().toInt(&status, 16);
            int iMainNode = header.mid( 2, 1).toHex().toInt(&status, 16);
            int iClientNode = header.mid( 3, 1).toHex().toInt(&status, 16);
            int iNetworkFlag = header.mid( 4, 1).toHex().toInt(&status, 16);
            int iFrameType = header.mid( 5, 1).toHex().toInt(&status, 16);

            if(iNetworkFlag == 0)   {
                qDebug() << "Have got package from server : " << iClientNode;
            } else {
                qDebug() << "Have got package from client: " << iClientNode;
            }
            qDebug() << "Size = " << iSize << " FrameType = " << iFrameType;

                // checking for default command
            if(iSize == 24) {
                QByteArray frame;
                frame = bufferRead.mid(0,24);
                frame.remove(0,8);

                if(frame.length() >= 16)    {                   // checking if frame is correct
                    bool status;
                    int iCommand = frame.mid( 0,4).toHex().toInt(&status,16);
                    int iPar1    = frame.mid( 4,4).toHex().toInt(&status,16);
                    int iPar2    = frame.mid( 8,4).toHex().toInt(&status,16);
                    int iTime    = frame.mid(12,4).toHex().toInt(&status,16);
                    bufferRead.remove(0, 24);                   // delete frame from buffer

                    //qDebug() << "Input command : " << iCommand << " " << iPar1 << " " << iPar2;

                    ui->textEdit->append(QString("<<-- S :: %1 %2 %3").arg(iCommand).arg(iPar1).arg(iPar2));
                }
            } else {
                // try to unpack big package
            }
        }
    }
}

void Client::discardSocket()    {
    socket->deleteLater();
    socket=nullptr;
    this->setWindowTitle("Disconnected!");
}

void Client::getErrorCode(QAbstractSocket::SocketError errorCode)   {
    qDebug() << "Socket Error = " << errorCode;
}

void Client::displayMessage(const QString& str) {
    messageCount++;
    qDebug() << messageCount << ": " << str;
}

void Client::sendMessage(command_type type)   {

    switch(type)    {
        case com_init:
            if(socket)  {
                if(socket->isOpen())    {
                    if(bInited == false) {
                        int MainNode  = 2;
                        int NodeID    = 1;
                        int FrameType = 0x15;
                        QString name  = "127.0.0.1";

                        dataSend.clear();

                        dataSend.append((char) 0x00);
                        dataSend.append((char) 13 + name.size());
                        dataSend.append((char) MainNode);
                        dataSend.append((char) NodeID);
                        dataSend.append((char) 0x01);
                        dataSend.append((char) FrameType);
                        dataSend.append((char) 0x00);
                        dataSend.append((char) 0x00);

                        dataSend.append((char) 0xC0);
                        dataSend.append((char) 0x00);
                        dataSend.append((char) 0x00);
                        dataSend.append((char) 0x03);

                        dataSend.append(name);
                        dataSend.append((char) 0x00);

                        bInited = true;

                        ui->textEdit->append(QString("C -->> : init command"));

                        socket->write(dataSend);
                        socket->flush();
                        socket->waitForBytesWritten(10);
                    }
                } else {
                    QMessageBox::critical(this,"QTCPClient","Socket doesn't seem to be opened");
                }
            } else {
                QMessageBox::critical(this,"QTCPClient","Not connected");
            }
        break;
        case com_ping:
            dataSend.clear();
            // 8 байт - заголовок кадра
            dataSend.append((char) 0x00);           // 1 байт - 0
            dataSend.append((char) 0x08);           // 2 байт - текущая длинна кадра в байтах (включая размер заголовка)
            dataSend.append((char) 0x02);           // 3 байт - идентификатор получателя
            dataSend.append((char) 0x01);           // 4 байт - идентификатор отправителя (номер узла)
            dataSend.append((char) 0x01);           // 5 байт - флаг кадра ( 0 - я сервер, 1 - я клинет )
            dataSend.append((char) 0x00);           // 6 байт - тип кадра 0
            dataSend.append((char) 0x00);           // 7 байт - зарезервировано
            dataSend.append((char) 0x00);           // 8 байт - зарезервировано
            socket->write(dataSend);
            socket->flush();
            socket->waitForBytesWritten(10000);
        break;
        case com_buffer:
            if(bufferSend.size() == 12){
                dataSend.clear();                
                    // 8 байт - заголовок кадра
                dataSend.append((char) 0x00);           // 1 байт - 0
                dataSend.append((char) 0x18);           // 2 байт - текущая длинна кадра в байтах (включая размер заголовка)
                dataSend.append((char) 0x02);           // 3 байт - идентификатор получателя
                dataSend.append((char) 0x01);           // 4 байт - идентификатор отправителя (номер узла)
                dataSend.append((char) 0x01);           // 5 байт - флаг кадра ( 0 - я сервер, 1 - я клинет )
                dataSend.append((char) 0x7F);           // 6 байт - тип кадра 0
                dataSend.append((char) 0x00);           // 7 байт - зарезервировано
                dataSend.append((char) 0x00);           // 8 байт - зарезервировано

                    // 4 байта - модельное время команды в миллисекундах
                bufferSend.append((char) 0x00);
                bufferSend.append((char) 0x00);
                bufferSend.append((char) 0x00);
                bufferSend.append((char) 0x03);

                dataSend.append(bufferSend);

                //qDebug() << "Length: " << dataSend.length();

                bufferSend.clear(); // очищаем буфер

                socket->write(dataSend);
                socket->flush();
                socket->waitForBytesWritten(10000);
            }
        break;
    }
}


void Client::on_pushButton_clicked()    {

        qint32 code = ui->lineCommand->text().toInt();
        qint32 par1 = ui->linePar1->text().toInt();
        qint32 par2 = ui->linePar2->text().toInt();
        command_t command = { code, par1, par2, 0};
        addCommand(command);    
}


void Client::addCommand(command_t command)  {

    bufferSend.clear();
    bufferSend.append((command.code >> 24) & 0xFF);
    bufferSend.append((command.code >> 16) & 0xFF);
    bufferSend.append((command.code >> 8)  & 0xFF);
    bufferSend.append((command.code) & 0xFF);

    bufferSend.append((command.par1 >> 24) & 0xFF);
    bufferSend.append((command.par1 >> 16) & 0xFF);
    bufferSend.append((command.par1 >> 8)  & 0xFF);
    bufferSend.append((command.par1) & 0xFF);

    bufferSend.append((command.par2 >> 24) & 0xFF);
    bufferSend.append((command.par2 >> 16) & 0xFF);
    bufferSend.append((command.par2 >> 8)  & 0xFF);
    bufferSend.append((command.par2) & 0xFF);

    ui->textEdit->append(QString("C -->> :: %1 %2 %3").arg(command.code).arg(command.par1).arg(command.par2));
    sendMessage(command_type::com_buffer);
}


void Client::OnTimer()  {
    if(bConnected == true)  {
        sendMessage(command_type::com_ping);
    }
}

void Client::slotReConnect()
{
    socket->connectToHost(sHost,9999);
    if(socket->waitForConnected(100))  {
        this->setWindowTitle("Connected to Server");
        ui->textEdit->append(QString("System :: connected to server"));
        bConnected = true;
    } else {
        //ui->textEdit->append(QString("System :: connecting ERROR"));
        return;
    }
    timer->start();
    bInited = false;
    sendMessage(command_type::com_init);
}

void Client::slotStopReConnect()
{
    reconnect_timer->stop();
    timer->start();
}

void Client::slotStartReConnect()
{
    reconnect_timer->start();
    timer->stop();
}

void Client::on_pushButton_2_clicked()  {

    QString sHost = ui->lineHost->text();
/*
    socket->connectToHost(host,9999);

    if(socket->waitForConnected())  {
        this->setWindowTitle("Connected to Server");
        ui->textEdit->append(QString("System :: connected to server"));
        bConnected = true;
    } else {
        ui->textEdit->append(QString("System :: connecting ERROR"));
        return;
    }
*/
    //timer->start();
    //bInited = false;
    //sendMessage(command_type::com_init);

    reconnect_timer = new QTimer;

    reconnect_timer->setInterval(10);
    reconnect_timer->start();
    connect(reconnect_timer,SIGNAL(timeout()),this,SLOT(slotReConnect()));
    connect(socket,SIGNAL(connected()),this,SLOT(slotStopReConnect()));
    connect(socket,SIGNAL(disconnected()),this,SLOT(slotStartReConnect()));









}
