#include "main_window.h"
#include "application_identity.h"
#include "wave_canvas.h"
#include "trace_canvas.h"
#include "signal_style.h"
#include "quick_waveform.h"
#include "ui_controls.h"
#include "wave/project_io.h"
#include "ElaLineEdit.h"
#include "ElaSpinBox.h"
#include "ElaDoubleSpinBox.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QFontDatabase>
#include <QFileInfo>
#include <QFileDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEnterEvent>
#include <QLabel>
#include <QHelpEvent>
#include <QLineEdit>
#include <QMenuBar>
#include <QMenu>
#include <QPointer>
#include <QRegularExpression>
#include <QMimeData>
#include <QPushButton>
#include <QRawFont>
#include <QScrollBar>
#include <QStatusBar>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleOptionComboBox>
#include <QWindow>
#include <QScopeGuard>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QtMath>
#include <QToolBar>
#include <QToolButton>

#include <limits>
#include <cmath>
#include <set>

namespace {
wave::Project project()
{
    auto p = wave::makeDemonstrationProject();
    p.scenarios.front().duration = 220'000;
    return p;
}
void screenshot(QWidget* widget,const QString& name)
{
    const auto directory = qEnvironmentVariable("WAVE_FOLLOWUP_PREVIEW_DIR");
    if (directory.isEmpty()) return;
    QVERIFY(QDir().mkpath(directory));
    QVERIFY(widget->grab().save(directory+'/'+name+".png"));
}
std::string sample(const wave::Lane& lane,wave::Tick tick)
{
    for (const auto& segment : lane.segments)
        if (segment.start <= tick && tick < segment.end) return segment.value;
    return {};
}
wave::Project styleProject()
{
    wave::Project p; p.id="style-gallery"; p.name="Signal styles"; p.timeBase={1000};
    wave::ClockDomain clock; clock.id=clock.name="clk"; clock.period=10;
    p.clockDomains.push_back(clock);
    wave::Scenario s; s.id=s.name="styles"; s.duration=200;
    wave::Lane group; group.id=group.name="Interface"; group.kind=wave::LaneKind::Group; group.height=40;
    s.lanes.push_back(group);
    for (const auto& name : {"clk","bit","data","reserved","dont-care","X","Z"}) {
        wave::Lane lane; lane.id=lane.name=name; lane.groupId=group.id;
        lane.kind=lane.name=="clk" ? wave::LaneKind::Clock : lane.name=="bit" ? wave::LaneKind::Bit : wave::LaneKind::Bus;
        lane.width=lane.kind==wave::LaneKind::Bus ? 8 : 1;
        lane.clockDomainId=lane.name=="dont-care" ? "" : "clk";
        if (lane.kind==wave::LaneKind::Bit) lane.segments={{"low",0,40,"0",{}},{"high",40,100,"1",{}},{"low2",100,200,"0",{}}};
        else if (lane.kind==wave::LaneKind::Bus) {
            wave::Segment segment{lane.id+"-value",20,100,lane.name=="Z" ? "0bZZZZZZZZ" : lane.name=="X" || lane.name=="dont-care" ? "0bXXXXXXXX" : "0x35",{}};
            if (lane.name=="reserved" || lane.name=="dont-care") segment.extensions["waveWorkbench.busPreset"]='"'+lane.name+'"';
            lane.segments.push_back(segment);
        }
        s.lanes.push_back(lane);
    }
    p.scenarios.push_back(s); return p;
}
}

class HiddenNativeWindows final : public QObject {
    bool eventFilter(QObject* object,QEvent* event) override {
        if (event->type()==QEvent::Polish) {
            if (auto* widget=qobject_cast<QWidget*>(object); widget && widget->isWindow())
                widget->setAttribute(Qt::WA_DontShowOnScreen);
        }
        return false;
    }
};

class UiFollowupTest : public QObject {
    Q_OBJECT
    QTemporaryDir settings_;
    HiddenNativeWindows hidden_;
private slots:
    void initTestCase()
    {
        if (QGuiApplication::platformName()=="windows") qApp->installEventFilter(&hidden_);
        QVERIFY(settings_.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_.path());
        qApp->setOrganizationName("WaveUiFollowupTests");
        qApp->setApplicationName("IsolatedUi");
        const auto fontPath=QDir(qEnvironmentVariable("QT_QPA_FONTDIR")).filePath("msyh.ttc");
        if (QFileInfo::exists(fontPath)) QVERIFY(QFontDatabase::addApplicationFont(fontPath)>=0);
        wave::ui::initializeApplicationTheme();
        const auto font = QRawFont::fromFont(qApp->font());
        QVERIFY(font.isValid());
        for (const auto character : QStringLiteral("Wave clk_2 [7:0] 10.5 ns"))
            QVERIFY2(font.supportsCharacter(character),qPrintable(font.familyName()));
        if (QFileInfo::exists(fontPath)) QVERIFY(QFontMetrics(qApp->font()).inFont(QChar(u'信')));
    }

    void brandingPreservesLegacySettings()
    {
        const auto organization=QCoreApplication::organizationName();
        const auto application=QCoreApplication::applicationName();
        const auto display=QGuiApplication::applicationDisplayName();
        const auto version=QCoreApplication::applicationVersion();
        const auto recoveryEnvironment=qgetenv("WAVEWORKBENCH_RECOVERY_DIR");
        auto restore=qScopeGuard([&] {
            QCoreApplication::setOrganizationName(organization);
            QCoreApplication::setApplicationName(application);
            QGuiApplication::setApplicationDisplayName(display);
            QCoreApplication::setApplicationVersion(version);
            if (recoveryEnvironment.isNull()) qunsetenv("WAVEWORKBENCH_RECOVERY_DIR");
            else qputenv("WAVEWORKBENCH_RECOVERY_DIR",recoveryEnvironment);
            wave::ui::initializeApplicationTheme();
        });
        QCoreApplication::setOrganizationName("WaveWorkbench");
        QCoreApplication::setApplicationName("Wave Workbench");
        QSettings legacy;
        legacy.setValue("appearance/colorScheme","dark");
        legacy.setValue("canvas/signalHeaderWidth",312);
        legacy.sync();
        QCOMPARE(legacy.status(),QSettings::NoError);
        const auto settingsFile=legacy.fileName();
        const auto dataDirectory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        qunsetenv("WAVEWORKBENCH_RECOVERY_DIR");
        const auto recoveryPath=wave::untitledRecoveryPath();

        wave::configureStandaloneIdentity("test-version");
        QCOMPARE(QGuiApplication::applicationDisplayName(),QString("Tickx"));
        QCOMPARE(QCoreApplication::applicationVersion(),QString("test-version"));
        QCOMPARE(QSettings{}.fileName(),settingsFile);
        QCOMPARE(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation),dataDirectory);
        QCOMPARE(wave::untitledRecoveryPath(),recoveryPath);
        QCOMPARE(QSettings{}.value("appearance/colorScheme").toString(),QString("dark"));
        qputenv("WAVEWORKBENCH_RECOVERY_DIR",(settings_.path()+"/branding-recovery").toUtf8());
        wave::ui::initializeApplicationTheme();
        wave::MainWindow window(project()); window.show(); QCoreApplication::processEvents();
        QVERIFY(window.windowTitle().endsWith(QStringLiteral(" — Tickx")));
        auto* canvas=window.findChild<wave::WaveCanvas*>(); QVERIFY(canvas);
        QCOMPARE(canvas->signalHeaderWidth(),312);
        QTest::mousePress(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(312,60));
        QTest::mouseMove(canvas->viewport(),QPoint(384,60));
        QTest::mouseRelease(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(384,60));
        QCOMPARE(canvas->signalHeaderWidth(),384);
        legacy.sync(); QCOMPARE(legacy.value("canvas/signalHeaderWidth").toInt(),384);
        screenshot(&window,"tickx-branding-legacy-settings");
    }

    void styleRoundTripAndInheritance()
    {
        auto p = project();
        auto& lane = *wave::findLane(p.scenarios.front(),"lane-request");
        const auto values = lane.segments;
        const auto events = p.scenarios.front().events;
        QCOMPARE(wave::signalStylePresetIds().size(),10);
        for (const auto& preset : wave::signalStylePresetIds()) {
            wave::SignalStyleSettings settings; settings.preset = preset;
            wave::storeSignalStyleSettings(p.extensions,settings);
            const auto loaded = wave::deserializeProject(wave::serializeProject(p));
            QVERIFY2(loaded.ok(),qPrintable(loaded.error));
            QVERIFY(*loaded.project == p);
            for (const auto scheme : {wave::WaveformColorScheme::Light,wave::WaveformColorScheme::Dark}) {
                const auto resolved = wave::resolvedSignalStyle(&p,lane,scheme);
                QCOMPARE(resolved.fill.alpha(),255);
                QVERIFY(resolved.stroke >= .75);
            }
        }
        wave::SignalStyleSettings override; override.preset="cute"; override.edge="square"; override.stroke=3;
        override.fill = QColor("#557799");
        wave::storeSignalStyleSettings(lane.extensions,override);
        auto resolved = wave::resolvedSignalStyle(&p,lane,wave::WaveformColorScheme::Light);
        QCOMPARE(resolved.stroke,3.0); QVERIFY(resolved.trapezoid); QCOMPARE(resolved.fill,QColor("#557799"));
        QVERIFY(lane.segments == values); QVERIFY(p.scenarios.front().events == events);
        lane.extensions["waveWorkbench.signalAppearance"] = R"({"preset":"bad","stroke":-99,"fill":"invalid"})";
        resolved = wave::resolvedSignalStyle(&p,lane,wave::WaveformColorScheme::Light);
        QVERIFY(resolved.fill.isValid()); QVERIFY(resolved.stroke > 0);
    }

    void canvasGeometryIgnoresLaneOverrides()
    {
        const QStringList slopedPresets{"cute","blueprint","scope","paper","facet"};
        for (const auto& preset : wave::signalStylePresetIds()) {
            for (const auto& edge : {QString("preset"),QString("square"),QString("trapezoid")}) {
                auto p=styleProject();
                wave::SignalStyleSettings defaults; defaults.preset=preset; defaults.edge=edge;
                defaults.corners="sharp";
                wave::storeSignalStyleSettings(p.extensions,defaults);
                const bool sloped=edge=="trapezoid" || (edge=="preset" && slopedPresets.contains(preset));
                for (auto& lane : p.scenarios.front().lanes) {
                    wave::normalizeSegments(lane,true);
                    wave::SignalStyleSettings override;
                    override.preset=sloped ? "terminal" : "facet";
                    override.edge=sloped ? "square" : "trapezoid";
                    override.corners="round"; override.fill=QColor("#557799"); override.stroke=3;
                    wave::storeSignalStyleSettings(lane.extensions,override);
                    for (const auto scheme : {wave::WaveformColorScheme::Light,wave::WaveformColorScheme::Dark}) {
                        const auto style=wave::resolvedSignalStyle(&p,lane,scheme);
                        QCOMPARE(style.trapezoid,sloped);
                        QVERIFY(!style.rounded);
                        QCOMPARE(style.bevel>0,sloped);
                        QCOMPARE(style.fill,QColor("#557799")); QCOMPARE(style.stroke,3.0);
                    }
                }
                const auto loaded=wave::deserializeProject(wave::serializeProject(p));
                QVERIFY(loaded.ok()); QVERIFY(*loaded.project==p);
            }
        }
    }

    void canvasEdgesRenderConsistently_data()
    {
        QTest::addColumn<bool>("sloped");
        QTest::newRow("square") << false;
        QTest::newRow("trapezoid") << true;
    }

    void canvasEdgesRenderConsistently()
    {
        QFETCH(bool,sloped);
        auto p=styleProject();
        p.clockDomains.front().period=120; p.clockDomains.front().phase=40;
        wave::SignalStyleSettings settings; settings.preset="engineering";
        settings.edge=sloped ? "trapezoid" : "square"; settings.corners="sharp";
        settings.palette="signal"; settings.stroke=2; settings.fill=QColor("#f4fff8");
        wave::storeSignalStyleSettings(p.extensions,settings);
        for (auto& lane : p.scenarios.front().lanes) {
            lane.color="#df007f";
            wave::SignalStyleSettings override; override.preset="cute";
            override.edge=sloped ? "square" : "trapezoid";
            wave::storeSignalStyleSettings(lane.extensions,override);
        }
        const auto original=p;
        auto& scenario=p.scenarios.front(); wave::CommandStack commands; wave::WaveCanvas canvas;
        canvas.resize(1180,650); canvas.setDocument(&p,&scenario,&commands); canvas.show();
        canvas.rangeEditPaletteWidget()->hide(); QCoreApplication::processEvents(); canvas.fitScenario();
        screenshot(&canvas,sloped ? "uniform-trapezoid" : "uniform-square");
        const auto image=canvas.viewport()->grab().toImage();
        const auto dpr=image.devicePixelRatio();
        const auto scale=double(canvas.viewport()->width()-canvas.signalHeaderWidth())/200;
        const auto edgeX=canvas.signalHeaderWidth()+qRound(40*scale);
        const auto strokeCenter=[&](int row,int dy) {
            double total=0; int count=0;
            const int y=qRound((80+row*56+dy)*dpr);
            for (int x=qFloor((edgeX-7)*dpr); x<=qCeil((edgeX+7)*dpr); ++x) {
                const auto color=image.pixelColor(x,y);
                if (color.red()>120 && color.green()<80 && color.blue()>70) { total+=x/dpr; ++count; }
            }
            return count ? total/count : std::numeric_limits<double>::quiet_NaN();
        };
        for (const auto row : {0,1}) {
            const auto upper=strokeCenter(row,18), lower=strokeCenter(row,37);
            QVERIFY(std::isfinite(upper)); QVERIFY(std::isfinite(lower));
            if (sloped) QVERIFY2(upper-lower>2,qPrintable(QString("row %1 slope %2").arg(row).arg(upper-lower)));
            else QVERIFY2(std::abs(upper-lower)<.6,qPrintable(QString("row %1 slope %2").arg(row).arg(upper-lower)));
        }
        const auto busLeft=canvas.signalHeaderWidth()+qRound(20*scale);
        const auto edge=image.pixelColor(qRound((busLeft+2)*dpr),qRound((80+2*56+14)*dpr));
        QCOMPARE(edge==settings.fill,!sloped);
        QVERIFY(p==original);
    }

    void specialWaveformsKeepTheirShapes()
    {
        auto p=styleProject();
        p.clockDomains.front().period=120; p.clockDomains.front().phase=40;
        auto& scenario=p.scenarios.front();
        scenario.lanes[1].segments={{"gate",20,60,"gated",{}},{"disabled",100,140,"disabled",{}}};
        for (const auto* value : {"X","Z"}) {
            wave::Lane lane; lane.id=lane.name=std::string("bit-")+value;
            lane.kind=wave::LaneKind::Bit; lane.groupId="Interface";
            lane.segments={{lane.id,0,200,value,{}}};
            scenario.lanes.push_back(lane);
        }
        const auto original=scenario;
        wave::SignalStyleSettings settings; settings.preset="engineering"; settings.corners="sharp";
        wave::CommandStack commands; wave::WaveCanvas canvas;
        canvas.resize(1180,800); canvas.setDocument(&p,&scenario,&commands); canvas.show();
        canvas.rangeEditPaletteWidget()->hide(); QCoreApplication::processEvents();
        std::vector<QImage> images;
        for (const auto* edge : {"square","trapezoid"}) {
            settings.edge=edge; wave::storeSignalStyleSettings(p.extensions,settings);
            canvas.refreshModel(); canvas.fitScenario(); QCoreApplication::processEvents();
            screenshot(&canvas,QString("special-waveforms-")+edge);
            images.push_back(canvas.viewport()->grab().toImage());
        }
        const auto dpr=images.front().devicePixelRatio();
        const auto scale=double(canvas.viewport()->width()-canvas.signalHeaderWidth())/200;
        const auto region=[&](double start,double end,int top,int height) {
            return QRect(qRound((canvas.signalHeaderWidth()+start*scale)*dpr),qRound(top*dpr),
                qRound((end-start)*scale*dpr),qRound(height*dpr));
        };
        for (const auto row : {3,4,5,6,7,8}) {
            const auto rect=region(1,199,80+row*56+2,52);
            QVERIFY2(images[0].copy(rect)==images[1].copy(rect),qPrintable(QString("special row %1 changed").arg(row)));
        }
        for (const auto start : {20,100}) {
            const auto rect=region(start+1,start+39,83,49);
            QVERIFY2(images[0].copy(rect)==images[1].copy(rect),"clock override changed with normal edge style");
        }
        QVERIFY(scenario==original);
    }

    void traceEdgesFollowCanvasStyle_data() { canvasEdgesRenderConsistently_data(); }

    void traceEdgesFollowCanvasStyle()
    {
        QFETCH(bool,sloped);
        auto p=styleProject();
        wave::SignalStyleSettings settings; settings.preset="engineering";
        settings.edge=sloped ? "trapezoid" : "square"; settings.corners="sharp";
        wave::storeSignalStyleSettings(p.extensions,settings);
        wave::TraceIndex trace; trace.startTick=0; trace.endTick=200;
        for (const auto width : {1,8,8,1}) {
            wave::TraceSignal signal; signal.id=signal.fullName=std::to_string(trace.traceSignals.size());
            signal.width=width;
            signal.transitions=trace.traceSignals.size()==3 ? std::vector<wave::TraceTransition>{{0,"X"},{40,"Z"}}
                : width==1 ? std::vector<wave::TraceTransition>{{0,"0"},{40,"1"},{100,"0"}}
                : trace.traceSignals.size()==1 ? std::vector<wave::TraceTransition>{{0,"00010010"},{40,"0x35"}}
                : std::vector<wave::TraceTransition>{{0,"00010010"},{40,"XXXXXXXX"},{120,"ZZZZZZZZ"}};
            trace.traceSignals.push_back(std::move(signal));
        }
        wave::TraceCanvas canvas; canvas.resize(1180,300);
        canvas.setTrace(&p,&p.scenarios.front(),&trace,nullptr); canvas.show();
        QCoreApplication::processEvents(); canvas.fitTrace();
        screenshot(&canvas,sloped ? "uniform-trace-trapezoid" : "uniform-trace-square");
        const auto image=canvas.viewport()->grab().toImage();
        const auto dpr=image.devicePixelRatio();
        const auto edgeX=230+40.0*(canvas.viewport()->width()-242)/200;
        const auto color=wave::waveformTheme(wave::waveformColorScheme(canvas.palette())).bit;
        const auto strokeCenter=[&](int y) {
            double total=0, weight=0;
            for (int x=qFloor((edgeX-7)*dpr); x<=qCeil((edgeX+7)*dpr); ++x) {
                const auto pixel=image.pixelColor(x,qRound(y*dpr));
                const auto distance=std::abs(pixel.red()-color.red())+std::abs(pixel.green()-color.green())
                    +std::abs(pixel.blue()-color.blue());
                const auto amount=std::max(0.0,1-distance/100.0);
                total+=x/dpr*amount; weight+=amount;
            }
            return weight ? total/weight : std::numeric_limits<double>::quiet_NaN();
        };
        const auto upper=strokeCenter(49), lower=strokeCenter(63);
        QVERIFY(std::isfinite(upper)); QVERIFY(std::isfinite(lower));
        if (sloped) QVERIFY(upper-lower>2);
        else QVERIFY(std::abs(upper-lower)<.6);
        const auto x=qRound((edgeX+2)*dpr);
        QCOMPARE(image.pixelColor(x,qRound((34+44+11)*dpr))
            == image.pixelColor(x,qRound((34+44+3)*dpr)),sloped);
        settings.edge=sloped ? "square" : "trapezoid";
        wave::storeSignalStyleSettings(p.extensions,settings); canvas.viewport()->update();
        const auto changed=canvas.viewport()->grab().toImage();
        for (const auto row : {2,3}) {
            const QRect rect(qRound((edgeX+2)*dpr),qRound((34+row*44+2)*dpr),
                qRound((canvas.viewport()->width()-edgeX-4)*dpr),qRound(40*dpr));
            QVERIFY2(image.copy(rect)==changed.copy(rect),"Actual X/Z shape changed with normal edge style");
        }
    }

    void backgroundContrastPersistenceAndFallback()
    {
        auto p=styleProject();
        for (auto& lane : p.scenarios.front().lanes) wave::normalizeSegments(lane,true);
        for (const auto scheme : {wave::WaveformColorScheme::Light,wave::WaveformColorScheme::Dark}) {
            const auto base=wave::waveformTheme(scheme);
            QCOMPARE(wave::resolvedSignalBackground(nullptr,scheme).theme.canvas,base.canvas);
            auto legacy=p; legacy.extensions.clear();
            wave::SignalStyleSettings laneOverride; laneOverride.preset="cute";
            wave::storeSignalStyleSettings(legacy.scenarios.front().lanes[2].extensions,laneOverride);
            QCOMPARE(wave::resolvedSignalBackground(&legacy,scheme).theme.canvas,base.canvas);
            legacy.extensions["waveWorkbench.signalAppearance"]="{broken";
            QVERIFY(!wave::resolvedSignalBackground(&legacy,scheme).cycleBand.isValid());
            legacy.extensions["waveWorkbench.signalAppearance"]=R"({"preset":"missing"})";
            QCOMPARE(wave::resolvedSignalBackground(&legacy,scheme).theme.canvas,base.canvas);
            std::set<QRgb> canvases, bands;
            for (const auto& id : wave::signalStylePresetIds()) {
                wave::SignalStyleSettings settings; settings.preset=id;
                wave::storeSignalStyleSettings(p.extensions,settings);
                const auto background=wave::resolvedSignalBackground(&p,scheme);
                const auto& theme=background.theme;
                for (const auto& color : {theme.canvas,background.cycleBand,theme.panel,theme.raised,theme.selection}) {
                    QVERIFY(color.isValid()); QCOMPARE(color.alpha(),255);
                    QVERIFY2(wave::waveColorContrastRatio(theme.text,color)>=7.0,qPrintable(id));
                    QVERIFY2(wave::waveColorContrastRatio(theme.mutedText,color)>=4.5,qPrintable(id));
                }
                QVERIFY(wave::waveColorContrastRatio(theme.selectionText,theme.selection)>=7.0);
                QVERIFY(theme.canvas!=background.cycleBand);
                canvases.insert(theme.canvas.rgb()); bands.insert(background.cycleBand.rgb());
                for (const auto& lane : p.scenarios.front().lanes) {
                    if (lane.kind==wave::LaneKind::Group) continue;
                    const auto signal=wave::resolvedSignalStyle(&p,lane,scheme);
                    QVERIFY2(wave::waveColorContrastRatio(signal.color,theme.canvas)>=3.0,qPrintable(id));
                    QVERIFY2(wave::waveColorContrastRatio(signal.color,background.cycleBand)>=3.0,qPrintable(id));
                }
                const auto loaded=wave::deserializeProject(wave::serializeProject(p));
                QVERIFY(loaded.ok()); QVERIFY(*loaded.project==p);
                const auto restored=wave::resolvedSignalBackground(&*loaded.project,scheme);
                QCOMPARE(restored.theme.canvas,theme.canvas); QCOMPARE(restored.cycleBand,background.cycleBand);
                QCOMPARE(restored.minorGrid,background.minorGrid); QCOMPARE(restored.ruling,background.ruling);
            }
            QCOMPARE(canvases.size(),std::size_t(10)); QCOMPARE(bands.size(),std::size_t(10));
        }
    }

    void backgroundClockAlignmentAndIsolation()
    {
        for (const auto falling : {false,true}) {
            auto p=styleProject();
            auto& clock=p.clockDomains.front(); clock.period=12; clock.phase=3;
            clock.activeEdge=falling ? wave::ClockEdge::Falling : wave::ClockEdge::Rising;
            wave::SignalStyleSettings settings; settings.preset="blueprint";
            wave::storeSignalStyleSettings(p.extensions,settings);
            const auto original=p; const auto palette=qApp->palette();
            auto& scenario=p.scenarios.front(); wave::CommandStack commands; wave::WaveCanvas canvas;
            canvas.resize(1180,650); canvas.setDocument(&p,&scenario,&commands); canvas.show();
            canvas.rangeEditPaletteWidget()->hide(); QCoreApplication::processEvents(); canvas.fitScenario();
            canvas.selectLaneHeaders({"bit"},"bit");
            const auto background=wave::resolvedSignalBackground(&p,wave::waveformColorScheme(canvas.palette()));
            for (const auto zoomed : {false,true}) {
                if (zoomed) { canvas.zoomIn(); canvas.horizontalScrollBar()->setValue(41); }
                const auto image=canvas.viewport()->grab().toImage();
                const auto scale=double(canvas.viewport()->width()-canvas.signalHeaderWidth())/200*(zoomed ? 1.25 : 1);
                const auto at=[&](int tick) {
                    const int x=canvas.signalHeaderWidth()+qRound(tick*scale)-canvas.horizontalScrollBar()->value();
                    return image.pixelColor(qRound(x*image.devicePixelRatio()),qRound((canvas.viewport()->height()-25)*image.devicePixelRatio()));
                };
                const int anchor=falling ? 9 : 3;
                for (int cycle=2;cycle<8;++cycle) {
                    const auto expected=cycle%2 ? background.cycleBand : background.theme.canvas;
                    QCOMPARE(at(anchor+12*cycle+2),expected);
                    QCOMPARE(at(anchor+12*cycle+10),expected);
                }
                QVERIFY(canvas.setGroupCollapsed("Interface",true));
                QVERIFY(canvas.setGroupCollapsed("Interface",false));
                canvas.selectLaneHeaders({"bit"},"bit");
                const auto regrouped=canvas.viewport()->grab().toImage();
                const auto dpr=image.devicePixelRatio();
                const QRect emptyRegion(qRound((canvas.signalHeaderWidth()+2)*dpr),
                    qRound((canvas.viewport()->height()-40)*dpr),qRound((canvas.viewport()->width()-canvas.signalHeaderWidth()-4)*dpr),qRound(20*dpr));
                QCOMPARE(regrouped.copy(emptyRegion),image.copy(emptyRegion));
                QVERIFY(p==original); QCOMPARE(commands.size(),std::size_t(0)); QCOMPARE(qApp->palette(),palette);
            }
        }
    }

    void chromeAndExactUnits()
    {
        wave::MainWindow window(project());
        window.show(); QTest::qWait(150);
        QVERIFY(window.findChild<QWidget*>("WaveTitleBar"));
        auto* menus = window.findChild<QMenuBar*>("TitleMenuBar"); QVERIFY(menus); QVERIFY(menus->isVisible());
        QCOMPARE(menus->actions(),window.menuBar()->actions());
        QVERIFY(!window.menuBar()->isVisible()); QVERIFY(!window.statusBar()->isVisible());
        QVERIFY(!window.findChild<QLabel*>("SaveStateLabel"));
        auto* canvas = window.findChild<wave::WaveCanvas*>(); QVERIFY(canvas);
        auto* toolbar = window.findChild<QToolBar*>("WaveformToolbar"); QVERIFY(toolbar);
        auto* end = window.findChild<QLineEdit*>("TimelineDurationEdit"); QVERIFY(end);
        QVERIFY(toolbar->isAncestorOf(end)); QVERIFY(!canvas->viewport()->isAncestorOf(end));
        const auto actions = toolbar->actions();
        QVERIFY(actions.indexOf(window.findChild<QAction*>("ZoomInAction")) < actions.indexOf(window.findChild<QAction*>("MeasureToolAction")));
        auto* time = window.findChild<QLineEdit*>("GoToTimeEdit");
        auto* units = window.findChild<QComboBox*>("GoToTimeUnit");
        QVERIFY(time); QVERIFY(units); QCOMPARE(units->currentText(),QString("ns"));
        const auto original = window.project();
        time->setText("2.5"); QTest::keyClick(time,Qt::Key_Return);
        QCOMPARE(canvas->cursorTick(),wave::Tick(2'500));
        units->setCurrentText("us"); time->setText("0.031"); QTest::keyClick(time,Qt::Key_Return);
        QCOMPARE(canvas->cursorTick(),wave::Tick(31'000));
        time->setText("0.0000001"); QTest::keyClick(time,Qt::Key_Return);
        QCOMPARE(canvas->cursorTick(),wave::Tick(31'000));
        QCOMPARE(time->property("waveState").toString(),QString("error"));
        QVERIFY(window.project() == original);
        QTest::keyClick(time,Qt::Key_Escape);
        QCOMPARE(time->text(),QString("0.031"));
        wave::ui::dismissNotification(&window);
        screenshot(&window,"workspace");
        QWidget host;
        wave::MainWindow embedded(project(),{},&host);
        QVERIFY(!embedded.findChild<QWidget*>("WaveTitleBar"));
    }

    void resizeFitsAndPreservesContext()
    {
        wave::MainWindow window(project()); window.show(); QTest::qWait(150);
        auto* canvas = window.findChild<wave::WaveCanvas*>();
        canvas->revealLocation("lane-request",25'000);
        canvas->zoomIn(); canvas->zoomIn();
        QVERIFY(canvas->visibleTimeSpan() < 220'000);
        const auto before = window.project();
        window.resize(1180,700); QTest::qWait(180);
        QVERIFY(std::abs(canvas->visibleTimeSpan()-220'000) <= 1);
        QCOMPARE(canvas->horizontalScrollBar()->value(),0);
        QCOMPARE(canvas->cursorTick(),wave::Tick(25'000)); QCOMPARE(canvas->selectedLaneId(),QString("lane-request"));
        QVERIFY(window.project() == before);
        window.showMaximized(); QTest::qWait(180);
        QVERIFY(std::abs(canvas->visibleTimeSpan()-220'000) <= 1);
        window.showNormal(); QTest::qWait(180);
        QVERIFY(std::abs(canvas->visibleTimeSpan()-220'000) <= 1);
        QCOMPARE(canvas->cursorTick(),wave::Tick(25'000));
        QCOMPARE(canvas->selectedLaneId(),QString("lane-request"));
    }

    void creationAndRenameTypography()
    {
        wave::MainWindow window(project()); window.show(); QTest::qWait(150);
        auto* canvas = window.findChild<wave::WaveCanvas*>();
        canvas->beginQuickLaneSetup("lane-request",wave::LaneKind::Bit,"req",{}, {"clk"},{"clk"},"clk");
        QVERIFY(!canvas->findChild<QLabel*>("QuickLaneSetupPrompt"));
        QVERIFY(!canvas->findChild<QLabel*>("QuickLaneSetupHint"));
        QVERIFY(!canvas->findChild<QLabel*>("QuickLaneSetupError")->isVisible());
        auto* clock = canvas->findChild<QComboBox*>("QuickLaneClockCombo");
        QVERIFY(clock->isVisible()); QCOMPARE(clock->count(),2); QCOMPARE(clock->itemData(0).toString(),QString{});
        canvas->finishQuickLaneSetup();
        canvas->beginLaneRename("lane-request","req");
        auto* rename = canvas->findChild<QLineEdit*>("LaneRenameEdit"); QVERIFY(rename->isVisible());
        const auto expected = wave::ui::signalNameFont(canvas->viewport()->font());
        QCOMPARE(rename->font().family(),expected.family());
        QCOMPARE(rename->font().pointSizeF(),expected.pointSizeF()); QCOMPARE(rename->font().weight(),expected.weight());
        screenshot(&window,"rename"); canvas->finishLaneRename();
    }

    void busEditorIconsAndPreviews()
    {
        wave::MainWindow window(project()); window.show(); QTest::qWait(150);
        auto* canvas = window.findChild<wave::WaveCanvas*>();
        canvas->revealLocation("lane-data",30'000);
        const auto pos = QPoint(canvas->signalHeaderWidth()+int(30'000.0/canvas->visibleTimeSpan()*(canvas->viewport()->width()-canvas->signalHeaderWidth())),40+56*4+28);
        QTest::mouseDClick(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,pos);
        auto* palette = canvas->findChild<QWidget*>("BusPresetPalette"); QVERIFY(palette); QVERIFY(palette->isVisible());
        QVERIFY(!canvas->findChild<QToolButton*>("BusPresetZeroButton"));
        for (const auto name : {"BusPresetReservedButton","BusPresetDontCareButton","BusPresetXButton","BusPresetZButton"}) {
            auto* button = canvas->findChild<QToolButton*>(name); QVERIFY(button);
            QVERIFY(button->isVisible()); QVERIFY(!button->icon().isNull()); QCOMPARE(button->toolButtonStyle(),Qt::ToolButtonIconOnly);
            QVERIFY(!button->accessibleName().isEmpty());
        }
        screenshot(&window,"bus-editor");
        QTest::keyClick(canvas->findChild<QLineEdit*>("BusPresetValueEdit"),Qt::Key_Escape);
    }

    void quickPatternValidationAndPreview()
    {
        const wave::QuickBitPattern pattern{"01010101111010",10'000,true};
        const auto bytes = wave::encodeQuickBitPattern(pattern);
        const auto decoded = wave::decodeQuickBitPattern(bytes); QVERIFY(decoded); QCOMPARE(decoded->bits,pattern.bits);
        QVERIFY(!wave::validQuickBitPattern({"01X",1,false}));
        QVERIFY(!wave::validQuickBitPattern({"01",std::numeric_limits<wave::Tick>::max(),false}));
        QVERIFY(!wave::validQuickBitPattern({QString(wave::maxQuickPatternBits+1,'0'),1,false}));
        QVERIFY(!wave::decodeQuickBitPattern("{}"));
        QCOMPARE(wave::quickBitPatternStep({"01",10,false,{1000}},{1}),std::optional<wave::Tick>(10'000));
        QVERIFY(!wave::quickBitPatternStep({"01",1,false,{1}},{1000}));
        QVERIFY(!wave::quickBitPatternStep({"01",1,false,{0}},{1000}));
        QWidget owner;
        auto* composer = wave::createQuickWaveformComposer({1},10'000,true,&owner);
        auto* preview = composer->findChild<QWidget*>("QuickWaveformPreview");
        auto* confirm = composer->findChild<QPushButton*>("QuickWaveformConfirm");
        auto* input = composer->findChild<QLineEdit*>("QuickWaveformSequence");
        QVERIFY(!preview->property("fragmentReady").toBool());
        confirm->click(); QVERIFY(preview->property("fragmentReady").toBool());
        input->setText("010a"); QVERIFY(!confirm->isEnabled()); QVERIFY(!preview->property("fragmentReady").toBool());
        input->setText("01010101111010"); confirm->click();
        screenshot(composer,"quick-waveform"); composer->close();
    }

    void quickPatternCommandAndDrop()
    {
        auto p=project(); auto& s=p.scenarios.front(); wave::CommandStack commands;
        wave::findLane(s,"lane-request")->clockDomainId=p.clockDomains.front().id;
        wave::WaveCanvas canvas; canvas.resize(1200,700); canvas.setDocument(&p,&s,&commands); canvas.show();
        QCoreApplication::processEvents();
        const auto before=p;
        QString error;
        QVERIFY(canvas.applyQuickBitPattern("lane-request",21'000,{"0101",1,true},&error));
        const auto* edited=wave::findLane(s,"lane-request");
        QCOMPARE(sample(*edited,20'000),std::string("0"));
        QCOMPARE(sample(*edited,30'000),std::string("1"));
        QCOMPARE(sample(*edited,40'000),std::string("0"));
        QCOMPARE(sample(*edited,50'000),std::string("1"));
        for (const auto tick : {0,19'999,60'000,80'000,130'000})
            QCOMPARE(sample(*edited,tick),sample(*wave::findLane(before.scenarios.front(),"lane-request"),tick));
        QVERIFY(s.duration==before.scenarios.front().duration);
        QVERIFY(commands.canUndo()); QVERIFY(commands.undo()); QVERIFY(p==before);
        QVERIFY(commands.redo()); QVERIFY(p!=before); QVERIFY(commands.undo());
        QVERIFY(!canvas.applyQuickBitPattern("lane-data",0,{"0101",10'000,false},&error));
        QVERIFY(!canvas.applyQuickBitPattern("lane-request",210'000,{"0101",10'000,false},&error));
        QVERIFY(p==before);
        canvas.beginLaneRename("lane-request","req");
        QVERIFY(!canvas.applyQuickBitPattern("lane-request",0,{"01",10'000,false},&error));
        QVERIFY(p==before); canvas.finishLaneRename();
        QMimeData mime; mime.setData(wave::bitPatternMime,wave::encodeQuickBitPattern({"101",10'000,false}));
        const QPoint pos(canvas.signalHeaderWidth()+40,40+56*2+28);
        QDragEnterEvent enter(pos,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QCoreApplication::sendEvent(canvas.viewport(),&enter); QVERIFY(enter.isAccepted());
        QDropEvent drop(pos,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QCoreApplication::sendEvent(canvas.viewport(),&drop); QVERIFY(drop.isAccepted());
        QVERIFY(p!=before); QVERIFY(commands.undo()); QVERIFY(p==before);
        wave::findLane(s,"lane-request")->clockDomainId.clear();
        const auto asyncBefore=p;
        QVERIFY(canvas.applyQuickBitPattern("lane-request",21'123,{"10",5,false,{1000}},&error));
        QCOMPARE(sample(*wave::findLane(s,"lane-request"),21'122),std::string("0"));
        QCOMPARE(sample(*wave::findLane(s,"lane-request"),21'123),std::string("1"));
        QCOMPARE(sample(*wave::findLane(s,"lane-request"),26'122),std::string("1"));
        QCOMPARE(sample(*wave::findLane(s,"lane-request"),26'123),std::string("0"));
        QVERIFY(commands.undo()); QVERIFY(p==asyncBefore);
    }

    void stylesRenderOpaqueAndGrouped()
    {
        const auto savedPalette=qApp->palette();
        for (const auto scheme : {wave::WaveformColorScheme::Light,wave::WaveformColorScheme::Dark}) {
            auto palette=savedPalette;
            const auto theme=wave::waveformTheme(scheme);
            palette.setColor(QPalette::Window,theme.application); palette.setColor(QPalette::WindowText,theme.text);
            palette.setColor(QPalette::ButtonText,theme.text); palette.setColor(QPalette::Text,theme.text);
            qApp->setPalette(palette); wave::ui::applyTheme(scheme);
            for (const auto& id : wave::signalStylePresetIds()) {
                auto p=styleProject(); wave::SignalStyleSettings settings; settings.preset=id;
                wave::storeSignalStyleSettings(p.extensions,settings);
                auto& s=p.scenarios.front(); const auto original=p;
                wave::CommandStack commands; wave::WaveCanvas canvas;
                canvas.resize(1180,570); canvas.setDocument(&p,&s,&commands);
                canvas.rangeEditPaletteWidget()->hide(); canvas.show();
                QCoreApplication::processEvents(); canvas.fitScenario();
                const auto image=canvas.viewport()->grab().toImage();
                const auto scale=double(canvas.viewport()->width()-canvas.signalHeaderWidth())/200;
                const auto colorAt=[&](double tick,int row,int dy) {
                    const QPoint logical(canvas.signalHeaderWidth()+int(tick*scale),80+row*56+dy);
                    return image.pixelColor(int(logical.x()*image.devicePixelRatio()),int(logical.y()*image.devicePixelRatio()));
                };
                for (const auto row : {2,4,5,6}) {
                    const auto fill=wave::resolvedSignalStyle(&p,s.lanes.at(row+1),scheme).fill;
                    for (const auto tick : {30,35,40,80,85,90}) QCOMPARE(colorAt(tick,row,17),fill);
                }
                const auto background=wave::resolvedSignalBackground(&p,scheme);
                for (const auto row : {0,1,2,3,4,5,6}) {
                    QCOMPARE(colorAt(35,row,4),background.cycleBand);
                    QCOMPARE(colorAt(85,row,4),background.theme.canvas);
                }
                // State textures and symbols remain distinct within the same canvas geometry.
                int differences=0;
                for (int y=10;y<44;++y)
                    for (int sample=105;sample<490;++sample)
                        if (qGray(colorAt(sample/5.0,3,y).rgb()) != qGray(colorAt(sample/5.0,4,y).rgb())) ++differences;
                QVERIFY2(differences>50,qPrintable(id)); QVERIFY(p==original);
                screenshot(&canvas,"style-"+id+(scheme==wave::WaveformColorScheme::Dark ? "-dark" : "-light"));
                QVERIFY(canvas.setGroupCollapsed("Interface",true));
                QVERIFY(canvas.setGroupCollapsed("Interface",false));
                QVERIFY(p==original);
                canvas.beginLaneRename("Interface","Interface");
                auto* name=canvas.findChild<QLineEdit*>("LaneRenameEdit");
                QCOMPARE(name->font().weight(),wave::ui::signalNameFont(canvas.viewport()->font(),true).weight());
                canvas.finishLaneRename();
            }
        }
        qApp->setPalette(savedPalette); wave::ui::applyTheme(wave::waveformColorScheme(savedPalette));
    }

    void bindingAndStyleDialogsUndo()
    {
        auto p=project(); auto other=p.clockDomains.front(); other.id="slow-clock"; other.name="Slow clock"; other.period*=2;
        p.clockDomains.push_back(other);
        wave::MainWindow window(p); window.show(); QTest::qWait(150);
        auto* undo=window.findChild<QAction*>("UndoAction"); auto* redo=window.findChild<QAction*>("RedoAction");
        QTimer guard; guard.setSingleShot(true);
        connect(&guard,&QTimer::timeout,&window,[] {
            if (auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
        });
        for (const auto& clockId : {QString("slow-clock"),QString{}}) {
            bool inspected=false;
            QTimer::singleShot(0,&window,[&] {
                auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()); QVERIFY(dialog);
                auto* binding=dialog->findChild<QComboBox*>("LanePropertiesClockCombo"); QVERIFY(binding);
                binding->setCurrentIndex(binding->findData(clockId));
                auto* preset=dialog->findChild<QComboBox*>("SignalStylePreset"); QVERIFY(preset);
                preset->setCurrentIndex(preset->findData("cute"));
                QVERIFY(!dialog->findChild<QComboBox*>("SignalStyleEdge"));
                QVERIFY(!dialog->findChild<QComboBox*>("SignalStyleCorners"));
                screenshot(dialog,"signal-properties"); inspected=true;
                dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            });
            guard.start(2000); window.openLanePropertiesPreview("lane-request"); guard.stop(); QVERIFY(inspected);
            const auto changed=window.project(); const auto* lane=wave::findLane(changed.scenarios.front(),"lane-request");
            QCOMPARE(QString::fromStdString(lane->clockDomainId),clockId);
            QCOMPARE(wave::signalStyleSettings(lane->extensions).preset,QString("cute"));
            QVERIFY(!wave::resolvedSignalStyle(&changed,*lane,wave::WaveformColorScheme::Light).trapezoid);
            const auto loaded=wave::deserializeProject(wave::serializeProject(changed)); QVERIFY(loaded.ok()); QVERIFY(*loaded.project==changed);
            undo->trigger(); QVERIFY(window.project()==p); redo->trigger(); QVERIFY(window.project()==changed); undo->trigger();
        }
        auto* styleAction=window.findChild<QAction*>("SignalStyleDefaultsAction"); QVERIFY(styleAction);
        QTimer::singleShot(0,&window,[&] {
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()); QVERIFY(dialog);
            auto* preset=dialog->findChild<QComboBox*>("SignalStylePreset");
            preset->setCurrentIndex(preset->findData("academic"));
            auto* edge=dialog->findChild<QComboBox*>("SignalStyleEdge"); QVERIFY(edge);
            edge->setCurrentIndex(edge->findData("trapezoid")); screenshot(dialog,"style-defaults");
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        });
        guard.start(2000); styleAction->trigger(); guard.stop();
        QCOMPARE(wave::signalStyleSettings(window.project().extensions).preset,QString("academic"));
        const auto paired=wave::resolvedSignalBackground(&window.project(),wave::WaveformColorScheme::Light);
        QVERIFY(paired.cycleBand.isValid());
        const auto styled=window.project();
        for (const auto& lane : styled.scenarios.front().lanes)
            QVERIFY(wave::resolvedSignalStyle(&styled,lane,wave::WaveformColorScheme::Light).trapezoid);
        undo->trigger(); QVERIFY(window.project()==p);
        QCOMPARE(wave::resolvedSignalBackground(&window.project(),wave::WaveformColorScheme::Light).theme.canvas,
            wave::waveformTheme(wave::WaveformColorScheme::Light).canvas);
        redo->trigger(); QVERIFY(window.project()==styled);
        QCOMPARE(wave::resolvedSignalBackground(&window.project(),wave::WaveformColorScheme::Light).theme.canvas,paired.theme.canvas);
        undo->trigger();
        auto* canvas=window.findChild<wave::WaveCanvas*>();
        window.findChild<QToolButton*>("CanvasAddBitButton")->click();
        auto* clock=canvas->findChild<QComboBox*>("QuickLaneClockCombo"); clock->setCurrentIndex(0);
        QTest::keyClick(canvas->findChild<QLineEdit*>("QuickLaneNameEdit"),Qt::Key_Return);
        QVERIFY(!canvas->hasQuickLaneSetup());
        QVERIFY(wave::findLane(window.project().scenarios.front(),canvas->selectedLaneId().toStdString())->clockDomainId.empty());
        undo->trigger(); QVERIFY(window.project()==p);
    }

    void styleApplyViaPopupAndButton()
    {
        const auto motion=qApp->property("waveworkbench.reducedMotion");
        const auto restoreMotion=qScopeGuard([&] { qApp->setProperty("waveworkbench.reducedMotion",motion); });
        qApp->setProperty("waveworkbench.reducedMotion",false);
        wave::MainWindow window(styleProject()); window.show(); QTest::qWait(150);
        auto* action=window.findChild<QAction*>("SignalStyleDefaultsAction"); QVERIFY(action);
        QTimer guard; guard.setSingleShot(true);
        connect(&guard,&QTimer::timeout,&window,[] {
            if (auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
        });
        for (const auto& id : wave::signalStylePresetIds()) for (const bool accept : {true,false}) {
            const auto before=window.project();
            bool clicked=false;
            QPointer<QComboBox> ownedPreset;
            QTimer::singleShot(0,&window,[&] {
                auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()); QVERIFY(dialog);
                auto* preset=dialog->findChild<QComboBox*>("SignalStylePreset"); QVERIFY(preset);
                ownedPreset=preset;
                QTest::mouseClick(preset,Qt::LeftButton);
                QTest::qWait(50);
                QVERIFY(preset->view()->isVisible());
                QTest::keyClick(preset->view(),Qt::Key_Home);
                for (int index=0;index<preset->findData(id);++index)
                    QTest::keyClick(preset->view(),Qt::Key_Down);
                QTest::keyClick(preset->view(),Qt::Key_Return);
                QCOMPARE(preset->currentData().toString(),id);
                QTest::qWait(50);
                auto* button=dialog->findChild<QDialogButtonBox*>()->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel);
                QEnterEvent hover(button->rect().center(),button->rect().center(),button->mapToGlobal(button->rect().center()));
                QCoreApplication::sendEvent(button,&hover);
                QTest::qWait(80);
                QTest::mouseClick(button,Qt::LeftButton);
                clicked=true;
            });
            guard.start(3000); action->trigger(); guard.stop(); QVERIFY(clicked);
            QVERIFY(ownedPreset.isNull());
            if (accept) {
                QCOMPARE(wave::signalStyleSettings(window.project().extensions).preset,id);
                const auto after=window.project();
                window.findChild<QAction*>("UndoAction")->trigger(); QVERIFY(window.project()==before);
                window.findChild<QAction*>("RedoAction")->trigger(); QVERIFY(window.project()==after);
            } else QVERIFY(window.project()==before);
            QTest::qWait(80);
        }
    }

    void firstShowGeometry()
    {
        wave::MainWindow window(styleProject());
        QVERIFY(!window.testAttribute(Qt::WA_Mapped));
        window.resize(1180,760);
        window.show(); QTest::qWait(200);
        auto* title=window.findChild<QWidget*>("WaveTitleBar"); QVERIFY(title);
        auto* bar=window.findChild<QToolBar*>("WaveformToolbar"); QVERIFY(bar);
#ifdef Q_OS_WIN
        if (QGuiApplication::platformName()=="windows") {
            const auto handle=reinterpret_cast<HWND>(window.winId());
            RECT client{}; POINT origin{};
            QVERIFY(GetClientRect(handle,&client));
            QVERIFY(ClientToScreen(handle,&origin));
            QCOMPARE(qRound((client.right-client.left)/window.devicePixelRatioF()),window.width());
            QCOMPARE(qRound((client.bottom-client.top)/window.devicePixelRatioF()),window.height());
        }
#endif
        QCOMPARE(title->pos(),QPoint(0,0));
        QCOMPARE(title->width(),window.width());
        QVERIFY(bar->geometry().top()>=title->geometry().bottom());
        auto* close=title->findChild<QPushButton*>("ElaCloseButton"); QVERIFY(close); QVERIFY(close->isVisible());
        QVERIFY(title->rect().contains(QRect(close->mapTo(title,QPoint()),close->size())));
        const auto closeImage=close->grab().toImage();
        int ink=0;
        for (int y=closeImage.height()/4;y<closeImage.height()*3/4;++y)
            for (int x=closeImage.width()/4;x<closeImage.width()*3/4;++x)
                if (qGray(closeImage.pixel(x,y))<100) ++ink;
        QVERIFY(ink>10);
        auto* units=window.findChild<QComboBox*>("GoToTimeUnit"); QVERIFY(units);
        QStyleOptionComboBox option; option.initFrom(units); option.currentText="ns";
        const auto textRect=units->style()->subControlRect(QStyle::CC_ComboBox,&option,QStyle::SC_ComboBoxEditField,units);
        QVERIFY(textRect.width()>=units->fontMetrics().horizontalAdvance("ns"));
        const auto initialSize=window.size();
        window.resize(initialSize+QSize(100,0)); QTest::qWait(150);
        window.resize(initialSize); QTest::qWait(150);
        QCOMPARE(title->width(),window.width());
        QVERIFY(bar->geometry().top()>=title->geometry().bottom());
        screenshot(&window,"first-show");
    }

    void englishBuiltInText()
    {
        QVERIFY(QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs));
        QCOMPARE(wave::signalStylePresetNames(),QStringList({"Academic","Cute","Minimal","Engineering","Blueprint",
            "Retro terminal","Oscilloscope","Paper","High contrast","Faceted tech"}));
        const QRegularExpression han(QStringLiteral("\\p{sc=Han}"));
        QVERIFY(han.isValid());
        QVERIFY(!han.match(QStringLiteral("English · separator")).hasMatch());
        wave::MainWindow window(styleProject()); window.show(); QTest::qWait(100);
        for (auto* widget : window.findChildren<QWidget*>())
            for (const auto* property : {"text","windowTitle","placeholderText","toolTip","accessibleName","accessibleDescription"}) {
                const auto value=widget->property(property).toString();
                QVERIFY2(!han.match(value).hasMatch(),qPrintable(widget->objectName()+": "+value));
            }
        auto* systemMenu=window.findChild<QMenu*>("WindowSystemMenu"); QVERIFY(systemMenu);
        QVERIFY(systemMenu->actions().size()>=4);
        for (auto* action : window.findChildren<QAction*>()) QVERIFY2(!han.match(action->text()).hasMatch(),qPrintable(action->text()));
        QFileDialog picker(&window,QStringLiteral("Open project"),settings_.path());
        picker.show(); QCoreApplication::processEvents();
        auto* fileName=picker.findChild<QLineEdit*>("fileNameEdit"); QVERIFY(fileName);
        QCOMPARE(picker.labelText(QFileDialog::FileName),QStringLiteral("File &name:"));
        picker.reject();
        ElaLineEdit edit; ElaSpinBox spin; ElaDoubleSpinBox decimal;
        for (QWidget* field : {static_cast<QWidget*>(&edit),static_cast<QWidget*>(&spin),static_cast<QWidget*>(&decimal)}) {
            field->show(); QStringList labels;
            QTimer inspectMenu;
            connect(&inspectMenu,&QTimer::timeout,field,[&] {
                for (auto* popup : field->findChildren<QMenu*>()) {
                    if (!popup->isVisible()) continue;
                    for (auto* action : popup->actions()) if (!action->isSeparator()) labels.append(action->text());
                    inspectMenu.stop(); popup->close(); break;
                }
            });
            inspectMenu.start(10);
            QContextMenuEvent context(QContextMenuEvent::Keyboard,QPoint(5,5),field->mapToGlobal(QPoint(5,5)));
            QCoreApplication::sendEvent(field,&context); QCoreApplication::processEvents();
            QTRY_VERIFY(!labels.isEmpty());
            for (const auto& label : labels) QVERIFY2(!han.match(label).hasMatch(),qPrintable(label));
        }
        const auto userText=QStringLiteral("用户信号");
        auto p=styleProject(); p.name=userText.toStdString();
        const auto loaded=wave::deserializeProject(wave::serializeProject(p));
        QVERIFY(loaded.ok()); QCOMPARE(QString::fromStdString(loaded.project->name),userText);
    }

    void titleClosePreservesCancelledDraft()
    {
        wave::MainWindow window(styleProject()); window.show(); QTest::qWait(100);
        auto* end=window.findChild<QLineEdit*>("TimelineDurationEdit"); QVERIFY(end);
        end->setText("210 ns"); QTest::keyClick(end,Qt::Key_Return);
        QVERIFY(window.windowTitle().contains('*'));
        const auto before=window.project();
        auto* close=window.findChild<QPushButton*>("ElaCloseButton"); QVERIFY(close);
        bool cancelled=false;
        QTimer guard; guard.setSingleShot(true);
        connect(&guard,&QTimer::timeout,&window,[] {
            if (auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
        });
        QTimer::singleShot(0,&window,[&] {
            auto* dialog=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); QVERIFY(dialog);
            auto* cancel=dialog->button(QMessageBox::Cancel); QVERIFY(cancel);
            cancel->click(); cancelled=true;
        });
        guard.start(3000); close->click(); guard.stop();
        QVERIFY(cancelled); QVERIFY(window.isVisible()); QVERIFY(window.project()==before);
        QVERIFY(window.windowTitle().contains('*'));
    }
};
QTEST_MAIN(UiFollowupTest)
#include "ui_followup_test.moc"
