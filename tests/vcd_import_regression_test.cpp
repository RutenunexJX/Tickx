#include "trace_canvas.h"
#include "wave/trace.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <sstream>

namespace {
const std::string header =
    "$timescale 1 ns $end\n"
    "$scope module top $end\n"
    "$var wire 1 ! a $end\n"
    "$var wire 1 \" b $end\n"
    "$var wire 4 # data $end\n"
    "$var wire 4 % mask $end\n"
    "$var wire 1 ! alias $end\n"
    "$upscope $end\n"
    "$enddefinitions $end\n";

const std::string scalarBody = "#0\n0!\n1\"\n#10\n1!\n0\"\n#20\n0!\n1\"\n";
const std::string vectorBody = "#0\nb0000 #\nb1111 %\n#10\nb0011 #\nbxxzz %\n#20\nb1100 #\nbzzxx %\n";
const std::string mixedBody =
    "#0\n0!\n1\"\nb0000 #\nb1111 %\n"
    "#10\n1!\n0\"\nb0011 #\nbxxzz %\n"
    "#20\n0!\n1\"\nb1100 #\nbzzxx %\n";

wave::TraceParseOptions options()
{
    wave::TraceParseOptions result;
    result.projectTimeBase = {1'000};
    result.identity = {"regression-project", "regression-trace", 7};
    return result;
}

wave::TraceParseResult parse(const std::string& text, const wave::TraceParseOptions& settings = options())
{
    std::istringstream input(text);
    return wave::parseVcd(input, settings);
}
}

class VcdImportRegressionTest final : public QObject {
    Q_OBJECT
private slots:
    void packedChanges_data()
    {
        QTest::addColumn<QString>("canonical");
        QTest::addColumn<QString>("packed");
        QTest::newRow("scalars") << QString::fromStdString(scalarBody)
            << "#0\n0! 1\"\n#10\n1! 0\"\n#20\n0! 1\"\n";
        QTest::newRow("vectors") << QString::fromStdString(vectorBody)
            << "#0\nb0000 # b1111 %\n#10\nb0011 # bxxzz %\n#20\nb1100 # bzzxx %\n";
        QTest::newRow("mixed") << QString::fromStdString(mixedBody)
            << "#0\n0! 1\" b0000 # b1111 %\n#10\n1! 0\" b0011 # bxxzz %\n#20\n0! 1\" b1100 # bzzxx %\n";
        QTest::newRow("timestamps") << QString::fromStdString(mixedBody)
            << "#0 0! 1\" b0000 # b1111 % #10 1! 0\" b0011 # bxxzz % #20 0! 1\" b1100 # bzzxx %";
        QTest::newRow("split-vector-identifiers") << QString::fromStdString(vectorBody)
            << "#0\nb0000\n#\nb1111\n%\n#10\nb0011\n#\nbxxzz\n%\n#20\nb1100\n#\nbzzxx\n%";
    }

    void packedChanges()
    {
        QFETCH(QString, canonical);
        QFETCH(QString, packed);
        auto settings = options();
        settings.offset = 17;
        const auto expected = parse(header + canonical.toStdString(), settings);
        const auto actual = parse(header + packed.toStdString(), settings);
        QVERIFY2(expected.ok(), expected.errorSummary().c_str());
        QVERIFY2(actual.ok(), actual.errorSummary().c_str());
        QCOMPARE(actual.diagnostics.size(), expected.diagnostics.size());
        QVERIFY(actual.index->traceSignals == expected.index->traceSignals);
        QCOMPARE(actual.index->transitionCount, expected.index->transitionCount);
        QCOMPARE(actual.index->startTick, expected.index->startTick);
        QCOMPARE(actual.index->endTick, expected.index->endTick);
        QVERIFY(actual.index->identity == settings.identity);
    }

    void directivesAndWhitespace()
    {
        auto compactHeader = header;
        for (auto& ch : compactHeader) if (ch == '\n') ch = ' ';
        const auto actual = parse(
            "$comment text$end #999 b1111 # $end\t" + compactHeader +
            "$dumpvars 0! 1\" b0000\n# b1111 % $end "
            "$comment #999 1! b1111 # $end "
            "#10 $dumpall 1! 0\" b0011 # bxxzz % $end "
            "#20 $dumpoff 0! 1\" b1100 # bzzxx % $end "
            "$dumpon 0! 1\" b1100 # bzzxx % $end");
        const auto expected = parse(header + mixedBody);
        QVERIFY2(actual.ok(), actual.errorSummary().c_str());
        QVERIFY(actual.diagnostics.empty());
        QVERIFY(actual.index->traceSignals == expected.index->traceSignals);
        QCOMPARE(actual.index->transitionCount, expected.index->transitionCount);
        QCOMPARE(actual.index->endTick, wave::Tick{20});
    }

    void stableTail_data()
    {
        QTest::addColumn<QString>("body");
        QTest::addColumn<qint64>("offset");
        QTest::addColumn<qint64>("picoseconds");
        QTest::addColumn<qint64>("end");
        QTest::addColumn<qulonglong>("count");
        QTest::newRow("last-change-20-last-time-100") << "#0\n0!\n#20\n1!\n#100\n"
            << qint64{0} << qint64{1'000} << qint64{100} << qulonglong{4};
        QTest::newRow("initial-only") << "#0\n0!\n#100\n"
            << qint64{0} << qint64{1'000} << qint64{100} << qulonglong{2};
        QTest::newRow("deduplicated-final-value") << "#0\n0!\n#20\n0!\n#100\n0!\n"
            << qint64{0} << qint64{1'000} << qint64{100} << qulonglong{2};
        QTest::newRow("converted-offset") << "#0\n0!\n#20\n1!\n#100\n"
            << qint64{-25} << qint64{10} << qint64{9'975} << qulonglong{4};
        QTest::newRow("negative-offset-tail") << "#0\n0!\n#20\n1!\n#100\n"
            << qint64{-200} << qint64{1'000} << qint64{-100} << qulonglong{4};
        QTest::newRow("timestamps-without-values") << "#0\n#100\n"
            << qint64{-200} << qint64{1'000} << qint64{-100} << qulonglong{0};
    }

    void stableTail()
    {
        QFETCH(QString, body);
        QFETCH(qint64, offset);
        QFETCH(qint64, picoseconds);
        QFETCH(qint64, end);
        QFETCH(qulonglong, count);
        auto settings = options();
        settings.offset = offset;
        settings.projectTimeBase = {picoseconds};
        const auto result = parse(header + body.toStdString(), settings);
        QVERIFY2(result.ok(), result.errorSummary().c_str());
        QCOMPARE(result.index->endTick, end);
        QCOMPARE(result.index->startTick, offset);
        QCOMPARE(result.index->transitionCount, count);
        for (const auto& signal : result.index->traceSignals) {
            for (const auto& transition : signal.transitions)
                QVERIFY(transition.tick < result.index->endTick);
        }
    }

    void ignoredTypesAndDirectiveBoundary()
    {
        const auto result = parse(header +
            "#0 r3.14 ! stext \" 0! b0000 # "
            "#10 r2.0\n! sother\n\" 1! b1010 # "
            "#20 b1111 $end 0! #100");
        QVERIFY2(result.ok(), result.errorSummary().c_str());
        const auto* a = result.index->findSignal("top.a");
        const auto* data = result.index->findSignal("top.data");
        QVERIFY(a && data);
        QCOMPARE(a->transitions.size(), std::size_t{3});
        QCOMPARE(data->transitions.size(), std::size_t{2});
        QCOMPARE(QString::fromStdString(a->valueAt(99)->value), QString("0"));
        QCOMPARE(QString::fromStdString(data->valueAt(99)->value), QString("1010"));
        QCOMPARE(result.index->endTick, wave::Tick{100});
        QVERIFY(!result.diagnostics.empty());
    }

    void timestampLikeIdentifier()
    {
        const auto result = parse(
            "$timescale 1 ns $end\n"
            "$var wire 4 #100 data $end\n"
            "$var wire 4 $end other $end\n"
            "$enddefinitions $end\n"
            "#0 b0000 #100 b0011 $end #20 b1111 #100 b1100 $end #100");
        QVERIFY2(result.ok(), result.errorSummary().c_str());
        QVERIFY(result.diagnostics.empty());
        QCOMPARE(result.index->transitionCount, std::uint64_t{4});
        QCOMPARE(result.index->endTick, wave::Tick{100});
    }

    void cancelledTokenStreams_data()
    {
        QTest::addColumn<bool>("comment");
        QTest::newRow("one-line-changes") << false;
        QTest::newRow("one-line-comment") << true;
    }

    void cancelledTokenStreams()
    {
        QFETCH(bool, comment);
        std::string body = comment ? "$comment " : "#0 ";
        for (int i = 0; i < 20'000; ++i) body += comment ? "word " : "0! 1! ";
        if (comment) body += "$end";
        bool stop = false;
        int progressCalls = 0;
        std::uint64_t completedBefore = 0;
        auto settings = options();
        settings.isCancelled = [&] { return stop; };
        settings.progress = [&](const std::uint64_t completed, const std::uint64_t total) {
            QVERIFY(completed >= completedBefore);
            QCOMPARE(total, std::uint64_t{0});
            completedBefore = completed;
            ++progressCalls;
            stop = true;
        };
        const auto result = parse(header + body, settings);
        QVERIFY(progressCalls > 0);
        QVERIFY(result.cancelled);
        QVERIFY(!result.index.has_value());
    }

    void invalidTimestampAndCsvRange()
    {
        const auto invalid = parse(header + "#0 0!\n#invalid 1!\n");
        QVERIFY(!invalid.ok());
        QVERIFY(!invalid.diagnostics.empty());
        QCOMPARE(invalid.diagnostics.back().line, std::size_t{11});
        auto settings = options();
        settings.projectTimeBase = {1'001};
        const auto inexact = parse(header + "#0 0! #1", settings);
        QVERIFY(!inexact.ok());
        QVERIFY(inexact.errorSummary().find("exactly") != std::string::npos);
        std::istringstream csv("time,a\n0,0\n20,1\n100,1\n");
        const auto result = wave::parseCsv(csv, options());
        QVERIFY2(result.ok(), result.errorSummary().c_str());
        QCOMPARE(result.index->endTick, wave::Tick{20});
        QCOMPARE(result.index->transitionCount, std::uint64_t{2});
    }

    void stableTailIsVisible()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto fileName = directory.filePath("stable-tail.vcd");
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const auto content = QByteArray::fromStdString(header + "#0\n0!\n#20\n1!\n#100\n");
        QCOMPARE(file.write(content), content.size());
        file.close();
        const auto parsed = wave::parseVcdFile(std::filesystem::path(fileName.toStdWString()), options());
        QVERIFY2(parsed.ok(), parsed.errorSummary().c_str());
        wave::Project project;
        project.timeBase = options().projectTimeBase;
        wave::Scenario scenario;
        wave::ImportedTrace reference;
        wave::TraceCanvas canvas;
        canvas.resize(1'000, 300);
        canvas.setTrace(&project, &scenario, &*parsed.index, &reference);
        canvas.setVisibleSignalIds({"top.a"});
        canvas.show();
        canvas.fitTrace();
        QApplication::processEvents();
        QSignalSpy cursor(&canvas, &wave::TraceCanvas::cursorChanged);
        const auto probe = [&] {
            const QPointF point(canvas.viewport()->width() - 30, 56);
            QMouseEvent event(QEvent::MouseMove, point,
                canvas.viewport()->mapToGlobal(point.toPoint()),
                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(canvas.viewport(), &event);
        };
        probe();
        QVERIFY(!cursor.empty());
        QVERIFY2(cursor.back()[0].toLongLong() > 80, "fitTrace cannot reach the stable tail");
        QCOMPARE(cursor.back()[2].toString(), QString("1"));
        const auto evidence = qEnvironmentVariable("WAVE_VCD_EVIDENCE_DIR");
        if (!evidence.isEmpty()) {
            QDir().mkpath(evidence);
            QVERIFY(canvas.viewport()->grab().save(evidence + "/stable-tail-fit.png"));
        }
        canvas.zoomIn();
        canvas.horizontalScrollBar()->setValue(canvas.horizontalScrollBar()->maximum());
        QApplication::processEvents();
        probe();
        QVERIFY(cursor.back()[0].toLongLong() > 80);
        QVERIFY(cursor.back()[0].toLongLong() <= 100);
        QCOMPARE(cursor.back()[2].toString(), QString("1"));
        if (!evidence.isEmpty())
            QVERIFY(canvas.viewport()->grab().save(evidence + "/stable-tail-zoom.png"));
        QCOMPARE(parsed.index->findSignal("top.a")->transitions.size(), std::size_t{2});
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir profile;
    if (!profile.isValid()) return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, profile.path());
    QApplication application(argc, argv);
    VcdImportRegressionTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "vcd_import_regression_test.moc"
