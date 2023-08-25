#include "server.h"
#include "ui_server.h"
#include <QMessageBox>

#include <QSettings>
#include <QTextCodec>
#include <QDataStream>
#include <QFile>
#include <QFileDialog>

Server::Server(QMainWindow *parent)
    : QMainWindow(parent)
    , ui(new Ui::Server)
{
    ui->setupUi(this);
    m_started = false;

    this->resize(473,599);
    bSharedWindow = false;

    QString SettingsName = "config.ini";
    QSettings settings(SettingsName, QSettings::IniFormat);
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));

    settings.beginGroup("MAIN_SETTINGS");
    ui->linePort->setText((settings.value("ServerPort", 9999)).toString());
    ui->lineMain->setText((settings.value("MainNode", 2)).toString());
    ui->lineCurrent->setText((settings.value("NodeId", 3)).toString());
    ui->lineFrame->setText((settings.value("FrameType", 0)).toString());
    settings.endGroup();

    if(QFile::exists("pack_param.pnf")) {

        QFile file;
        file.setFileName("pack_param.pnf");
        if(file.open(QFile::ReadOnly))  {
            QTextStream out(&file);
            out.setCodec(QTextCodec::codecForName("UTF-8"));
            QString line;
            ui->lineCommandPack->setText(out.readLine());
            while (out.readLineInto(&line)) {
                ui->listWidget->addItem(line.toUtf8());
                line.clear();
            }
        }
        file.close();
    }


}

Server::~Server()   {
    foreach (QTcpSocket* socket, connection_set)    {
        socket->close();
        socket->deleteLater();
    }
    m_server->close();
    m_server->deleteLater();
    delete ui;
}

void Server::newConnection()    {
    while (m_server->hasPendingConnections())   {
        appendToSocketList(m_server->nextPendingConnection());
    }
}

void Server::appendToSocketList(QTcpSocket* socket) {
    connection_set.insert(socket);
    connect(socket, &QTcpSocket::readyRead, this, &Server::network);
    connect(socket, &QTcpSocket::disconnected, this, &Server::discardSocket);
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(getErrorCode(QAbstractSocket::SocketError)));
    ui->textEdit->append(QString("Node %1 descriptor has connected").arg(socket->socketDescriptor()));
}

void Server::getErrorCode(QAbstractSocket::SocketError errorCode)   {
    qDebug() << "Socket Error = " << errorCode;
}

void Server::network() {

    QTcpSocket* socket = static_cast<QTcpSocket*>(sender());

    if( socket->state() == QAbstractSocket::ConnectedState )    {

        bufferRead = socket->readAll();

        //qDebug() << "Length: " << bufferRead.length();
        //qDebug() << bufferRead;

        if(bInited == false) {

            int MainNode  = ui->lineMain->text().toInt();
            int NodeID    = ui->lineCurrent->text().toInt();
            int FrameType = 127;
            QString name  = "127.0.0.1";

            dataSend.clear();

            dataSend.append((char) 0x00);
            dataSend.append((char) 13 + name.size());
            dataSend.append((char) MainNode);
            dataSend.append((char) NodeID);
            dataSend.append((char) 0x00);
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

            //qDebug() << "Init command is sent";
            ui->textEdit->append(QString("S -->> : init command"));

            socket->write(dataSend);
            socket->flush();
            socket->waitForBytesWritten(10000);
        }

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

                    ui->textEdit->append(QString("<<-- C :: %1 %2 %3").arg(iCommand).arg(iPar1).arg(iPar2));
                }
            } else {
                // try to unpack big package
            }
        }
    }
}

void Server::discardSocket()    {
    QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());
    QSet<QTcpSocket*>::iterator it = connection_set.find(socket);
    if (it != connection_set.end()){
        //displayMessage(QString("INFO :: Node has disconnected").arg(socket->socketDescriptor()));
        ui->textEdit->append(QString("Node has disconnected").arg(socket->socketDescriptor()));
        connection_set.remove(*it);
    }
    socket->deleteLater();
    bInited = false;
}

void Server::sendMessage(QTcpSocket* socket, command_type type)    {
    switch(type)    {
        case com_init:
            if(socket)  {
                if(socket->isOpen())    {

                    QDataStream socketStream(socket);
                    socketStream.setVersion(QDataStream::Qt_4_0);

                    int MainNode  = ui->lineMain->text().toInt();
                    int NodeID    = ui->lineCurrent->text().toInt();
                    int FrameType = 127;

                    QString name = "127.0.0.1";

                    QByteArray byteArray;

                    byteArray.append((char) 0x00);
                    byteArray.append((char) 13 + name.size());
                    byteArray.append((char) MainNode);
                    byteArray.append((char) NodeID);
                    byteArray.append((char) 0x00);
                    byteArray.append((char) FrameType);
                    byteArray.append((char) 0x00);
                    byteArray.append((char) 0x00);

                    byteArray.append((char) 0xC0);
                    byteArray.append((char) 0x00);
                    byteArray.append((char) 0x00);
                    byteArray.append((char) 0x03);

                    byteArray.append(name);
                    byteArray.append((char) 0x00);

                    socketStream << byteArray;
                    socket->write(byteArray);
                    socket->flush();
                    ui->textEdit->append(QString("S -->> : init command"));
                } else {
                    //QMessageBox::critical(this,"QTCPClient","Socket doesn't seem to be opened");
                    ui->textEdit->append(QString("Socket doesn't seem to be opened"));
                }
            } else {
                //QMessageBox::critical(this,"QTCPClient","Not connected");
                ui->textEdit->append(QString("Not connected"));
            }
            //qDebug() << "Init message is sent";
        break;
        case com_ping: {
            dataSend.clear();

            int MainNode  = ui->lineMain->text().toInt();
            int NodeID    = ui->lineCurrent->text().toInt();
            int FrameType = ui->lineFrame->text().toInt();

            // 8 байт - заголовок кадра
            dataSend.append((char) 0x00);           // 1 байт - 0
            dataSend.append((char) 0x08);           // 2 байт - текущая длинна кадра в байтах (включая размер заголовка)
            dataSend.append((char) MainNode);       // 3 байт - идентификатор получателя
            dataSend.append((char) NodeID);         // 4 байт - идентификатор отправителя (номер узла)
            dataSend.append((char) 0x00);           // 5 байт - флаг кадра ( 0 - я сервер, 1 - я клинет )
            dataSend.append((char) FrameType);      // 6 байт - тип кадра 0
            dataSend.append((char) 0x00);           // 7 байт - зарезервировано
            dataSend.append((char) 0x00);           // 8 байт - зарезервировано
            socket->write(dataSend);
            socket->flush();
            socket->waitForBytesWritten(10000);
        } break;
        case com_buffer:
            if(bufferSend.size() == 12){

                dataSend.clear();

                int MainNode  = ui->lineMain->text().toInt();
                int NodeID    = ui->lineCurrent->text().toInt();
                int FrameType = ui->lineFrame->text().toInt();

                // 8 байт - заголовок кадра
                dataSend.append((char) 0x00);           // 1 байт - 0
                dataSend.append((char) 0x18);           // 2 байт - текущая длинна кадра в байтах (включая размер заголовка)
                dataSend.append((char) MainNode);       // 3 байт - идентификатор получателя
                dataSend.append((char) NodeID);         // 4 байт - идентификатор отправителя (номер узла)
                dataSend.append((char) 0x00);           // 5 байт - флаг кадра ( 0 - я сервер, 1 - я клинет )
                dataSend.append((char) FrameType);      // 6 байт - тип кадра 0
                dataSend.append((char) 0x00);           // 7 байт - зарезервировано
                dataSend.append((char) 0x00);           // 8 байт - зарезервировано

                // 4 байта - модельное время команды в миллисекундах
                bufferSend.append((char) 0x00);
                bufferSend.append((char) 0x00);
                bufferSend.append((char) 0x00);
                bufferSend.append((char) 0x00);
                // модельное время команды в миллисекундах

                //qDebug() << "bufferSend.length()" << bufferSend.length() << "bufferSend" << bufferSend;
                dataSend.append(bufferSend);
                //dataSend[1] = (char) 0x18;     // меняем длинну кадра на 24
                //dataSend[5] = (char) 0x7F;  // меняем тип кадра на 126
                bufferSend.clear(); // очищаем буфер
            }

            socket->write(dataSend);
            socket->flush();
            socket->waitForBytesWritten(10);
        break;
        case com_package:

            dataSend.clear();

            int MainNode  = ui->lineMain->text().toInt();
            int NodeID    = ui->lineCurrent->text().toInt();
            int FrameType = ui->lineFrame->text().toInt();

            // 8 байт - заголовок кадра
            dataSend.append((char) 0x00);           // 1 байт - 0
            dataSend.append((char) (8+bufferSend.size()+16));           // 2 байт - текущая длинна кадра в байтах (включая размер заголовка)
            dataSend.append((char) MainNode);       // 3 байт - идентификатор получателя
            dataSend.append((char) NodeID);         // 4 байт - идентификатор отправителя (номер узла)
            dataSend.append((char) 0x00);           // 5 байт - флаг кадра ( 0 - я сервер, 1 - я клинет )
            dataSend.append((char) FrameType);      // 6 байт - тип кадра 0
            dataSend.append((char) 0x00);           // 7 байт - зарезервировано
            dataSend.append((char) 0x00);           // 8 байт - зарезервировано
/*
            // 4 байта - модельное время команды в миллисекундах
            bufferSend.append((char) 0x00);
            bufferSend.append((char) 0x00);
            bufferSend.append((char) 0x00);
            bufferSend.append((char) 0x00);
            // модельное время команды в миллисекундах
*/
            //qDebug() << "bufferSend.length()" << bufferSend.length() << "bufferSend" << bufferSend;
            dataSend.append(bufferSend);
            //dataSend[1] = (char) 0x18;     // меняем длинну кадра на 24
            //dataSend[5] = (char) 0x7F;  // меняем тип кадра на 126
            bufferSend.clear(); // очищаем буфер


            socket->write(dataSend);
            socket->flush();
            socket->waitForBytesWritten(10);



        break;
    }
}

void Server::OnTimer()  {
    //this->setWindowTitle(QString("Connection count = %1").arg(connection_set.size()));

    for(auto sock : connection_set) {
        sendMessage(sock, command_type::com_ping);
    }
}

void Server::addCommand(command_t command)  {

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

    ui->textEdit->append(QString("S -->> :: %1 %2 %3").arg(command.code).arg(command.par1).arg(command.par2));
    for(auto sock : connection_set) {
        sendMessage(sock, command_type::com_buffer);
    }
}

void Server::addPackage(QByteArray package) {
    for(auto sock : connection_set) {
        bufferSend.clear();
        bufferSend.append(package);
        sendMessage(sock, com_package);
    }
}

void Server::displayMessage(const QString& str)
{
    qDebug() << str;
}

void Server::on_pushButton_clicked()    {
    qint32 code = ui->lineCommand->text().toInt();
    qint32 par1 = ui->linePar1->text().toInt();
    qint32 par2 = ui->linePar2->text().toInt();
    command_t command = { code, par1, par2, 0};
    addCommand(command);
}

void Server::on_pushButton_2_clicked()
{
    if(isStarted()) {
        timer->stop();
        delete timer;
        foreach (QTcpSocket* socket, connection_set)    {
            socket->close();
            socket->deleteLater();
        }
        m_server->close();
        m_started = false;
        ui->pushButton_2->setText("Старт сервера");
        ui->textEdit->append(QString("System :: server is stopped..."));
    } else {
        m_server = new QTcpServer(this);

        if(m_server->listen(QHostAddress::Any, ui->linePort->text().toInt()))   {
           connect(this, &Server::newMessage, this, &Server::displayMessage);
           connect(m_server, &QTcpServer::newConnection, this, &Server::newConnection);
           ui->textEdit->append(QString("System :: server is listening..."));
        } else {
            ui->textEdit->append(QString("System :: unable to start server. Code = %1").arg(m_server->errorString()));
            exit(EXIT_FAILURE);
        }

        timer = new QTimer;
        timer->setInterval(500);
        connect(timer,SIGNAL(timeout()),this,SLOT(OnTimer()));
        timer->start();

        bInited = false;
        m_started = true;
        ui->pushButton_2->setText("Стоп сервера");

        QString SettingsName = "config.ini";
        QSettings settings(SettingsName, QSettings::IniFormat);
        settings.setIniCodec(QTextCodec::codecForName("UTF-8"));

        settings.beginGroup("MAIN_SETTINGS");
        settings.setValue("ServerPort",ui->linePort->text().toInt());
        settings.setValue("MainNode",ui->lineMain->text().toInt());
        settings.setValue("NodeId",ui->lineCurrent->text().toInt());
        settings.setValue("FrameType",ui->lineFrame->text().toInt());
        settings.endGroup();

    }
}

void Server::on_pushButton_3_clicked()  {

    if(bSharedWindow)   {
        this->resize(473,599);
        bSharedWindow = false;
    } else {
        this->resize(800,599);
        bSharedWindow = true;
    }
}

void Server::on_pushButton_4_clicked()  {
    if(!m_started)  {
        ui->textEdit->append("not sended :: server isnt started");
        return;
    }

    QByteArray pack;
    pack.clear();

    bool ok;
    qint32 code = ui->lineCommandPack->text().toInt(&ok,16);

    QDataStream stream(&pack,QIODevice::WriteOnly);

    for(int i = 0; i < ui->listWidget->count(); i++)    {
        QString s;
        s.clear();
        s = ui->listWidget->item(i)->text();
        if(s.contains('.')) {
            double buff = s.toDouble();
            stream << buff;
            qDebug() << buff;
        } else {
            qint32 buff = s.toInt();
            stream << buff;
            qDebug() << buff;
        }
    }

    QByteArray arr;
    arr.clear();

    QDataStream arrstr(&arr,QIODevice::WriteOnly);
    arrstr << code;
    arrstr << pack.size();
    arrstr << 0;
    arrstr << 0;

    pack.prepend(arr);

    addPackage(pack);
}

void Server::on_pushButton_5_clicked()  {
    ui->listWidget->addItem(ui->linePackParam->text());
    ui->linePackParam->clear();
    ui->linePackParam->setFocus();
}

void Server::on_pushButton_6_clicked()  {
    ui->listWidget->takeItem(ui->listWidget->currentRow());
}

void Server::on_pushButton_7_clicked()  {
    QFile file;
    file.setFileName("pack_param.pnf");
    file.open(QIODevice::WriteOnly);
    QTextStream out(&file);
    out.setCodec(QTextCodec::codecForName("UTF-8"));
    out << ui->lineCommandPack->text();

    if(ui->listWidget->count() > 0)
        out << "\n";

    for(int i = 0; i < ui->listWidget->count(); i++)    {
        out << ui->listWidget->item(i)->text();
        if(i < ui->listWidget->count() - 1)
            out << "\n";

    }
    file.close();
}
