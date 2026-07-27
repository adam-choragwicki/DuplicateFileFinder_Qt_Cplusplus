#include "scanner.h"
#include "scan_request.h"

#include <QDebug>

Scanner::Scanner(QObject* parent) : QObject(parent)
{
    progressTimer_.setInterval(150);

    connect(&progressTimer_, &QTimer::timeout, this, [this]
    {
        const int elapsedMilliseconds = qMin(static_cast<int>(elapsedTimer_.elapsed()), fakeScanDurationMilliseconds_);

        emit progressChanged(elapsedMilliseconds);

        if (elapsedMilliseconds >= fakeScanDurationMilliseconds_)
        {
            progressTimer_.stop();
            isScanning_ = false;

            qInfo() << "Scan operation complete";

            emit operationComplete();
        }
    });
}

void Scanner::scan(const ScanRequest& scanRequest)
{
    if (isScanning_)
    {
        qWarning() << "A scan operation is already in progress";
        return;
    }

    qDebug() << "Scan started";
    qDebug() << "Root directory path" << scanRequest.getRootDirectoryPath();
    qDebug() << "Scan type" << static_cast<int>(scanRequest.getScanType());

    isScanning_ = true;
    elapsedTimer_.start();
    emit progressChanged(0);
    progressTimer_.start();
}

bool Scanner::isScanning() const
{
    return isScanning_;
}

void Scanner::cancelScan()
{
    if (!isScanning_)
    {
        return;
    }

    progressTimer_.stop();
    isScanning_ = false;
    qInfo() << "Scan operation cancelled";
    emit operationCancelled();
}
