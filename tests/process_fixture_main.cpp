#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#ifndef WAVE_TOOLCHAIN_FIXTURE_MODE
#error WAVE_TOOLCHAIN_FIXTURE_MODE must be defined
#endif

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTextStream output(stdout);
    QTextStream error(stderr);
#if WAVE_TOOLCHAIN_FIXTURE_MODE == 1
    output << "Verilator 5.028 2024-08-21 rev test\n";
    error << "g++ (GCC) 13.2.0\nfixture-stderr\n";
    output.flush();
    error.flush();
    return 0;
#elif WAVE_TOOLCHAIN_FIXTURE_MODE == 2
    output << "Verilator 4.228 2023-08-21 rev old\n";
    error << "g++ (GCC) 7.5.0\n";
    output.flush();
    error.flush();
    return 0;
#elif WAVE_TOOLCHAIN_FIXTURE_MODE == 3
    output << "fixture-output-before-failure\n";
    error << "fixture-error-before-failure\n";
    output.flush();
    error.flush();
    return 19;
#elif WAVE_TOOLCHAIN_FIXTURE_MODE == 4
    output << "fixture-started\n";
    error << "fixture-waiting\n";
    output.flush();
    error.flush();
    QTimer::singleShot(60'000, &application, &QCoreApplication::quit);
    return application.exec();
#elif WAVE_TOOLCHAIN_FIXTURE_MODE == 5
    const auto arguments = application.arguments();
    if (arguments.contains(QStringLiteral("--version"))) {
        output << "Verilator 5.028 2024-08-21 rev simulation-fixture\n";
        output.flush();
        return 0;
    }
    if (qEnvironmentVariable("WAVE_VERILATOR_FIXTURE_BUILD_FAIL") == QStringLiteral("1")) {
        error << "fixture Verilator build failure\n";
        error.flush();
        return 23;
    }
    bool buildDelayOk = false;
    const auto buildDelayMs = qEnvironmentVariableIntValue(
        "WAVE_VERILATOR_FIXTURE_BUILD_DELAY_MS", &buildDelayOk);
    if (buildDelayOk && buildDelayMs > 0) {
        QThread::msleep(static_cast<unsigned long>(buildDelayMs));
    }
    const auto countPath = qEnvironmentVariable("WAVE_VERILATOR_FIXTURE_COUNT_FILE");
    if (!countPath.isEmpty()) {
        QFile countFile(countPath);
        if (!countFile.open(QIODevice::WriteOnly | QIODevice::Append)
            || countFile.write("build\n") != 6) {
            error << "fixture Verilator could not record the build\n";
            error.flush();
            return 28;
        }
    }
    QString objectDirectory;
    QString executableName;
    for (qsizetype index = 0; index + 1 < arguments.size(); ++index) {
        if (arguments[index] == QStringLiteral("--Mdir")) {
            objectDirectory = arguments[index + 1];
        } else if (arguments[index] == QStringLiteral("-o")) {
            executableName = arguments[index + 1];
        }
    }
    const auto simulatorFixture = qEnvironmentVariable("WAVE_SIMULATOR_FIXTURE");
    if (objectDirectory.isEmpty() || executableName.isEmpty()
        || !QFileInfo(simulatorFixture).isFile()
        || !QDir().mkpath(objectDirectory)) {
        error << "fixture Verilator received an invalid build contract\n";
        error.flush();
        return 24;
    }
    const auto destination = QDir(objectDirectory).filePath(executableName);
    QFile::remove(destination);
    if (!QFile::copy(simulatorFixture, destination)) {
        error << "fixture Verilator could not create simulator executable\n";
        error.flush();
        return 25;
    }
    QFile destinationFile(destination);
    destinationFile.setPermissions(
        QFileInfo(simulatorFixture).permissions()
        | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
    output << "fixture Verilator built " << destination << '\n';
    output.flush();
    return 0;
#elif WAVE_TOOLCHAIN_FIXTURE_MODE == 6
    QString vcdPath;
    QString stimulusPath;
    for (const auto& argument : application.arguments()) {
        if (argument.startsWith(QStringLiteral("--vcd="))) {
            vcdPath = argument.mid(QStringLiteral("--vcd=").size());
        } else if (argument.startsWith(QStringLiteral("--stimulus="))) {
            stimulusPath = argument.mid(QStringLiteral("--stimulus=").size());
        }
    }
    bool delayOk = false;
    const auto delayMs = qEnvironmentVariableIntValue(
        "WAVE_SIMULATOR_FIXTURE_DELAY_MS", &delayOk);
    if (delayOk && delayMs > 0) {
        QThread::msleep(static_cast<unsigned long>(delayMs));
    }
    QSaveFile vcd(vcdPath);
    if (vcdPath.isEmpty() || !QFileInfo(stimulusPath).isFile()
        || !vcd.open(QIODevice::WriteOnly)) {
        error << "fixture simulator did not receive runtime stimulus and writable VCD paths\n";
        error.flush();
        return 26;
    }
    static constexpr char document[] =
        "$date fixed fixture $end\n"
        "$version Wave Workbench fixture $end\n"
        "$timescale 1ps $end\n"
        "$scope module TOP $end\n"
        "$var wire 1 ! clk_i $end\n"
        "$var wire 1 \" rst_i $end\n"
        "$var wire 4 # count_o [3:0] $end\n"
        "$var wire 1 $ pulse_o $end\n"
        "$upscope $end\n"
        "$enddefinitions $end\n"
        "#0\n1!\n0\"\nb0001 #\n1$\n"
        "#5000\n0!\n"
        "#10000\n1!\nb0010 #\n0$\n"
        "#15000\n0!\n"
        "#20000\n1!\nb0011 #\n1$\n"
        "#25000\n0!\n"
        "#30000\n1!\nb0100 #\n0$\n";
    if (vcd.write(document) != static_cast<qint64>(sizeof(document) - 1)
        || !vcd.commit()) {
        error << "fixture simulator could not commit VCD\n";
        error.flush();
        return 27;
    }
    output << "fixture simulator wrote " << vcdPath << '\n';
    output.flush();
    return 0;
#else
#error Unsupported WAVE_TOOLCHAIN_FIXTURE_MODE
#endif
}
