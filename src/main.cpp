#include "mainwindow.h"
#include <QApplication>

#include <QSettings>

#ifdef USE_PYLON
#include <pylon/PylonIncludes.h>
#endif

#include "execArgParser.h"

// Stream operator for custom types needed to save those types to QTs application settings structure
#ifndef QT_NO_DATASTREAM
QDataStream &operator<<(QDataStream &out, const QList<QPair<QString,QString>> &l) {
    int s = l.size();
    out << s;
    if (s) for (int i = 0; i < s; ++i) out << l[i].first << l[i].second;
    return out;
}
QDataStream &operator>>(QDataStream &in, QList<QPair<QString,QString>> &l) {
    if (!l.empty()) l.clear();
    int s = 0;
    in >> s;
    if (s) {
        l.reserve(s);
        for (int i = 0; i < s; ++i) {
            QString f, sec;
            in >> f >> sec;
            l.append(QPair<QString, QString>(f, sec));
        }
    }
    return in;
}

QDataStream &operator<<(QDataStream &stream, const QMap<QString, QList<float>> &map)
{
    QMapIterator<QString, QList<float>> i(map);
    while (i.hasNext()) {
        i.next();
        stream << i.key() << i.value();
    }
    return stream;
}
QDataStream &operator>>(QDataStream &stream, QMap<QString, QList<float>> &map)
{
    while(!stream.atEnd()) {
        QString key;
        QList<float> value;
        stream >> key;
        stream >> value;
        if(!key.isEmpty())
            map[key] = value;
    }
    return stream;
}
#endif


int main(int argc, char *argv[])
{
    try {

        qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &msg){
            QFile logFile("lastrun.txt");
            if (logFile.open(QIODevice::WriteOnly)) {
                QTextStream out(&logFile);
                out << msg << "\n";
            }
        });

        int result = 0;
        do {
            QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
            QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
            QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

            QApplication a(argc, argv);

            // Changing this may change the settings path thus not loading old application settings!
            QCoreApplication::setOrganizationName("FGLT");
            QCoreApplication::setApplicationName("PupilEXT");
            QCoreApplication::setApplicationVersion("0.1.3 Beta");

            // TODO 2025.07.09: something with QMap<QString,QList<float>> in LLDB
            // NOTE: these calls will not be needed in Qt6, only in Qt5
            //qRegisterMetaTypeStreamOperators<QMap<QString, QList<float>>>("QMap<QString,QList<float>>");
            //qRegisterMetaTypeStreamOperators<QList<QPair<QString, QString>>>("QList<QPair<QString, QString>>");

#ifdef USE_PYLON
            Pylon::PylonAutoInitTerm autoInitTerm;  // PylonInitialize() will be called here
#endif

            // Test exec args:
            int argcT = 14;
            char* argvT[16];

            argvT[0] = (char*)"blabla";
            argvT[1] = (char*)"-setAcquisitionTriggeringMode";
            argvT[2] = (char*)"S";
            argvT[3] = (char*)"-setExposureTimeMicrosec";
            argvT[4] = (char*)"10000";
            argvT[5] = (char*)"-setGain";
            argvT[6] = (char*)"15";
            argvT[7] = (char*)"-setSoftwareTriggeringFramerate";
            argvT[8] = (char*)"50";
            argvT[9] = (char*)"-openSingleCamera";
            argvT[10] = (char*)"Basler acA1300-60gm (22385468)";
            argvT[11] = (char*)"-connectRemoteUDP";
            argvT[12] = (char*)"0.0.0.0:6900";
            argvT[13] = (char*)"-setImageOutputFormat";
            argvT[14] = (char*)"jpeg";
            argvT[15] = nullptr;

            //char *argvT[] = {, , };
            ExecArgParser* execArgParser = new ExecArgParser(argcT, argvT);

            // To be able to interpret start arguments (supplied through command line startup, via e.g. .lnk icons in windows OS with arguments, or batch file exe call)
            // useful e.g. in case of automatic exec on scheduled PC startup for warming up camera device before experimental session
 //           ExecArgParser* execArgParser = new ExecArgParser(argc, argv);

            MainWindow w;
            w.setWindowIcon(QIcon(":/icon.svg"));
            w.show();

            execArgParser->connectMainWindow(&w);
            execArgParser->iterThroughDuties();

            result = a.exec();
        } while( result == MainWindow::EXIT_CODE_REBOOT );
        return result;

    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
        throw;
    }
    return -1;
}
