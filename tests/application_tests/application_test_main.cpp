#include <QApplication>

#include <gtest/gtest.h>

int main(int argc, char* argv[])
{
    // Prevent application tests from creating native windows, regardless of how the test executable is launched.
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));

    QApplication application(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
