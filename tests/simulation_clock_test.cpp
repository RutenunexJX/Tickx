#include "wave/model.h"
#include "wave/simulation_pipeline.h"
#include "wave/stimulus_scenario.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <limits>

namespace {
constexpr wave::Tick maximum = std::numeric_limits<wave::Tick>::max();
constexpr wave::Tick minimum = std::numeric_limits<wave::Tick>::min();
struct ClockCase {
    const char* name;
    wave::Tick period;
    wave::Tick phase;
    wave::Rational duty;
    const char* firstTen;
};
const ClockCase cases[] = {
    {"negative", 10, -3, {1, 2}, "1100000111"},
    {"zero", 10, 0, {1, 2}, "1111100000"},
    {"positive", 10, 3, {1, 2}, "0001111100"},
    {"third-duty", 10, -3, {1, 3}, "0000000111"},
    {"min-phase", 10, minimum, {1, 2}, "0011111000"},
    {"future-phase", 10, maximum, {1, 2}, "0000000000"},
    {"rounded-zero-high", 1, -1, {1, 2}, "0000000000"},
    {"wide-duty-product", maximum, 0, {maximum - 1, maximum}, "1111111111"},
};
wave::ClockDomain clockFor(const ClockCase& item)
{
    wave::ClockDomain clock;
    clock.id = "test-clock"; clock.period = item.period; clock.phase = item.phase; clock.dutyCycle = item.duty;
    return clock;
}
QByteArray read(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
bool write(const QString& path, const QByteArray& data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
QString tool(const char* name)
{
    auto executable = QString::fromLatin1(name);
#ifdef Q_OS_WIN
    executable += ".exe";
#endif
    return QDir(QCoreApplication::applicationDirPath()).filePath(executable);
}
}

class SimulationClockTest final : public QObject {
    Q_OBJECT
    QTemporaryDir files_;
    QString driver_;
    QByteArray harnessHash_;
    wave::SimulationRunReport generate(const ClockCase& item, bool gated, bool disabled = false)
    {
        const QDir fixture(QStringLiteral(WAVE_SOURCE_DIR "/tests/fixtures/simulation/fixed-counter"));
        const auto parsed = wave::parseZeroSlackStimulusScenario(read(fixture.filePath("stimulus.json")));
        if (!parsed.ok()) return {};
        auto scenario = *parsed.scenario;
        for (auto& port : scenario.ports) {
            if (port.binding.name == "clk_i") {
                port.clock = wave::StimulusClockConfiguration{
                    item.period, item.phase, item.duty, wave::ClockEdge::Rising, item.firstTen[0]};
                port.segments = gated ? std::vector<wave::StimulusRange>{
                    {8, 12, disabled ? "disabled" : "gated"}} : std::vector<wave::StimulusRange>{};
            } else if (port.binding.name == "rst_i") {
                port.segments = {{0, 4, "1"}, {4, scenario.duration, "0"}};
            }
        }
        const auto stimulus = files_.filePath(QString::fromLatin1(item.name) + (disabled ? "-disabled.json" : ".json"));
        if (!write(stimulus, wave::serializeZeroSlackStimulusScenario(scenario))) return {};
        wave::SimulationRunRequest request;
        request.manifestPath = fixture.filePath("manifest.json");
        request.stimulusPath = stimulus; request.workspaceRoot = fixture.absolutePath();
        request.artifactDirectory = files_.filePath("runs");
        request.buildCacheDirectory = files_.filePath("cache");
        request.toolchain.verilatorProgram = tool("wave-verilator-fixture");
        request.toolchain.cxxProgram = tool("wave-toolchain-fixture-ready");
        request.toolchain.environment.insert("WAVE_SIMULATOR_FIXTURE", tool("wave-simulator-fixture"));
        request.toolchain.timeoutMs = 2000; request.buildTimeoutMs = 2000; request.runTimeoutMs = 2000;
        QEventLoop loop; QTimer timeout; timeout.setSingleShot(true);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        wave::SimulationRunReport result;
        wave::VerilatorSimulationRunner runner;
        if (!runner.start(request, [&](auto report) { result = std::move(report); loop.quit(); })) return result;
        timeout.start(8000); loop.exec();
        return result;
    }
private slots:
    void dutyArithmetic()
    {
        struct Expected { wave::Tick period, numerator, denominator, high; };
        const Expected values[] = {
            {9223372036854775807LL, 4611686018427387903LL, 9223372036854775807LL, 4611686018427387903LL},
            {9223372036854775806LL, 9223372036854775805LL, 9223372036854775807LL, 9223372036854775804LL},
            {5678912345678912345LL, 3478289472897423123LL, 7591427938912979871LL, 2602000728773380139LL},
            {7000000000000000003LL, 4000000000000000007LL, 8000000000000000009LL, 3500000000000000003LL},
            {2147483647LL, 9223372036854775806LL, 9223372036854775807LL, 2147483646LL},
            {10LL, 1LL, 3LL, 3LL},
        };
        for (const auto& item : values)
            QCOMPARE(wave::clockHighTicks(item.period, {item.numerator, item.denominator}),
                     std::optional<wave::Tick>(item.high));
        QVERIFY(!wave::clockHighTicks(0, {1, 2}));
        QVERIFY(!wave::clockHighTicks(10, {0, 2}));
        QVERIFY(!wave::clockHighTicks(10, {2, 2}));
    }
    void clockLevels_data()
    {
        QTest::addColumn<int>("index");
        for (int i = 0; i < int(std::size(cases)); ++i) QTest::newRow(cases[i].name) << i;
    }
    void clockLevels()
    {
        QFETCH(int, index);
        const auto& item = cases[index]; const auto clock = clockFor(item);
        wave::Lane lane; lane.kind = wave::LaneKind::Clock;
        for (wave::Tick tick = 0; tick < 10; ++tick)
            QCOMPARE(wave::clockValueAt(clock, lane, tick), item.firstTen[tick]);
        if (item.phase == minimum) {
            QCOMPARE(wave::clockValueAt(clock, lane, maximum - 1), '1');
            QCOMPARE(wave::clockValueAt(clock, lane, maximum), '0');
            QCOMPARE(wave::clockValueAt(clock, lane, minimum), '1');
        }
        if (item.phase == maximum) {
            QCOMPARE(wave::clockValueAt(clock, lane, maximum - 1), '0');
            QCOMPARE(wave::clockValueAt(clock, lane, maximum), '1');
        }
        if (item.period == maximum) {
            QCOMPARE(wave::tickAtCycle(clock, 0, wave::ClockEdge::Falling), std::optional<wave::Tick>(maximum - 1));
            QCOMPARE(wave::clockValueAt(clock, lane, maximum - 2), '1');
            QCOMPARE(wave::clockValueAt(clock, lane, maximum - 1), '0');
        }
        lane.segments = {{"gated", 8, 12, "gated", {}}, {"disabled", 12, 14, "disabled", {}}};
        QCOMPARE(wave::clockValueAt(clock, lane, 9), '0');
        QCOMPARE(wave::clockValueAt(clock, lane, 13), 'X');
    }

    void generatedDriver_data() { clockLevels_data(); }
    void generatedDriver()
    {
        QFETCH(int, index);
        if (QStringLiteral(WAVE_CXX_ID) != "GNU" && QStringLiteral(WAVE_CXX_ID) != "Clang")
            QSKIP("Generated-driver compilation fixture currently uses GCC/Clang flags");
        const auto& item = cases[index];
        const auto result = generate(item, true);
        QVERIFY2(result.ok(), qPrintable(result.diagnostic));
        const auto harness = read(result.artifacts.harnessPath);
        const auto hash = QCryptographicHash::hash(harness, QCryptographicHash::Sha256);
        if (driver_.isEmpty()) {
            harnessHash_ = hash;
            QVERIFY(write(files_.filePath("harness.cpp"), harness));
            QVERIFY(write(files_.filePath("verilated.h"), R"cpp(#pragma once
#include <cstdint>
class VerilatedContext { public: void commandArgs(int,char**) {} void traceEverOn(bool) {}
void time(std::uint64_t n) { now=n; } std::uint64_t time() { return now; } std::uint64_t now{}; };
)cpp"));
            QVERIFY(write(files_.filePath("verilated_vcd_c.h"), R"cpp(#pragma once
#include <cstdint>
class VerilatedVcdC { public: void open(const char*) {} void dump(std::uint64_t) {} void close() {} };
)cpp"));
            QVERIFY(write(files_.filePath("Vwave_fixture.h"), R"cpp(#pragma once
#include "verilated.h"
class VerilatedVcdC;
class Vwave_fixture { public:
explicit Vwave_fixture(VerilatedContext*) {} std::uint64_t clk_i{},rst_i{};
void trace(VerilatedVcdC*,int) {} void eval() {} void final() {} };
)cpp"));
            QVERIFY(write(files_.filePath("probe.cpp"), R"cpp(#define main generated_driver_main
#include "harness.cpp"
#undef main
#include <iostream>
#include <limits>
int main(int argc,char** argv) {
 RuntimePlan p; if(argc!=2 || !loadPlan(argv[1],p)) return 2;
 for(std::uint64_t t=0;t<=40;++t) std::cout<<t<<" "<<valueAt(p.inputs[0],t)<<" "<<valueAt(p.inputs[1],t)<<"\n";
 for(auto t: {std::uint64_t(std::numeric_limits<std::int64_t>::max()-2),
               std::uint64_t(std::numeric_limits<std::int64_t>::max()-1),
               std::uint64_t(std::numeric_limits<std::int64_t>::max())})
   std::cout<<t<<" "<<valueAt(p.inputs[0],t)<<" "<<valueAt(p.inputs[1],t)<<"\n";
}
)cpp"));
            const auto output = files_.filePath("driver-probe.exe");
            QProcess compiler;
            compiler.start(QStringLiteral(WAVE_TEST_CXX), {"-std=c++17", "-O2", files_.filePath("probe.cpp"), "-o", output});
            QVERIFY(compiler.waitForFinished(15000));
            QVERIFY2(compiler.exitCode() == 0, compiler.readAllStandardError().constData());
            driver_ = output;
        }
        QCOMPARE(hash, harnessHash_);
        QProcess driver;
        driver.start(driver_, {result.artifacts.runtimeStimulusPath});
        QVERIFY(driver.waitForFinished(3000));
        QCOMPARE(driver.exitCode(), 0);
        const auto output = driver.readAllStandardOutput();
        const auto lines = output.trimmed().split('\n');
        QCOMPARE(lines.size(), 44);
        const auto clock = clockFor(item);
        wave::Lane lane; lane.kind = wave::LaneKind::Clock;
        lane.segments = {{"gated", 8, 12, "gated", {}}};
        for (const auto& line : lines) {
            const auto fields = line.split(' ');
            QCOMPARE(fields.size(), 3);
            const auto tick = fields[0].toLongLong();
            QCOMPARE(fields[1].toInt(), wave::clockValueAt(clock, lane, tick) - '0');
            QCOMPARE(fields[2].toInt(), tick < 4 ? 1 : 0);
        }
        const auto evidence = qEnvironmentVariable("WAVE_ISSUE61_EVIDENCE_DIR");
        if (!evidence.isEmpty()) {
            QDir().mkpath(evidence);
            QVERIFY(write(evidence + "/generated-harness.cpp", harness));
            QVERIFY(write(evidence + "/" + item.name + ".plan", read(result.artifacts.runtimeStimulusPath)));
            QVERIFY(write(evidence + "/" + item.name + ".inputs.txt", output));
        }
    }
    void disabledOverrideStillRejected()
    {
        const auto report = generate(cases[1], true, true);
        QVERIFY(!report.ok());
        QVERIFY(report.diagnostic.contains("unknown", Qt::CaseInsensitive)
                || report.diagnostic.contains("unsupported", Qt::CaseInsensitive));
    }
};
QTEST_GUILESS_MAIN(SimulationClockTest)
#include "simulation_clock_test.moc"
