#include "main_window.h"
#include "wave_canvas.h"
#include "signal_style.h"
#include "quick_waveform.h"
#include "ui_controls.h"
#include "wave/project_io.h"

#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFontDatabase>
#include <QFileInfo>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QHelpEvent>
#include <QLineEdit>
#include <QMenuBar>
#include <QMimeData>
#include <QPushButton>
#include <QRawFont>
#include <QScrollBar>
#include <QStatusBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <limits>
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

class UiFollowupTest : public QObject {
    Q_OBJECT
    QTemporaryDir settings_;
private slots:
    void initTestCase()
    {
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
        QCOMPARE(resolved.stroke,3.0); QVERIFY(!resolved.trapezoid); QCOMPARE(resolved.fill,QColor("#557799"));
        QVERIFY(lane.segments == values); QVERIFY(p.scenarios.front().events == events);
        lane.extensions["waveWorkbench.signalAppearance"] = R"({"preset":"bad","stroke":-99,"fill":"invalid"})";
        resolved = wave::resolvedSignalStyle(&p,lane,wave::WaveformColorScheme::Light);
        QVERIFY(resolved.fill.isValid()); QVERIFY(resolved.stroke > 0);
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
                // Geometry/texture differ even when rendered with the same monochrome preset.
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
                auto* edge=dialog->findChild<QComboBox*>("SignalStyleEdge"); edge->setCurrentIndex(edge->findData("trapezoid"));
                screenshot(dialog,"signal-properties"); inspected=true;
                dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            });
            guard.start(2000); window.openLanePropertiesPreview("lane-request"); guard.stop(); QVERIFY(inspected);
            const auto changed=window.project(); const auto* lane=wave::findLane(changed.scenarios.front(),"lane-request");
            QCOMPARE(QString::fromStdString(lane->clockDomainId),clockId);
            QCOMPARE(wave::signalStyleSettings(lane->extensions).preset,QString("cute"));
            QVERIFY(wave::resolvedSignalStyle(&changed,*lane,wave::WaveformColorScheme::Light).trapezoid);
            const auto loaded=wave::deserializeProject(wave::serializeProject(changed)); QVERIFY(loaded.ok()); QVERIFY(*loaded.project==changed);
            undo->trigger(); QVERIFY(window.project()==p); redo->trigger(); QVERIFY(window.project()==changed); undo->trigger();
        }
        auto* styleAction=window.findChild<QAction*>("SignalStyleDefaultsAction"); QVERIFY(styleAction);
        QTimer::singleShot(0,&window,[&] {
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()); QVERIFY(dialog);
            auto* preset=dialog->findChild<QComboBox*>("SignalStylePreset");
            preset->setCurrentIndex(preset->findData("academic")); screenshot(dialog,"style-defaults");
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        });
        guard.start(2000); styleAction->trigger(); guard.stop();
        QCOMPARE(wave::signalStyleSettings(window.project().extensions).preset,QString("academic"));
        const auto paired=wave::resolvedSignalBackground(&window.project(),wave::WaveformColorScheme::Light);
        QVERIFY(paired.cycleBand.isValid());
        const auto styled=window.project();
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
};
QTEST_MAIN(UiFollowupTest)
#include "ui_followup_test.moc"
