#pragma once

#include "scan_progress.h"
#include "types/duplicate_group.h"
#include "types/scan_result.h"

#include <gtest/gtest.h>

#include <QList>
#include <QString>
#include <QTemporaryDir>

#include <algorithm>
#include <variant>
#include <initializer_list>

/// Common scan workflow test fixture
class ScanWorkflowTest : public ::testing::Test
{
protected:
    void SetUp() override;

    [[nodiscard]] bool createDirectory(const QString& relativePath) const;
    [[nodiscard]] bool writeFile(const QString& relativePath, const QByteArray& contents) const;
    [[nodiscard]] QString getTemporaryDirectoryPath(const QString& relativePath) const;
    [[nodiscard]] QString getTemporaryScanRootPath() const;

    [[nodiscard]] static QString getSmokeTestScanRootPath();
    [[nodiscard]] static FileRecord createExpectedFileRecord(const QString& scanRootPath, const QString& relativeFilePath, qint64 expectedSizeBytes);
    [[nodiscard]] static DuplicateGroup createExpectedDuplicateGroup(std::initializer_list<FileRecord> expectedFiles);
    [[nodiscard]] static bool containsDuplicateGroup(const ScanResult& scanResult, const DuplicateGroup& expectedDuplicateGroup);

    template<typename Phase>
    [[nodiscard]] static QList<Phase> getPhaseTransitions(const QList<ScanProgress>& progressUpdates)
    {
        QList<Phase> phases;

        for (const ScanProgress& progress: progressUpdates)
        {
            const auto* phase = std::get_if<Phase>(&progress.scanPhase);

            if (phase && (phases.isEmpty() || phases.constLast() != *phase))
            {
                phases.append(*phase);
            }
        }

        return phases;
    }

    template<typename ScanPhase>
    [[nodiscard]] static bool containsOnlyPhasesFor(const QList<ScanProgress>& progressUpdates)
    {
        return std::all_of(
            progressUpdates.cbegin(),
            progressUpdates.cend(),
            [](const ScanProgress& scanProgress)
            {
                return std::holds_alternative<ScanPhase>(scanProgress.scanPhase);
            });
    }

    static const ScanProgressCallback ignoreProgressCallback;

private:
    [[nodiscard]] static bool matchesExpectedDuplicateGroup(const DuplicateGroup& actualDuplicateGroup, const DuplicateGroup& expectedDuplicateGroup);
    [[nodiscard]] static bool matchesExpectedFileRecord(const FileRecord& actualFile, const FileRecord& expectedFile);

    QTemporaryDir temporaryDirectory_;
};
