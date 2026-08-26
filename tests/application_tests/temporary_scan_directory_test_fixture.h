#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <gtest/gtest.h>

/// @brief Provides application tests with an isolated temporary scan root and file-writing support.
class TemporaryScanDirectoryTest : public ::testing::Test
{
protected:
    /// @brief Verifies that the fixture's temporary directory was created successfully.
    void SetUp() override
    {
        ASSERT_TRUE(temporaryDirectory_.isValid());
    }

    /// @brief Creates or replaces a file below the fixture's temporary scan root.
    ///
    /// @param relativePath File path relative to the temporary scan root.
    /// @param contents Complete byte sequence to write to the file.
    /// @return `true` when all parent directories and file contents were written successfully.
    [[nodiscard]] bool writeFile(const QString& relativePath, const QByteArray& contents) const
    {
        const QString absoluteFilePath = temporaryDirectory_.filePath(relativePath);

        if (!QDir().mkpath(QFileInfo(absoluteFilePath).absolutePath()))
        {
            return false;
        }

        QFile outputFile(absoluteFilePath);
        return outputFile.open(QIODevice::WriteOnly)
               && outputFile.write(contents) == contents.size();
    }

    /// @brief Returns the absolute path of the fixture's temporary scan root.
    [[nodiscard]] QString scanRootPath() const
    {
        return temporaryDirectory_.path();
    }

private:
    QTemporaryDir temporaryDirectory_;
};
