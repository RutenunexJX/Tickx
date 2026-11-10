#include "main_window.h"
#include "wave_canvas.h"
#include "wave/export.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
bool write(const QString& path, const QByteArray& data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
wave::Project fixture(int count)
{
    wave::Project p;
    p.id = "pdf-regression"; p.name = "Pagination"; p.timeBase = {1000};
    wave::Scenario s; s.id = "scenario"; s.name = "Signals"; s.duration = 100;
    for (int i = 0; i < count; ++i) {
        if (i % 20 == 0) {
            wave::Lane group; group.id = "group-" + std::to_string(i / 20);
            group.name = "GROUP_" + std::to_string(i / 20); group.kind = wave::LaneKind::Group;
            s.lanes.push_back(group);
        }
        wave::Lane lane;
        lane.id = "signal-" + std::to_string(i);
        lane.name = QString("SIGNAL_%1").arg(i, 3, 10, QLatin1Char('0')).toStdString();
        lane.groupId = "group-" + std::to_string(i / 20);
        lane.color = "#207080";
        lane.segments = {{lane.id + "-0", 0, 20, "0", {}},
                         {lane.id + "-1", 20, 60, "1", {}},
                         {lane.id + "-2", 60, 100, "0", {}}};
        s.lanes.push_back(lane);
    }
    wave::Marker m; m.id = "marker"; m.name = "MARKER_ALL"; m.start = 0; m.end = 100;
    m.kind = wave::MarkerKind::Interval; s.markers.push_back(m);
    p.scenarios.push_back(s);
    return p;
}
}

class ExportNavigationTest final : public QObject {
    Q_OBJECT
private slots:
    void pdfPages_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<int>("signalsCount");
        QTest::addColumn<int>("rowHeight");
        QTest::addColumn<bool>("timePages");
        QTest::addColumn<bool>("selected");
        QTest::newRow("short") << "short" << 3 << 52 << false << false;
        QTest::newRow("many") << "many" << 40 << 52 << false << false;
        QTest::newRow("time-and-lanes") << "time-and-lanes" << 26 << 52 << true << false;
        QTest::newRow("selected-visible") << "selected-visible" << 40 << 52 << false << true;
        QTest::newRow("tall-rows") << "tall-rows" << 12 << 120 << false << false;
        QTest::newRow("empty") << "empty" << 0 << 52 << false << false;
    }
    void pdfPages()
    {
        QFETCH(QString, name); QFETCH(int, signalsCount); QFETCH(int, rowHeight);
        QFETCH(bool, timePages); QFETCH(bool, selected);
        if (QStringLiteral(WAVE_PDF_PYTHON).isEmpty()) QSKIP("PDF text validation needs Python with pdfplumber");
        auto p = fixture(signalsCount);
        auto& s = p.scenarios.front();
        if (selected) for (auto& lane : s.lanes)
            if (lane.kind != wave::LaneKind::Group)
                lane.visible = lane.id == "signal-2" || lane.id == "signal-39";
        wave::ExportOptions options;
        options.laneHeight = rowHeight;
        if (timePages) { options.start = 10; options.end = 70; options.pdfPageSpanTicks = 30; }
        QString error;
        const auto bytes = wave::renderWaveformPdf(p, s, options, &error);
        QVERIFY2(!bytes.isEmpty(), qPrintable(error));
        QTemporaryDir temporary;
        auto directory = qEnvironmentVariable("WAVE_ISSUE61_EVIDENCE_DIR");
        if (directory.isEmpty()) directory = temporary.path();
        QVERIFY(QDir().mkpath(directory));
        const auto pdf = QDir(directory).filePath(name + ".pdf");
        QVERIFY(write(pdf, bytes));
        QProcess text;
        text.start(QStringLiteral(WAVE_PDF_PYTHON), {QStringLiteral(WAVE_SOURCE_DIR "/tests/pdf_page_text.py"), pdf});
        QVERIFY(text.waitForFinished(5000));
        QVERIFY2(text.exitCode() == 0, text.readAllStandardError().constData());
        const auto extracted = text.readAllStandardOutput();
        QVERIFY(write(QDir(directory).filePath(name + ".pages.json"), extracted));
        const auto pageData = QJsonDocument::fromJson(extracted).array();
        QList<QStringList> pages;
        for (const auto& value : pageData) {
            const auto page = value.toObject();
            QStringList words;
            for (const auto& entry : page.value("words").toArray()) {
                const auto item = entry.toObject();
                const auto word = item.value("text").toString();
                if (word.startsWith("SIGNAL_") || word.startsWith("GROUP_")) {
                    QVERIFY(item.value("x0").toDouble() >= 0);
                    QVERIFY(item.value("top").toDouble() >= 0);
                    QVERIFY(item.value("x1").toDouble() <= page.value("width").toDouble());
                    QVERIFY(item.value("bottom").toDouble() <= page.value("height").toDouble());
                }
                words.append(word);
            }
            pages.append(words);
        }
        QVERIFY(!pages.isEmpty());
        QStringList expected;
        for (const auto& lane : s.lanes)
            if (lane.visible && lane.kind != wave::LaneKind::Group)
                expected.append(QString::fromStdString(lane.name));
        const auto allExpected = timePages ? expected + expected : expected;
        QStringList actual;
        for (const auto& words : pages) {
            for (const auto& word : words) if (word.startsWith("SIGNAL_")) actual.append(word);
            QVERIFY(words.contains("MARKER_ALL"));
            QVERIFY(words.contains("ns"));
        }
        QCOMPARE(actual, allExpected);
        if (!expected.isEmpty()) {
            QVERIFY(pages.front().contains("GROUP_0"));
            if (signalsCount > 20) {
                bool groupPresent = false;
                for (const auto& words : pages) groupPresent |= words.contains("GROUP_1");
                QVERIFY(groupPresent);
            }
        }
        if (signalsCount <= 3 || selected) QCOMPARE(pages.size(), 1);
        if (signalsCount >= 26 && !selected) QVERIFY(pages.size() > (timePages ? 2 : 1));
    }

    void edgeFeedback()
    {
        auto p = fixture(1);
        wave::MainWindow window(p);
        window.resize(1100, 700); window.show(); window.activateWindow();
        auto* canvas = window.findChild<wave::WaveCanvas*>();
        QVERIFY(canvas);
        canvas->revealLocation("signal-0", 100); canvas->setFocus();
        QTRY_VERIFY(canvas->hasFocus());
        const auto original = window.project();
        QTest::keyClick(canvas, Qt::Key_Right, Qt::ControlModifier);
        QCOMPARE(canvas->cursorTick(), wave::Tick(100));
        auto* notice = window.findChild<QWidget*>("WaveNotification");
        QVERIFY(notice); QVERIFY(notice->isVisible());
        QVERIFY(notice->accessibleDescription().contains("No later edge"));
        QVERIFY(notice->accessibleDescription().contains("SIGNAL_000"));
        QCOMPARE(QApplication::focusWidget(), canvas);
        auto directory = qEnvironmentVariable("WAVE_ISSUE61_EVIDENCE_DIR");
        if (!directory.isEmpty()) {
            QVERIFY(QDir().mkpath(directory));
            QVERIFY(window.grab().save(directory + "/no-later-edge.png"));
        }
        QTest::keyClick(canvas, Qt::Key_Left, Qt::ControlModifier);
        QVERIFY(canvas->cursorTick() < 100);
        QVERIFY(!notice->accessibleDescription().contains("No later edge"));
        canvas->revealLocation("signal-0", 0);
        QTest::keyClick(canvas, Qt::Key_Left, Qt::ControlModifier);
        QCOMPARE(canvas->cursorTick(), wave::Tick(0));
        QVERIFY(notice->isVisible());
        QVERIFY(notice->accessibleDescription().contains("No earlier edge"));
        if (!directory.isEmpty()) QVERIFY(window.grab().save(directory + "/no-earlier-edge.png"));
        QTest::keyClick(canvas, Qt::Key_Right, Qt::ControlModifier);
        QCOMPARE(canvas->cursorTick(), wave::Tick(20));
        QVERIFY(!notice->accessibleDescription().contains("No earlier edge"));
        QVERIFY(window.project() == original);
        QCOMPARE(QApplication::focusWidget(), canvas);
        if (!directory.isEmpty()) QVERIFY(window.grab().save(directory + "/navigation-resumed.png"));
    }
};
int main(int argc, char** argv)
{
    QTemporaryDir profile;
    qputenv("WAVEWORKBENCH_SETTINGS_DIR", profile.path().toUtf8());
    qputenv("WAVEWORKBENCH_RECOVERY_DIR", (profile.path() + "/recovery").toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, profile.path());
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    ExportNavigationTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "export_navigation_test.moc"
