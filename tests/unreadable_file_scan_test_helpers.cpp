#include "unreadable_file_scan_test_helpers.h"

#include <QDir>
#include <QFile>

#include <utility>

#if defined(Q_OS_WIN)
#include <QProcess>
#endif

namespace
{
#if defined(Q_OS_WIN)
    bool runIcacls(const QStringList& arguments)
    {
        QProcess process;
        process.start(QStringLiteral("icacls.exe"), arguments);

        return process.waitForFinished() && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }
#endif
}

ScopedUnreadableFile::ScopedUnreadableFile(QString filePath) : filePath_(std::move(filePath))
{
#if defined(Q_OS_WIN)
    userName_ = qEnvironmentVariable("USERNAME");
    accessRestricted_ = !userName_.isEmpty()
                        && runIcacls({
                            QDir::toNativeSeparators(filePath_),
                            QStringLiteral("/deny"),
                            QStringLiteral("%1:R").arg(userName_)
                        });
#else
    originalPermissions_ = QFile::permissions(filePath_);
    accessRestricted_ = QFile::setPermissions(filePath_, QFileDevice::Permissions{});
#endif
}

ScopedUnreadableFile::~ScopedUnreadableFile()
{
#if defined(Q_OS_WIN)
    if (accessRestricted_)
    {
        runIcacls({
            QDir::toNativeSeparators(filePath_),
            QStringLiteral("/remove:d"),
            userName_
        });
    }
#else
    if (accessRestricted_)
    {
        QFile::setPermissions(filePath_, originalPermissions_);
    }
#endif
}

bool ScopedUnreadableFile::isUnreadable() const
{
    QFile file(filePath_);
    return !file.open(QIODevice::ReadOnly);
}

ScopedLogCapture* ScopedLogCapture::activeCapture_ = nullptr;

ScopedLogCapture::ScopedLogCapture()
{
    Q_ASSERT(activeCapture_ == nullptr);
    activeCapture_ = this;
    previousMessageHandler_ = qInstallMessageHandler(captureMessage);
}

ScopedLogCapture::~ScopedLogCapture()
{
    qInstallMessageHandler(previousMessageHandler_);
    activeCapture_ = nullptr;
}

bool ScopedLogCapture::contains(const QString& text) const
{
    for (const QString& message: messages_)
    {
        if (message.contains(text))
        {
            return true;
        }
    }

    return false;
}

void ScopedLogCapture::captureMessage(const QtMsgType, const QMessageLogContext&, const QString& message)
{
    if (activeCapture_)
    {
        activeCapture_->messages_.append(message);
    }
}
