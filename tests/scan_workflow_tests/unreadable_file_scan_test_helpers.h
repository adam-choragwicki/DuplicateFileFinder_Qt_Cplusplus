#pragma once

#include <QFileDevice>
#include <QString>
#include <QStringList>
#include <QtLogging>

/// Makes a test file unreadable for the current user and restores access when it goes out of scope.
class ScopedUnreadableFile final
{
public:
    explicit ScopedUnreadableFile(QString filePath);

    ScopedUnreadableFile(const ScopedUnreadableFile&) = delete;
    ScopedUnreadableFile& operator=(const ScopedUnreadableFile&) = delete;

    ~ScopedUnreadableFile();

    [[nodiscard]] bool isUnreadable() const;

private:
    QString filePath_;
    QString userName_;
    QFileDevice::Permissions originalPermissions_{};
    bool accessRestricted_{};
};

/// Captures Qt log messages and restores the previous message handler when it goes out of scope.
class ScopedLogCapture final
{
public:
    ScopedLogCapture();

    ScopedLogCapture(const ScopedLogCapture&) = delete;
    ScopedLogCapture& operator=(const ScopedLogCapture&) = delete;

    ~ScopedLogCapture();

    [[nodiscard]] bool contains(const QString& text) const;

private:
    static void captureMessage(QtMsgType type, const QMessageLogContext& context, const QString& message);

    static ScopedLogCapture* activeCapture_;
    QtMessageHandler previousMessageHandler_{};
    QStringList messages_;
};
