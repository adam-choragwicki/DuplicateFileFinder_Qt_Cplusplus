#pragma once

#include <QFile>
#include <QTextStream>

/// @brief Configures Qt message routing, severity filtering, and optional persistent log output.
///
/// Initialization installs a process-wide Qt message handler. The manager owns the active log file and stream for the
/// remainder of the process and can mirror accepted messages to the original console streams.
class LogManager
{
public:
    /// Selects the destination of accepted log messages.
    enum class Mode
    {
        /// Write messages only to the process console streams.
        LogToConsoleOnly,
        /// Write messages only to the application log file.
        LogToFileOnly,
        /// Write messages to both the log file and process console streams.
        LogToFileAndConsole
    };

    /// Sets the minimum Qt message severity accepted by the handler.
    enum class Verbosity
    {
        /// Accept debug messages and every more severe level.
        Debug,
        /// Accept informational messages and every more severe level.
        Info,
        /// Accept warning, critical, and fatal messages.
        Warning,
        /// Accept only critical and fatal messages.
        Error
    };

    /// Installs the Qt message handler and opens the log file when the selected mode requires one.
    /// @param[in] loggingMode Destination selection for accepted messages.
    /// @param[in] verbosity Minimum accepted message severity.
    static void initialize(Mode loggingMode = Mode::LogToFileAndConsole, Verbosity verbosity = Verbosity::Debug);

private:
    /// Filters, formats, and routes one message received from Qt's global logging system.
    static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);

    /// Returns the diagnostic name of a logging mode.
    static QString modeToString(Mode mode);
    /// Returns the diagnostic name of a verbosity level.
    static QString verbosityToString(Verbosity verbosity);

    /// Active destination selection used by messageHandler().
    static inline auto loggingMode_ = Mode::LogToFileAndConsole;
    /// Active severity threshold used by messageHandler().
    static inline auto verbosity_ = Verbosity::Debug;

    /// Owned application log file when file logging is enabled.
    static inline std::unique_ptr<QFile> logFile_;
    /// Text stream attached to logFile_ while file logging is active.
    static inline std::unique_ptr<QTextStream> logStream_;

    /// Enables source-file and line-number fields in formatted log messages.
    static constexpr bool ENABLE_CONTEXT_INFO = false;
};
