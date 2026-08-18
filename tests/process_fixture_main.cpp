#include <QCoreApplication>
#include <QTextStream>
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
#else
#error Unsupported WAVE_TOOLCHAIN_FIXTURE_MODE
#endif
}
