#pragma once

#include "scan_summary/scan_summary_logger.h"

#include <QList>
#include <QString>
#include <QtLogging>

#include <gtest/gtest.h>

#include <chrono>

/// @brief Captures Qt log messages emitted by `ScanSummaryLogger` for inspection by tests.
class ScanSummaryLoggerTest : public ::testing::Test
{
protected:
    /// @brief Installs the fixture's Qt message handler before each test.
    void SetUp() override
    {
        activeFixture_ = this;
        previousMessageHandler_ = qInstallMessageHandler(captureMessage);
    }

    /// @brief Restores the previously installed Qt message handler after each test.
    void TearDown() override
    {
        qInstallMessageHandler(previousMessageHandler_);
        activeFixture_ = nullptr;
    }

    /// @brief Logs one scan result and returns its messages as separate ordered entries.
    ///
    /// @param scanResult Result whose summary is passed to `ScanSummaryLogger`.
    /// @return Complete ordered list of messages emitted while logging @p scanResult.
    [[nodiscard]] QStringList captureLogMessages(const ScanResult& scanResult)
    {
        capturedMessages_.clear();
        ScanSummaryLogger::log(scanResult);
        return capturedMessages_;
    }

private:
    /// @brief Appends a Qt log message to the currently active logger-test fixture.
    ///
    /// Qt requires this callback to be static. `activeFixture_` identifies the fixture that installed the
    /// handler and therefore owns the captured message list.
    static void captureMessage(const QtMsgType, const QMessageLogContext&, const QString& message)
    {
        if (activeFixture_)
        {
            activeFixture_->capturedMessages_.append(message);
        }
    }

    inline static ScanSummaryLoggerTest* activeFixture_{};
    QtMessageHandler previousMessageHandler_{};
    QStringList capturedMessages_;
};

/// @brief Verifies the exact count, order, and text of messages written for one scan summary.
///
/// @param actualMessages Messages captured from `ScanSummaryLogger`.
/// @param expectedMessages Complete ordered output required by the test scenario.
inline void verifyExactLogMessages(const QStringList& actualMessages, const QStringList& expectedMessages)
{
    ASSERT_EQ(actualMessages.size(), expectedMessages.size());

    for (qsizetype messageIndex = 0; messageIndex < expectedMessages.size(); ++messageIndex)
    {
        SCOPED_TRACE(testing::Message() << "Log message " << messageIndex);
        EXPECT_EQ(actualMessages.at(messageIndex).toStdString(), expectedMessages.at(messageIndex).toStdString());
    }
}

/// @brief Returns every captured log message beginning with the requested field label.
///
/// @param messages Messages captured from `ScanSummaryLogger`.
/// @param prefix Field label used to select matching messages.
/// @return Messages beginning with @p prefix, in their original order.
[[nodiscard]] inline QStringList findMessagesStartingWith(const QStringList& messages, const QString& prefix)
{
    QStringList matchingMessages;

    for (const QString& message: messages)
    {
        if (message.startsWith(prefix))
        {
            matchingMessages.append(message);
        }
    }

    return matchingMessages;
}

/// @brief Describes one byte-count value and its required user-facing representation.
struct ByteCountFormattingScenario
{
    const char* name;
    quint64 byteCount;
    const char* expectedFormattedByteCount;
};

/// @brief Provides byte-count formatting parameters together with Qt log capture.
class ScanSummaryByteCountFormattingTest : public ScanSummaryLoggerTest,
                                           public testing::WithParamInterface<ByteCountFormattingScenario>
{};

/// @brief Describes one duration value and its required user-facing representation.
struct DurationFormattingScenario
{
    const char* name;
    std::chrono::milliseconds duration;
    const char* expectedFormattedDuration;
};

/// @brief Provides duration-formatting parameters together with Qt log capture.
class ScanSummaryDurationFormattingTest : public ScanSummaryLoggerTest,
                                          public testing::WithParamInterface<DurationFormattingScenario>
{};
