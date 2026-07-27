#include "scanner.h"
#include "scan_request.h"
#include <QDebug>

void Scanner::scan(const ScanRequest& scanRequest)
{
    qDebug() << "Scan started";
    qDebug() << "Root directory path" << scanRequest.getRootDirectoryPath();
    qDebug() << "Scan type" << static_cast<int>(scanRequest.getScanType());
}
