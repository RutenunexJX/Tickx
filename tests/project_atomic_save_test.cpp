#include "wave/model.h"
#include "wave/project_io.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTemporaryDir>
#include <QTest>

#include <string>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
#include <chrono>
#include <set>
#include <thread>
#endif

namespace {

QByteArray readBytes(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

QStringList directoryEntries(const QString& directory)
{
    return QDir(directory).entryList(
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDir::Name);
}

wave::Project replacementProject(const wave::Project& original, const int generation)
{
    auto result = original;
    result.name = "Atomic save generation " + std::to_string(generation);
    result.scenarios.front().name = "Scenario generation " + std::to_string(generation);
    return result;
}

#ifdef Q_OS_WIN
class NativeHandle {
public:
    explicit NativeHandle(HANDLE handle = INVALID_HANDLE_VALUE) : handle_(handle) {}
    ~NativeHandle() { reset(); }
    NativeHandle(const NativeHandle&) = delete;
    NativeHandle& operator=(const NativeHandle&) = delete;

    [[nodiscard]] HANDLE get() const { return handle_; }
    [[nodiscard]] bool valid() const
    {
        return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr;
    }

    void reset(const HANDLE handle = INVALID_HANDLE_VALUE)
    {
        if (valid()) {
            CloseHandle(handle_);
        }
        handle_ = handle;
    }

private:
    HANDLE handle_;
};

HANDLE openWithoutDeleteShare(const QString& path)
{
    return CreateFileW(
        reinterpret_cast<LPCWSTR>(path.utf16()),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
}

// Release only after the first failed attempt has removed its temporary file.
// This synchronizes on filesystem behavior, without predicting serialization
// speed or how long the first commit takes on the machine running the test.
class UnlockAfterFailedAttempt {
public:
    ~UnlockAfterFailedAttempt()
    {
        if (stopEvent_.valid()) {
            SetEvent(stopEvent_.get());
        }
        if (worker_.joinable()) {
            worker_.join();
        }
        cancelPendingNotification();
    }

    bool start(const QString& path)
    {
        target_.reset(openWithoutDeleteShare(path));
        if (!target_.valid()) {
            error_ = GetLastError();
            return false;
        }
        const auto directoryPath = QFileInfo(path).absolutePath();
        directory_.reset(CreateFileW(
            reinterpret_cast<LPCWSTR>(directoryPath.utf16()),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr));
        if (!directory_.valid()) {
            error_ = GetLastError();
            return false;
        }
        changedEvent_.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        stopEvent_.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!changedEvent_.valid() || !stopEvent_.valid()) {
            error_ = GetLastError();
            return false;
        }
        targetName_ = QFileInfo(path).fileName();
        if (!armNotification()) {
            return false;
        }
        worker_ = std::jthread([this] { observeRemoval(); });
        return true;
    }

    void join()
    {
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    [[nodiscard]] bool releasedAfterRemoval() const { return releasedAfterRemoval_; }
    [[nodiscard]] DWORD error() const { return error_; }

private:
    bool armNotification()
    {
        ResetEvent(changedEvent_.get());
        overlapped_ = {};
        overlapped_.hEvent = changedEvent_.get();
        pending_ = ReadDirectoryChangesW(
            directory_.get(),
            changes_.data(),
            static_cast<DWORD>(changes_.size()),
            FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME,
            nullptr,
            &overlapped_,
            nullptr);
        if (!pending_) {
            error_ = GetLastError();
        }
        return pending_;
    }

    void observeRemoval()
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        const HANDLE events[]{stopEvent_.get(), changedEvent_.get()};
        std::set<QString> createdNames;
        while (pending_) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (remaining <= 0) {
                error_ = WAIT_TIMEOUT;
                break;
            }
            const auto wait = WaitForMultipleObjects(
                2, events, FALSE, static_cast<DWORD>(remaining));
            if (wait != WAIT_OBJECT_0 + 1) {
                error_ = wait == WAIT_FAILED ? GetLastError() : wait;
                break;
            }
            DWORD byteCount = 0;
            const auto completed = GetOverlappedResult(
                directory_.get(), &overlapped_, &byteCount, FALSE);
            pending_ = false;
            if (!completed || byteCount == 0) {
                error_ = completed ? ERROR_NOTIFY_ENUM_DIR : GetLastError();
                break;
            }
            const auto* change = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                changes_.data());
            for (;;) {
                const auto name = QString::fromWCharArray(
                    change->FileName,
                    static_cast<int>(change->FileNameLength / sizeof(WCHAR)));
                if (name != targetName_ && change->Action == FILE_ACTION_ADDED) {
                    createdNames.insert(name);
                }
                if (change->Action == FILE_ACTION_REMOVED && createdNames.contains(name)) {
                    target_.reset();
                    releasedAfterRemoval_ = true;
                    return;
                }
                if (change->NextEntryOffset == 0) {
                    break;
                }
                change = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<const char*>(change) + change->NextEntryOffset);
            }
            if (!armNotification()) {
                break;
            }
        }
        cancelPendingNotification();
    }

    void cancelPendingNotification()
    {
        if (!pending_) {
            return;
        }
        CancelIoEx(directory_.get(), &overlapped_);
        DWORD ignored = 0;
        GetOverlappedResult(directory_.get(), &overlapped_, &ignored, TRUE);
        pending_ = false;
    }

    NativeHandle target_;
    NativeHandle directory_;
    NativeHandle changedEvent_;
    NativeHandle stopEvent_;
    alignas(DWORD) std::array<char, 16'384> changes_{};
    OVERLAPPED overlapped_{};
    QString targetName_;
    bool pending_{false};
    bool releasedAfterRemoval_{false};
    DWORD error_{ERROR_SUCCESS};
    std::jthread worker_;
};
#endif

} // namespace

class ProjectAtomicSaveTest : public QObject {
    Q_OBJECT

private slots:
    void createsAndReplacesCompleteProject()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(QStringLiteral("project.wave.json"));
        const auto original = wave::makeDemonstrationProject();
        const auto replacement = replacementProject(original, 1);
        QString error;

        QVERIFY2(wave::saveProjectFileAtomic(original, path, &error), qPrintable(error));
        QCOMPARE(readBytes(path), wave::serializeProject(original));
        QVERIFY2(wave::saveProjectFileAtomic(replacement, path, &error), qPrintable(error));
        QCOMPARE(readBytes(path), wave::serializeProject(replacement));
        QCOMPARE(directoryEntries(directory.path()), QStringList{QStringLiteral("project.wave.json")});
    }

    void persistentLockPreservesOriginalAndAllowsLaterSave()
    {
#ifdef Q_OS_WIN
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(QStringLiteral("project.wave.json"));
        const auto original = wave::makeDemonstrationProject();
        const auto replacement = replacementProject(original, 2);
        QString error;
        QVERIFY2(wave::saveProjectFileAtomic(original, path, &error), qPrintable(error));
        const auto entriesBefore = directoryEntries(directory.path());

        NativeHandle lock(openWithoutDeleteShare(path));
        QVERIFY2(lock.valid(), qPrintable(QStringLiteral("CreateFileW failed: %1").arg(GetLastError())));
        QVERIFY(!wave::saveProjectFileAtomic(replacement, path, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(readBytes(path), wave::serializeProject(original));
        QCOMPARE(directoryEntries(directory.path()), entriesBefore);

        lock.reset();
        QVERIFY2(wave::saveProjectFileAtomic(replacement, path, &error), qPrintable(error));
        QCOMPARE(readBytes(path), wave::serializeProject(replacement));
        QCOMPARE(directoryEntries(directory.path()), entriesBefore);
#else
        QSKIP("Windows delete-sharing semantics are required.");
#endif
    }

    void transientLockIsRecoveredWithinSingleSaveCall()
    {
#ifdef Q_OS_WIN
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(QStringLiteral("project.wave.json"));
        const auto original = wave::makeDemonstrationProject();
        const auto replacement = replacementProject(original, 3);
        QString error;
        QVERIFY2(wave::saveProjectFileAtomic(original, path, &error), qPrintable(error));
        const auto entriesBefore = directoryEntries(directory.path());

        UnlockAfterFailedAttempt unlocker;
        if (!unlocker.start(path)) {
            QFAIL(qPrintable(QStringLiteral("Native notification setup failed: %1").arg(unlocker.error())));
        }
        // One call only: the fixture releases its lock after a failed attempt,
        // and recovery must happen inside the shared save implementation.
        const bool saved = wave::saveProjectFileAtomic(replacement, path, &error);
        unlocker.join();
        QVERIFY2(unlocker.releasedAfterRemoval(),
            qPrintable(QStringLiteral("No failed temporary-file removal observed: %1").arg(unlocker.error())));
        QVERIFY2(saved, qPrintable(error));
        QCOMPARE(readBytes(path), wave::serializeProject(replacement));
        QCOMPARE(directoryEntries(directory.path()), entriesBefore);
#else
        QSKIP("Windows delete-sharing semantics are required.");
#endif
    }

    void repeatedReplacementsWithFileAndDirectoryWatchers()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(QStringLiteral("project.wave.json"));
        const auto original = wave::makeDemonstrationProject();
        QString error;
        QVERIFY2(wave::saveProjectFileAtomic(original, path, &error), qPrintable(error));

        QFileSystemWatcher watcher;
        QVERIFY(watcher.addPath(directory.path()));
        QVERIFY(watcher.addPath(path));
        int fileChanges = 0;
        int directoryChanges = 0;
        bool rearmFailed = false;
        const auto rearmFile = [&] {
            if (!watcher.files().contains(path) && !watcher.addPath(path)) {
                rearmFailed = true;
            }
        };
        connect(&watcher, &QFileSystemWatcher::fileChanged, this, [&](const QString&) {
            ++fileChanges;
            rearmFile();
        });
        connect(&watcher, &QFileSystemWatcher::directoryChanged, this, [&](const QString&) {
            ++directoryChanges;
            rearmFile();
        });

        for (int generation = 1; generation <= 128; ++generation) {
            const auto replacement = replacementProject(original, generation);
            const auto saved = wave::saveProjectFileAtomic(replacement, path, &error);
            QVERIFY2(saved, qPrintable(QStringLiteral("Replacement %1: %2").arg(generation).arg(error)));
            QCOMPARE(readBytes(path), wave::serializeProject(replacement));
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            rearmFile();
            QVERIFY(!rearmFailed);
        }
        QTRY_VERIFY_WITH_TIMEOUT(fileChanges > 0 && directoryChanges > 0, 3'000);
        QVERIFY(!rearmFailed);
        QCOMPARE(readBytes(path), wave::serializeProject(replacementProject(original, 128)));
        QCOMPARE(directoryEntries(directory.path()), QStringList{QStringLiteral("project.wave.json")});
    }
};

QTEST_GUILESS_MAIN(ProjectAtomicSaveTest)
#include "project_atomic_save_test.moc"
