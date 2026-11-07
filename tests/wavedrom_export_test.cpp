#include "wave/export.h"

#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <algorithm>
#include <limits>

namespace {
wave::Project clockProject(
    wave::Tick period = 10, wave::Tick phase = 0,
    wave::Rational duty = {1, 2}, wave::Tick duration = 40)
{
    wave::Project project;
    project.id = "wavedrom-test";
    project.name = "Clock export";
    wave::ClockDomain clock;
    clock.id = "clock-main";
    clock.name = "clk";
    clock.period = period;
    clock.phase = phase;
    clock.dutyCycle = duty;
    project.clockDomains.push_back(clock);
    wave::Lane lane;
    lane.id = "lane-clock";
    lane.name = "clk";
    lane.kind = wave::LaneKind::Clock;
    lane.clockDomainId = clock.id;
    wave::Scenario scenario;
    scenario.id = "scenario-clock";
    scenario.name = "Clock export";
    scenario.duration = duration;
    scenario.lanes.push_back(lane);
    project.scenarios.push_back(scenario);
    return project;
}

QString sampledLevels(const QJsonObject& document, int signal = 0)
{
    const auto metadata = document.value("waveWorkbench").toObject();
    const auto duration = metadata.value("endTick").toString().toLongLong()
        - metadata.value("startTick").toString().toLongLong();
    const auto grid = metadata.value("gridTick").toString().toLongLong();
    const auto signalValues = document.value("signal").toArray();
    if (grid <= 0 || duration <= 0 || signal >= signalValues.size()) return {};
    const auto wave = signalValues[signal].toObject().value("wave").toString();
    QString result;
    QChar level;
    for (qsizetype column = 0; column < wave.size(); ++column) {
        if (wave[column] != QLatin1Char('.')) level = wave[column];
        const auto remaining = duration - column * grid;
        if (level.isNull() || remaining <= 0) return {};
        result += QString(static_cast<qsizetype>(std::min(grid, remaining)), level);
    }
    return result;
}
} // namespace

class WaveDromExportTest final : public QObject {
    Q_OBJECT
private slots:
    void clockTiming_data()
    {
        QTest::addColumn<qint64>("period");
        QTest::addColumn<qint64>("phase");
        QTest::addColumn<qint64>("numerator");
        QTest::addColumn<qint64>("denominator");
        QTest::addColumn<qint64>("start");
        QTest::addColumn<qint64>("end");
        QTest::addColumn<qint64>("grid");
        QTest::addColumn<QString>("expected");
        const auto cycle = QStringLiteral("1111100000");
        QTest::newRow("pure-clock") << qint64{10} << qint64{0} << qint64{1} << qint64{2}
            << qint64{0} << qint64{40} << qint64{5} << cycle.repeated(4);
        QTest::newRow("nonzero-phase") << qint64{10} << qint64{3} << qint64{1} << qint64{2}
            << qint64{0} << qint64{40} << qint64{1}
            << (QString("000") + cycle.repeated(3) + "1111100");
        QTest::newRow("range-start-at-phase") << qint64{10} << qint64{3} << qint64{1} << qint64{2}
            << qint64{3} << qint64{43} << qint64{5} << cycle.repeated(4);
        QTest::newRow("range-start-between-edges") << qint64{10} << qint64{0} << qint64{1} << qint64{2}
            << qint64{7} << qint64{37} << qint64{1}
            << (QString("000") + cycle.repeated(2) + "1111100");
        QTest::newRow("thirty-percent-duty") << qint64{10} << qint64{0} << qint64{3} << qint64{10}
            << qint64{0} << qint64{40} << qint64{1} << QString("1110000000").repeated(4);
        QTest::newRow("odd-period-integer-duty") << qint64{7} << qint64{0} << qint64{1} << qint64{2}
            << qint64{0} << qint64{28} << qint64{1} << QString("1110000").repeated(4);
        QTest::newRow("negative-phase") << qint64{10} << qint64{-2} << qint64{1} << qint64{2}
            << qint64{0} << qint64{30} << qint64{1} << QString("1110000011").repeated(3);
        QTest::newRow("clock-not-started") << qint64{10} << qint64{101} << qint64{1} << qint64{2}
            << qint64{0} << qint64{40} << qint64{40} << QString(40, '0');
        QTest::newRow("range-without-edges") << qint64{100} << qint64{0} << qint64{1} << qint64{2}
            << qint64{10} << qint64{40} << qint64{30} << QString(30, '1');
        QTest::newRow("subtick-high-time") << qint64{1} << qint64{0} << qint64{1} << qint64{2}
            << qint64{0} << qint64{40} << qint64{40} << QString(40, '0');
        const auto last = std::numeric_limits<qint64>::max();
        QTest::newRow("near-tick-limit") << qint64{10} << (last - 35) << qint64{1} << qint64{2}
            << (last - 40) << last << qint64{5}
            << (QString("00000") + cycle.repeated(3) + "11111");
        QTest::newRow("falling-edge-outside-tick-range") << qint64{10} << (last - 3) << qint64{1} << qint64{2}
            << (last - 10) << last << qint64{1} << QString("0000000111");
    }

    void clockTiming()
    {
        QFETCH(qint64, period);
        QFETCH(qint64, phase);
        QFETCH(qint64, numerator);
        QFETCH(qint64, denominator);
        QFETCH(qint64, start);
        QFETCH(qint64, end);
        QFETCH(qint64, grid);
        QFETCH(QString, expected);
        const auto project = clockProject(period, phase, {numerator, denominator}, end);
        wave::ExportOptions options;
        options.start = start;
        options.end = end;
        QString error;
        const auto bytes = wave::generateWaveDromJson(project, project.scenarios.front(), options, &error);
        QVERIFY2(!bytes.isEmpty(), qPrintable(error));
        QVERIFY(error.isEmpty());
        const auto document = QJsonDocument::fromJson(bytes).object();
        QCOMPARE(sampledLevels(document), expected);
        QCOMPARE(document.value("waveWorkbench").toObject().value("gridTick").toString().toLongLong(), grid);
        if (QString::fromLatin1(QTest::currentDataTag()) == "pure-clock") {
            QCOMPARE(document.value("signal").toArray()[0].toObject().value("wave").toString(), QString("10101010"));
        }
    }

    void overridesAndMixedSignals()
    {
        auto project = clockProject();
        auto& scenario = project.scenarios.front();
        scenario.lanes.front().segments = {
            {"gate", 12, 18, "gated", {}},
            {"disable", 23, 28, "disabled", {}},
        };
        wave::Lane bit;
        bit.id = "data";
        bit.name = "data";
        bit.segments = {{"low", 0, 7, "0", {}}, {"high", 7, 40, "1", {}}};
        scenario.lanes.push_back(bit);
        QString error;
        const auto bytes = wave::generateWaveDromJson(project, scenario, {}, &error);
        QVERIFY2(!bytes.isEmpty(), qPrintable(error));
        const auto document = QJsonDocument::fromJson(bytes).object();
        auto expected = QString("1111100000").repeated(4);
        expected.replace(12, 6, QString(6, '0'));
        expected.replace(23, 5, QString(5, 'x'));
        QCOMPARE(sampledLevels(document), expected);
        QCOMPARE(sampledLevels(document, 1), QString(7, '0') + QString(33, '1'));
    }

    void irrelevantClocksDoNotRefineGrid()
    {
        auto project = clockProject(10, 0, {1, 2}, 20'000);
        auto unused = project.clockDomains.front();
        unused.id = "unused";
        unused.period = 3;
        project.clockDomains.push_back(unused);
        auto hidden = project.scenarios.front().lanes.front();
        hidden.id = "hidden";
        hidden.clockDomainId = unused.id;
        hidden.visible = false;
        hidden.segments = {{"hidden-gate", 1, 2, "gated", {}}};
        project.scenarios.front().lanes.push_back(hidden);
        auto group = hidden;
        group.id = "group";
        group.visible = true;
        group.kind = wave::LaneKind::Group;
        project.scenarios.front().lanes.push_back(group);
        QString error;
        const auto bytes = wave::generateWaveDromJson(project, project.scenarios.front(), {}, &error);
        QVERIFY2(!bytes.isEmpty(), qPrintable(error));
        const auto document = QJsonDocument::fromJson(bytes).object();
        QCOMPARE(document.value("signal").toArray().size(), 1);
        QCOMPARE(document.value("waveWorkbench").toObject().value("gridTick").toString(), QString("5"));
        QCOMPARE(document.value("signal").toArray()[0].toObject().value("wave").toString(), QString("10").repeated(2'000));
    }

    void columnLimit_data()
    {
        QTest::addColumn<qint64>("duration");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("exact-limit") << qint64{20'480} << true;
        QTest::newRow("one-over-limit") << qint64{20'485} << false;
        QTest::newRow("old-whole-period-aliasing") << qint64{40'960} << false;
    }

    void columnLimit()
    {
        QFETCH(qint64, duration);
        QFETCH(bool, accepted);
        const auto project = clockProject(10, 0, {1, 2}, duration);
        QString error;
        const auto bytes = wave::generateWaveDromJson(project, project.scenarios.front(), {}, &error);
        if (accepted) {
            QVERIFY2(!bytes.isEmpty(), qPrintable(error));
            const auto document = QJsonDocument::fromJson(bytes).object();
            QCOMPARE(document.value("signal").toArray()[0].toObject().value("wave").toString(), QString("10").repeated(2'048));
        } else {
            QVERIFY(bytes.isEmpty());
            QVERIFY(error.contains("4096"));
            QVERIFY(error.contains("range", Qt::CaseInsensitive));
            QVERIFY(wave::generateWaveDromJson(project, project.scenarios.front(), {}, nullptr).isEmpty());
        }
    }

    void bundlePropagatesColumnLimit()
    {
        const auto project = clockProject(10, 0, {1, 2}, 20'485);
        wave::ExportOptions options;
        options.width = 480;
        options.pngDpi = 72;
        const auto result = wave::generateArtifactBundle(project, project.scenarios.front(), options);
        QVERIFY(!result.ok());
        QVERIFY(!result.artifacts.has_value());
        QVERIFY2(result.error.contains("4096"), qPrintable(result.error));
    }
};

int main(int argc, char** argv)
{
    QGuiApplication application(argc, argv);
    WaveDromExportTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "wavedrom_export_test.moc"
