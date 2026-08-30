#include "mainwindow.h"
#include "appstyle.h"
#include "audiochip.h"
#include "audiolevelmonitor.h"
#include "captureexclude.h"
#include "countdownoverlay.h"
#include "floatingbar.h"
#include "hotkeymanager.h"
#include "previewwidget.h"
#include "regionselector.h"
#include "settings.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QIcon>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QSize>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QMediaRecorder>

namespace {

QString formatDuration(qint64 milliseconds)
{
    const qint64 total = qMax(qint64(0), milliseconds) / 1000;
    const int hours = int(total / 3600);
    const int minutes = int((total % 3600) / 60);
    const int seconds = int(total % 60);
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 1);
}

void polishWidget(QWidget *widget)
{
    if (!widget || !widget->style())
        return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QWidget *makeSettingsCard(const QString &title, QWidget *parent, QVBoxLayout **bodyOut)
{
    auto *card = new QWidget(parent);
    card->setObjectName(QStringLiteral("settingsCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    auto *root = new QVBoxLayout(card);
    root->setContentsMargins(14, 12, 14, 14);
    root->setSpacing(10);
    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("cardTitle"));
    root->addWidget(titleLabel);
    *bodyOut = root;
    return card;
}

void addSettingsField(QVBoxLayout *layout, QWidget *parent, const QString &text, QWidget *field)
{
    auto *row = new QWidget(parent);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(12);
    auto *label = new QLabel(text, row);
    label->setObjectName(QStringLiteral("fieldLabel"));
    label->setFixedWidth(64);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    rowLayout->addWidget(label);
    rowLayout->addWidget(field, 1);
    layout->addWidget(row);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settings(new AppSettings(this))
    , m_controller(new RecordingController(this))
    , m_overlay(new CountdownOverlay)
    , m_regionSelector(new RegionSelector)
    , m_floatingBar(new FloatingBar)
    , m_hotkeys(new HotkeyManager(this))
    , m_audioMonitor(new AudioLevelMonitor(this))
{
    setWindowIcon(QIcon(QStringLiteral(":/assets/app.png")));
    setupUi();
    setupTray();
    m_loadingSettings = true;
    connectSignals();
    refreshScreens();
    loadSettings();
    m_loadingSettings = false;
    updateModeRows();
    updateRegionLabel();
    updateHint();
    updateSummary();
    updateUi();

    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(300);
    connect(m_previewTimer, &QTimer::timeout, this, &MainWindow::updatePreview);
    syncPreviewTimer();
    syncAudioMonitor();
}

MainWindow::~MainWindow()
{
    delete m_overlay;
    delete m_regionSelector;
    delete m_floatingBar;
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("ScreenRec"));
    resize(460, 620);
    setMinimumSize(400, 540);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralRoot"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget(central);
    m_recorderPage = new QWidget;
    m_recorderPage->setObjectName(QStringLiteral("page"));
    m_settingsPage = new QWidget;
    m_settingsPage->setObjectName(QStringLiteral("page"));

    setupRecorderPage();
    setupSettingsPage();

    m_stack->addWidget(m_recorderPage);
    m_stack->addWidget(m_settingsPage);
    root->addWidget(m_stack);
    setCentralWidget(central);
    statusBar()->showMessage(tr("选择画面后点击开始录制"));
}

void MainWindow::setupRecorderPage()
{
    auto *layout = new QVBoxLayout(m_recorderPage);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    m_preview = new PreviewWidget(m_recorderPage);
    m_preview->setPlaceholder(tr("预览"), tr("选择模式后将显示画面"));
    layout->addWidget(m_preview, 1);

    m_summaryButton = new QPushButton(m_recorderPage);
    m_summaryButton->setObjectName(QStringLiteral("summaryButton"));
    m_summaryButton->setCursor(Qt::PointingHandCursor);
    m_summaryButton->setToolTip(tr("打开设置"));
    layout->addWidget(m_summaryButton);

    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);
    auto makeSeg = [this](const QString &text, const QIcon &icon, const QString &pos, int id) {
        auto *button = new QToolButton(m_recorderPage);
        button->setText(text);
        button->setIcon(icon);
        button->setIconSize(QSize(16, 16));
        button->setCheckable(true);
        button->setObjectName(QStringLiteral("segBtn"));
        button->setProperty("pos", pos);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_modeGroup->addButton(button, id);
        polishWidget(button);
        return button;
    };
    m_modeScreenBtn = makeSeg(tr("整屏"), AppIcons::screen(), QStringLiteral("first"),
                              int(RecordingController::CaptureMode::Screen));
    m_modeRegionBtn = makeSeg(tr("区域"), AppIcons::region(), QStringLiteral("last"),
                              int(RecordingController::CaptureMode::Region));
    m_modeScreenBtn->setChecked(true);
    m_modeScreenBtn->setToolTip(tr("录制整个显示器"));
    m_modeRegionBtn->setToolTip(tr("拖拽选择一部分屏幕"));

    auto *modeRow = new QHBoxLayout;
    modeRow->setSpacing(0);
    modeRow->addWidget(m_modeScreenBtn);
    modeRow->addWidget(m_modeRegionBtn);
    layout->addLayout(modeRow);

    m_screenRow = new QWidget(m_recorderPage);
    auto *screenLayout = new QHBoxLayout(m_screenRow);
    screenLayout->setContentsMargins(0, 0, 0, 0);
    m_screenCombo = AppStyle::createComboBox(m_screenRow);
    m_screenCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    screenLayout->addWidget(m_screenCombo);
    layout->addWidget(m_screenRow);

    m_regionRow = new QWidget(m_recorderPage);
    auto *regionLayout = new QHBoxLayout(m_regionRow);
    regionLayout->setContentsMargins(0, 0, 0, 0);
    regionLayout->setSpacing(8);
    m_regionLabel = new QLabel(m_regionRow);
    m_regionButton = new QPushButton(tr("选择区域"), m_regionRow);
    m_clearRegionButton = new QPushButton(tr("清空"), m_regionRow);
    m_regionButton->setCursor(Qt::PointingHandCursor);
    m_clearRegionButton->setCursor(Qt::PointingHandCursor);
    m_clearRegionButton->setToolTip(tr("清除已选区域"));
    m_clearRegionButton->hide();
    regionLayout->addWidget(m_regionLabel, 1);
    regionLayout->addWidget(m_regionButton);
    regionLayout->addWidget(m_clearRegionButton);
    layout->addWidget(m_regionRow);

    auto *audioRow = new QHBoxLayout;
    audioRow->setSpacing(8);
    m_micCheck = new AudioChip(tr("麦克风"), m_recorderPage);
    m_systemAudioCheck = new AudioChip(tr("系统声音"), m_recorderPage);
    m_micCheck->setIcon(AppIcons::mic());
    m_systemAudioCheck->setIcon(AppIcons::speaker());
    m_micCheck->setIconSize(QSize(16, 16));
    m_systemAudioCheck->setIconSize(QSize(16, 16));
    m_micCheck->setIdleTip(tr("录制麦克风输入"));
    m_systemAudioCheck->setIdleTip(tr("录制电脑正在播放的声音。开始前请先让内容出声。"));
    m_micCheck->setSilentHint(tr("未检测到麦克风输入"));
    m_systemAudioCheck->setSilentHint(tr("未检测到系统声音。开始前请先让内容出声。"));
    polishWidget(m_micCheck);
    polishWidget(m_systemAudioCheck);
    audioRow->addWidget(m_micCheck);
    audioRow->addWidget(m_systemAudioCheck);
    layout->addLayout(audioRow);

    m_startButton = new QPushButton(tr("开始录制"), m_recorderPage);
    m_startButton->setObjectName(QStringLiteral("primaryButton"));
    m_startButton->setIcon(AppIcons::record(Qt::white));
    m_startButton->setIconSize(QSize(16, 16));
    m_startButton->setCursor(Qt::PointingHandCursor);
    m_startButton->setDefault(true);
    layout->addWidget(m_startButton);

    m_hintLabel = new QLabel(m_recorderPage);
    m_hintLabel->setObjectName(QStringLiteral("hintLabel"));
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);
}

void MainWindow::setupSettingsPage()
{
    auto *pageLayout = new QVBoxLayout(m_settingsPage);
    pageLayout->setContentsMargins(16, 14, 16, 14);
    pageLayout->setSpacing(12);

    auto *header = new QHBoxLayout;
    header->setSpacing(4);
    m_backButton = new QToolButton(m_settingsPage);
    m_backButton->setObjectName(QStringLiteral("iconButton"));
    m_backButton->setIcon(AppIcons::back());
    m_backButton->setIconSize(QSize(20, 20));
    m_backButton->setCursor(Qt::PointingHandCursor);
    m_backButton->setToolTip(tr("返回"));
    auto *title = new QLabel(tr("设置"), m_settingsPage);
    title->setObjectName(QStringLiteral("titleLabel"));
    header->addWidget(m_backButton);
    header->addWidget(title);
    header->addStretch(1);
    pageLayout->addLayout(header);

    auto *scroll = new QScrollArea(m_settingsPage);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *inner = new QWidget;
    inner->setObjectName(QStringLiteral("page"));
    auto *innerLayout = new QVBoxLayout(inner);
    innerLayout->setContentsMargins(0, 0, 4, 0);
    innerLayout->setSpacing(12);

    QVBoxLayout *saveLayout = nullptr;
    innerLayout->addWidget(makeSettingsCard(tr("保存位置"), inner, &saveLayout));
    auto *pathRow = new QWidget(inner);
    auto *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(8);
    m_pathEdit = new QLineEdit(pathRow);
    m_pathEdit->setPlaceholderText(tr("视频保存目录"));
    m_browseButton = new QPushButton(tr("浏览"), pathRow);
    m_openFolderButton = new QPushButton(pathRow);
    m_openFolderButton->setIcon(AppIcons::folder());
    m_openFolderButton->setIconSize(QSize(16, 16));
    m_openFolderButton->setToolTip(tr("打开目录"));
    m_openFolderButton->setFixedWidth(40);
    m_browseButton->setCursor(Qt::PointingHandCursor);
    m_openFolderButton->setCursor(Qt::PointingHandCursor);
    pathLayout->addWidget(m_pathEdit, 1);
    pathLayout->addWidget(m_browseButton);
    pathLayout->addWidget(m_openFolderButton);
    saveLayout->addWidget(pathRow);

    QVBoxLayout *pictureLayout = nullptr;
    innerLayout->addWidget(makeSettingsCard(tr("画面"), inner, &pictureLayout));
    m_resolutionCombo = AppStyle::createComboBox(inner);
    m_resolutionCombo->addItem(tr("原始"), QSize());
    m_resolutionCombo->addItem(QStringLiteral("1080p"), QSize(1920, 1080));
    m_resolutionCombo->addItem(QStringLiteral("720p"), QSize(1280, 720));
    m_fpsCombo = AppStyle::createComboBox(inner);
    m_fpsCombo->addItem(QStringLiteral("24 FPS"), 24);
    m_fpsCombo->addItem(QStringLiteral("30 FPS"), 30);
    m_fpsCombo->addItem(QStringLiteral("60 FPS"), 60);
    m_qualityCombo = AppStyle::createComboBox(inner);
    m_qualityCombo->addItem(tr("低 · 体积小"), int(QMediaRecorder::LowQuality));
    m_qualityCombo->addItem(tr("标准"), int(QMediaRecorder::NormalQuality));
    m_qualityCombo->addItem(tr("高 · 推荐"), int(QMediaRecorder::HighQuality));
    m_qualityCombo->addItem(tr("最高 · 体积大"), int(QMediaRecorder::VeryHighQuality));
    m_countdownCombo = AppStyle::createComboBox(inner);
    m_countdownCombo->addItem(tr("立即开始"), 0);
    m_countdownCombo->addItem(tr("3 秒倒计时"), 3);
    m_countdownCombo->addItem(tr("5 秒倒计时"), 5);
    m_resolutionCombo->setToolTip(tr("限制输出分辨率。原始表示按采集尺寸编码。"));
    m_fpsCombo->setToolTip(tr("目标帧率，实际帧率取决于屏幕刷新和机器性能。"));
    m_qualityCombo->setToolTip(tr("编码器质量档。越高文件越大。"));
    addSettingsField(pictureLayout, inner, tr("分辨率"), m_resolutionCombo);
    addSettingsField(pictureLayout, inner, tr("帧率"), m_fpsCombo);
    addSettingsField(pictureLayout, inner, tr("画质"), m_qualityCombo);
    addSettingsField(pictureLayout, inner, tr("倒计时"), m_countdownCombo);

    QVBoxLayout *hotkeyLayout = nullptr;
    innerLayout->addWidget(makeSettingsCard(tr("热键"), inner, &hotkeyLayout));
    m_hotkeyCheck = new QCheckBox(tr("启用全局热键"), inner);
    m_hotkeyCheck->setObjectName(QStringLiteral("settingsCheck"));
    m_hotkeyCheck->setCursor(Qt::PointingHandCursor);
    hotkeyLayout->addWidget(m_hotkeyCheck);

    m_hotkeyRow = new QWidget(inner);
    auto *hotkeyFields = new QVBoxLayout(m_hotkeyRow);
    hotkeyFields->setContentsMargins(0, 0, 0, 0);
    hotkeyFields->setSpacing(10);
    m_startHotkeyEdit = new QKeySequenceEdit(m_hotkeyRow);
    m_stopHotkeyEdit = new QKeySequenceEdit(m_hotkeyRow);
    m_pauseHotkeyEdit = new QKeySequenceEdit(m_hotkeyRow);
    m_startHotkeyEdit->setMaximumSequenceLength(1);
    m_stopHotkeyEdit->setMaximumSequenceLength(1);
    m_pauseHotkeyEdit->setMaximumSequenceLength(1);
    addSettingsField(hotkeyFields, m_hotkeyRow, tr("开始"), m_startHotkeyEdit);
    addSettingsField(hotkeyFields, m_hotkeyRow, tr("停止"), m_stopHotkeyEdit);
    addSettingsField(hotkeyFields, m_hotkeyRow, tr("暂停"), m_pauseHotkeyEdit);
    hotkeyLayout->addWidget(m_hotkeyRow);

    m_hotkeyError = new QLabel(inner);
    m_hotkeyError->setObjectName(QStringLiteral("errorLabel"));
    m_hotkeyError->setWordWrap(true);
    m_hotkeyError->hide();
    hotkeyLayout->addWidget(m_hotkeyError);

    m_resetHotkeysButton = new QPushButton(tr("恢复默认热键"), inner);
    m_resetHotkeysButton->setCursor(Qt::PointingHandCursor);
    hotkeyLayout->addWidget(m_resetHotkeysButton, 0, Qt::AlignLeft);

    innerLayout->addStretch(1);

    scroll->setWidget(inner);
    pageLayout->addWidget(scroll, 1);

    auto *version = new QLabel(
        tr("ScreenRec %1").arg(QApplication::applicationVersion()), m_settingsPage);
    version->setObjectName(QStringLiteral("hintLabel"));
    version->setAlignment(Qt::AlignCenter);
    pageLayout->addWidget(version);
}

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(trayIcon(), this);
    m_tray->setToolTip(QStringLiteral("ScreenRec"));

    auto *menu = new QMenu(this);
    auto *showAction = menu->addAction(tr("显示主窗口"));
    m_trayStart = menu->addAction(tr("开始录制"));
    m_trayPause = menu->addAction(tr("暂停 / 继续"));
    m_trayStop = menu->addAction(tr("停止"));
    menu->addSeparator();
    auto *quitAction = menu->addAction(tr("退出"));
    m_tray->setContextMenu(menu);
    m_tray->show();

    connect(showAction, &QAction::triggered, this, &MainWindow::restoreMainWindow);
    connect(m_trayStart, &QAction::triggered, this, &MainWindow::onStartClicked);
    connect(m_trayPause, &QAction::triggered, this, &MainWindow::onPauseClicked);
    connect(m_trayStop, &QAction::triggered, this, &MainWindow::onStopClicked);
    connect(quitAction, &QAction::triggered, this, &MainWindow::quitApp);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                    if (m_controller->state() == RecordingController::State::Idle)
                        restoreMainWindow();
                    else
                        m_floatingBar->raise();
                }
            });
}

QIcon MainWindow::trayIcon() const
{
    return QIcon(QStringLiteral(":/assets/app.png"));
}

void MainWindow::connectSignals()
{
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(m_openFolderButton, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);
    connect(m_screenCombo, &QComboBox::currentIndexChanged, this, [this]() {
        onScreenSelectionChanged();
        saveSettings();
    });
    connect(m_regionButton, &QPushButton::clicked, this, &MainWindow::onSelectRegionClicked);
    connect(m_clearRegionButton, &QPushButton::clicked, this, &MainWindow::onClearRegionClicked);
    connect(m_modeGroup, &QButtonGroup::idClicked, this, [this](int) {
        onModeChanged();
        saveSettings();
    });
    connect(m_hotkeyCheck, &QCheckBox::toggled, this, [this]() {
        applyHotkeys();
        saveSettings();
    });
    connect(m_startHotkeyEdit, &QKeySequenceEdit::editingFinished, this, [this]() {
        applyHotkeys();
        saveSettings();
    });
    connect(m_stopHotkeyEdit, &QKeySequenceEdit::editingFinished, this, [this]() {
        applyHotkeys();
        saveSettings();
    });
    connect(m_pauseHotkeyEdit, &QKeySequenceEdit::editingFinished, this, [this]() {
        applyHotkeys();
        saveSettings();
    });
    connect(m_pathEdit, &QLineEdit::editingFinished, this, &MainWindow::saveSettings);
    connect(m_resetHotkeysButton, &QPushButton::clicked, this, &MainWindow::resetHotkeys);
    connect(m_backButton, &QToolButton::clicked, this, &MainWindow::showRecorderPage);
    connect(m_summaryButton, &QPushButton::clicked, this, &MainWindow::showSettingsPage);
    connect(m_preview, &PreviewWidget::clicked, this, &MainWindow::onPreviewClicked);
    connect(m_preview, &PreviewWidget::settingsClicked, this, &MainWindow::showSettingsPage);
    connect(m_preview, &PreviewWidget::openFileClicked, this, &MainWindow::onOpenLastFile);
    connect(m_preview, &PreviewWidget::openFolderClicked, this, &MainWindow::onOpenFolderClicked);
    connect(m_micCheck, &QCheckBox::toggled, this, [this]() {
        syncAudioMonitor();
        saveSettings();
    });
    connect(m_systemAudioCheck, &QCheckBox::toggled, this, [this]() {
        syncAudioMonitor();
        saveSettings();
    });
    connect(m_audioMonitor, &AudioLevelMonitor::micLevelChanged, m_micCheck, &AudioChip::setLevel);
    connect(m_audioMonitor, &AudioLevelMonitor::systemLevelChanged, m_systemAudioCheck,
            &AudioChip::setLevel);
    connect(m_countdownCombo, &QComboBox::currentIndexChanged, this, [this]() {
        updateHint();
        updateSummary();
        saveSettings();
    });
    connect(m_resolutionCombo, &QComboBox::currentIndexChanged, this, [this]() {
        updateSummary();
        saveSettings();
    });
    connect(m_fpsCombo, &QComboBox::currentIndexChanged, this, [this]() {
        updateSummary();
        saveSettings();
    });
    connect(m_qualityCombo, &QComboBox::currentIndexChanged, this, [this]() {
        updateSummary();
        saveSettings();
    });

    connect(qGuiApp, &QGuiApplication::screenAdded, this, &MainWindow::refreshScreens);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &MainWindow::refreshScreens);

    connect(m_regionSelector, &RegionSelector::selected, this, [this](const QRect &rect) {
        m_region = rect;
        updateRegionLabel();
        updateSummary();
        saveSettings();
        if (m_startAfterRegion) {
            m_startAfterRegion = false;
            actuallyStart();
        } else {
            restoreMainWindow();
        }
    });
    connect(m_regionSelector, &RegionSelector::cancelled, this, [this]() {
        m_startAfterRegion = false;
        restoreMainWindow();
    });

    connect(m_floatingBar, &FloatingBar::pauseClicked, this, &MainWindow::onPauseClicked);
    connect(m_floatingBar, &FloatingBar::stopClicked, this, &MainWindow::onStopClicked);
    connect(m_floatingBar, &FloatingBar::positionChanged, this, [this](const QPoint &pos) {
        m_settings->setFloatingBarPos(pos);
    });
    connect(m_hotkeys, &HotkeyManager::hotkeyPressed, this, &MainWindow::onHotkey);

    connect(m_overlay, &CountdownOverlay::skipped, this, [this]() {
        m_overlay->dismiss();
        m_controller->skipCountdown();
    });
    connect(m_overlay, &CountdownOverlay::cancelled, this, [this]() {
        m_overlay->dismiss();
        m_controller->cancelCountdown();
    });

    connect(m_controller, &RecordingController::stateChanged, this, [this](RecordingController::State state) {
        updateUi();
        updateFloatingBar();
        syncPreviewTimer();
        syncAudioMonitor();

        if (state == RecordingController::State::Countdown
            || state == RecordingController::State::Recording
            || state == RecordingController::State::Paused) {
            hide();
            placeFloatingBar();
            m_floatingBar->show();
            m_floatingBar->raise();
            if (m_tray)
                m_tray->setToolTip(state == RecordingController::State::Paused
                                       ? tr("ScreenRec · 已暂停")
                                       : tr("ScreenRec · 录制中"));
        } else if (state == RecordingController::State::Idle) {
            m_floatingBar->hide();
            m_overlay->dismiss();
            if (m_tray)
                m_tray->setToolTip(QStringLiteral("ScreenRec"));
            if (m_closeAfterStop) {
                saveSettings();
                qApp->quit();
                return;
            }
            restoreMainWindow();
        }
    });
    connect(m_controller, &RecordingController::durationChanged, this, [this](qint64 ms) {
        m_lastDurationMs = ms;
        m_floatingBar->setDurationText(formatDuration(ms));
    });
    connect(m_controller, &RecordingController::countdownTick, this, [this](int remaining) {
        m_countdownShown = remaining;
        m_overlay->setRemaining(remaining);
        if (remaining <= 0)
            m_overlay->dismiss();
        updateUi();
        updateFloatingBar();
    });
    connect(m_controller, &RecordingController::errorOccurred, this, [this](const QString &message) {
        m_overlay->dismiss();
        m_floatingBar->hide();
        restoreMainWindow();
        statusBar()->showMessage(message, 8000);
        QMessageBox::warning(this, tr("录制出错"), message);
        updateUi();
    });
    connect(m_controller, &RecordingController::warningOccurred, this, [this](const QString &message) {
        statusBar()->showMessage(message, 6000);
        if (m_tray)
            m_tray->showMessage(QStringLiteral("ScreenRec"), message, QSystemTrayIcon::Warning, 3000);
    });
    connect(m_controller, &RecordingController::recordingFinished, this, [this](const QString &path) {
        m_lastSavedPath = path;
        m_settings->setLastSavedPath(path);
        m_settings->setLastDurationMs(m_lastDurationMs);
        const QString fileName = QFileInfo(path).fileName();
        statusBar()->showMessage(tr("已保存  %1").arg(fileName), 8000);
        if (m_tray)
            m_tray->showMessage(QStringLiteral("ScreenRec"),
                                tr("已保存到 %1").arg(QDir::toNativeSeparators(path)),
                                QSystemTrayIcon::Information, 3000);
        restoreMainWindow();
        showLastResult();
    });
}

void MainWindow::loadSettings()
{
    const bool wasLoading = m_loadingSettings;
    m_loadingSettings = true;

    restoreWindowGeometry();

    m_pathEdit->setText(QDir::toNativeSeparators(m_settings->outputDirectory()));
    m_micCheck->setChecked(m_settings->recordMicrophone());
    m_systemAudioCheck->setChecked(m_settings->recordSystemAudio());
    m_region = m_settings->region();
    m_hotkeyCheck->setChecked(m_settings->hotkeysEnabled());
    m_startHotkeyEdit->setKeySequence(m_settings->startHotkey());
    m_stopHotkeyEdit->setKeySequence(m_settings->stopHotkey());
    m_pauseHotkeyEdit->setKeySequence(m_settings->pauseHotkey());
    m_lastSavedPath = m_settings->lastSavedPath();
    m_lastDurationMs = m_settings->lastDurationMs();

    const int countdown = m_settings->countdownSeconds();
    const int countdownIndex = m_countdownCombo->findData(countdown);
    m_countdownCombo->setCurrentIndex(countdownIndex >= 0 ? countdownIndex : 1);

    if (QAbstractButton *modeBtn = m_modeGroup->button(m_settings->captureMode()))
        modeBtn->setChecked(true);
    else
        m_modeScreenBtn->setChecked(true);

    const int resIndex = m_resolutionCombo->findData(m_settings->maxResolution());
    m_resolutionCombo->setCurrentIndex(resIndex >= 0 ? resIndex : 0);
    const int fpsIndex = m_fpsCombo->findData(m_settings->frameRate());
    m_fpsCombo->setCurrentIndex(fpsIndex >= 0 ? fpsIndex : 1);
    const int qualityIndex = m_qualityCombo->findData(m_settings->quality());
    m_qualityCombo->setCurrentIndex(qualityIndex >= 0 ? qualityIndex : 2);

    updateIdleStatus();
    m_loadingSettings = wasLoading;
}

void MainWindow::restoreWindowGeometry()
{
    const QByteArray geometry = m_settings->windowGeometry();
    if (!geometry.isEmpty() && restoreGeometry(geometry)) {
        if (isMinimized())
            setWindowState(windowState() & ~Qt::WindowMinimized);
        return;
    }

    QSize size = m_settings->windowSize();
    if (size.width() >= minimumWidth() && size.height() >= minimumHeight()) {
        if (QScreen *screen = QGuiApplication::primaryScreen()) {
            const QSize avail = screen->availableGeometry().size();
            size.setWidth(qBound(minimumWidth(), size.width(), avail.width()));
            size.setHeight(qBound(minimumHeight(), size.height(), avail.height()));
        }
        resize(size);
    }
    if (m_settings->windowMaximized())
        setWindowState(windowState() | Qt::WindowMaximized);
}

void MainWindow::persistWindowGeometry()
{
    if (!m_geometryRestored || isMinimized())
        return;

    const QSize sz = isMaximized() ? normalGeometry().size() : size();
    if (sz.width() < minimumWidth() || sz.height() < minimumHeight())
        return;

    m_settings->setWindowGeometry(saveGeometry());
    m_settings->setWindowSize(sz);
    m_settings->setWindowMaximized(isMaximized());
}

void MainWindow::refreshScreens()
{
    const QString previous = m_screenCombo->currentData().toString();
    const QString preferred = previous.isEmpty() ? m_settings->lastScreenName() : previous;

    m_screenCombo->clear();
    const QList<QScreen *> screens = QGuiApplication::screens();
    int restore = 0;
    for (int i = 0; i < screens.size(); ++i) {
        QScreen *screen = screens.at(i);
        const QRect g = screen->geometry();
        const qreal dpr = screen->devicePixelRatio();
        const bool primary = screen == QGuiApplication::primaryScreen();
        const QString text = tr("%1 · %2×%3")
                                 .arg(primary ? tr("主屏") : tr("显示器 %1").arg(i + 1))
                                 .arg(qRound(g.width() * dpr))
                                 .arg(qRound(g.height() * dpr));
        m_screenCombo->addItem(text, screen->name());
        if (screen->name() == preferred)
            restore = i;
    }
    if (m_screenCombo->count() > 0)
        m_screenCombo->setCurrentIndex(restore);
    clampRegion();
    updateModeRows();
    updateSummary();
}

void MainWindow::saveSettings()
{
    if (m_loadingSettings)
        return;

    m_settings->setOutputDirectory(m_pathEdit->text().trimmed());
    m_settings->setRecordMicrophone(m_micCheck->isChecked());
    m_settings->setRecordSystemAudio(m_systemAudioCheck->isChecked());
    m_settings->setFrameRate(m_fpsCombo->currentData().toInt());
    m_settings->setQuality(m_qualityCombo->currentData().toInt());
    m_settings->setMaxResolution(m_resolutionCombo->currentData().toSize());
    m_settings->setLastScreenName(m_screenCombo->currentData().toString());
    m_settings->setCountdownSeconds(m_countdownCombo->currentData().toInt());
    m_settings->setCaptureMode(int(currentMode()));
    m_settings->setRegion(m_region);
    m_settings->setHotkeysEnabled(m_hotkeyCheck->isChecked());
    m_settings->setStartHotkey(m_startHotkeyEdit->keySequence());
    m_settings->setStopHotkey(m_stopHotkeyEdit->keySequence());
    m_settings->setPauseHotkey(m_pauseHotkeyEdit->keySequence());
    m_settings->setLastSavedPath(m_lastSavedPath);
    m_settings->setLastDurationMs(m_lastDurationMs);
    persistWindowGeometry();
    m_settings->sync();
}

void MainWindow::updateModeRows()
{
    const bool region = currentMode() == RecordingController::CaptureMode::Region;
    m_screenRow->setVisible(!region && m_screenCombo->count() > 1);
    m_regionRow->setVisible(region);

    const QColor on(QStringLiteral("#E11D2E"));
    const QColor off(QStringLiteral("#1B2838"));
    m_modeScreenBtn->setIcon(AppIcons::screen(m_modeScreenBtn->isChecked() ? on : off));
    m_modeRegionBtn->setIcon(AppIcons::region(m_modeRegionBtn->isChecked() ? on : off));
}

void MainWindow::updateRegionLabel()
{
    const bool hasRegion = m_region.width() >= 16 && m_region.height() >= 16;
    if (hasRegion) {
        m_regionLabel->setText(tr("%1 × %2  ·  (%3, %4)")
                                   .arg(m_region.width())
                                   .arg(m_region.height())
                                   .arg(m_region.x())
                                   .arg(m_region.y()));
        m_regionLabel->setToolTip(QString());
    } else {
        m_regionLabel->setText(tr("未选择区域"));
        m_regionLabel->setToolTip(QString());
    }
    if (m_clearRegionButton)
        m_clearRegionButton->setVisible(hasRegion);
}

void MainWindow::updateHint()
{
    if (!m_hintLabel)
        return;

    if (m_hotkeyCheck->isChecked() && !m_startHotkeyEdit->keySequence().isEmpty()) {
        m_hintLabel->setText(tr("%1 开始")
                                 .arg(m_startHotkeyEdit->keySequence().toString(QKeySequence::NativeText)));
        m_hintLabel->show();
    } else {
        m_hintLabel->clear();
        m_hintLabel->hide();
    }
}

void MainWindow::updateSummary()
{
    if (!m_summaryButton)
        return;

    const QSize source = captureSize();
    const QSize maxRes = m_resolutionCombo->currentData().toSize();
    QString resText;
    if (maxRes.isValid() && maxRes.width() > 0) {
        resText = m_resolutionCombo->currentText();
    } else if (source.isValid() && source.width() > 0) {
        resText = tr("%1×%2").arg(source.width()).arg(source.height());
        m_resolutionCombo->setItemText(0, tr("原始（%1×%2）").arg(source.width()).arg(source.height()));
    } else {
        resText = tr("原始");
        m_resolutionCombo->setItemText(0, tr("原始"));
    }

    QString quality = m_qualityCombo->currentText();
    const int cut = quality.indexOf(QStringLiteral(" · "));
    if (cut > 0)
        quality = quality.left(cut);

    QStringList parts;
    parts << resText;
    parts << m_fpsCombo->currentText();
    parts << quality;
    const int countdown = m_countdownCombo->currentData().toInt();
    if (countdown > 0)
        parts << tr("%1秒倒计时").arg(countdown);
    else
        parts << tr("立即开始");
    m_summaryButton->setText(parts.join(QStringLiteral("  ·  ")));
}

void MainWindow::updateIdleStatus()
{
    if (m_controller->state() != RecordingController::State::Idle)
        return;
    if (!m_lastSavedPath.isEmpty() && QFileInfo::exists(m_lastSavedPath)) {
        statusBar()->showMessage(tr("上次保存  %1").arg(QFileInfo(m_lastSavedPath).fileName()));
    } else {
        statusBar()->showMessage(tr("选择画面后点击开始录制"));
    }
}

QSize MainWindow::captureSize() const
{
    if (currentMode() == RecordingController::CaptureMode::Region
        && m_region.width() >= 16 && m_region.height() >= 16)
        return m_region.size();
    if (QScreen *screen = selectedScreen()) {
        const qreal dpr = screen->devicePixelRatio();
        return QSize(qRound(screen->size().width() * dpr), qRound(screen->size().height() * dpr));
    }
    return {};
}

void MainWindow::updateUi()
{
    const auto state = m_controller->state();
    const bool idle = state == RecordingController::State::Idle;

    m_modeScreenBtn->setEnabled(idle);
    m_modeRegionBtn->setEnabled(idle);
    m_screenCombo->setEnabled(idle);
    m_regionButton->setEnabled(idle);
    if (m_clearRegionButton)
        m_clearRegionButton->setEnabled(idle);
    m_micCheck->setEnabled(idle);
    m_systemAudioCheck->setEnabled(idle);
    m_resolutionCombo->setEnabled(idle);
    m_fpsCombo->setEnabled(idle);
    m_qualityCombo->setEnabled(idle);
    m_pathEdit->setEnabled(idle);
    m_browseButton->setEnabled(idle);
    m_countdownCombo->setEnabled(idle);
    m_hotkeyCheck->setEnabled(idle);
    m_hotkeyRow->setEnabled(idle && m_hotkeyCheck->isChecked());
    m_resetHotkeysButton->setEnabled(idle);
    m_summaryButton->setEnabled(idle);
    m_startButton->setEnabled(idle);

    if (m_trayStart)
        m_trayStart->setVisible(idle);
    if (m_trayPause)
        m_trayPause->setEnabled(state == RecordingController::State::Recording
                                || state == RecordingController::State::Paused);
    if (m_trayStop)
        m_trayStop->setEnabled(!idle);

    switch (state) {
    case RecordingController::State::Idle:
        m_floatingBar->setDurationText(formatDuration(0));
        updateIdleStatus();
        break;
    case RecordingController::State::Countdown:
        statusBar()->showMessage(tr("倒计时 %1").arg(m_countdownShown));
        break;
    case RecordingController::State::Recording:
        statusBar()->showMessage(tr("录制中"));
        break;
    case RecordingController::State::Paused:
        statusBar()->showMessage(tr("已暂停"));
        break;
    case RecordingController::State::Stopping:
        statusBar()->showMessage(tr("正在保存…"));
        break;
    }
}

void MainWindow::updateFloatingBar()
{
    const auto state = m_controller->state();
    m_floatingBar->setRecordingUi(state == RecordingController::State::Paused,
                                  state == RecordingController::State::Countdown);
}

void MainWindow::placeFloatingBar()
{
    QScreen *screen = selectedScreen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    m_floatingBar->adjustSize();
    const QRect sg = screen->geometry();
    const QSize sz = m_floatingBar->size();
    const QPoint saved = m_settings->floatingBarPos();
    const QRect barRect(saved, sz);
    if (saved.x() >= 0 && sg.adjusted(0, 0, -sz.width(), -sz.height()).intersects(barRect)) {
        m_floatingBar->move(QPoint(qBound(sg.left() + 8, saved.x(), sg.right() - sz.width() - 8),
                                   qBound(sg.top() + 8, saved.y(), sg.bottom() - sz.height() - 8)));
        return;
    }

    QPoint pos(sg.center().x() - sz.width() / 2, sg.top() + 16);

    if (currentMode() == RecordingController::CaptureMode::Region && m_region.isValid()) {
        const QRect regionGlobal = m_region.translated(sg.topLeft());
        pos = QPoint(regionGlobal.center().x() - sz.width() / 2, regionGlobal.top() - sz.height() - 8);
        if (pos.y() < sg.top())
            pos.setY(qMin(regionGlobal.bottom() + 8, sg.bottom() - sz.height() - 8));
        if (QRect(pos, sz).intersects(regionGlobal))
            pos = QPoint(sg.left() + 16, sg.top() + 16);
    }

    if (pos.x() < sg.left())
        pos.setX(sg.left() + 8);
    if (pos.x() + sz.width() > sg.right())
        pos.setX(sg.right() - sz.width() - 8);
    m_floatingBar->move(pos);
}

void MainWindow::onStartClicked()
{
    if (m_controller->state() == RecordingController::State::Countdown) {
        m_overlay->dismiss();
        m_controller->skipCountdown();
        return;
    }

    saveSettings();
    m_preview->clearResult();

    if (currentMode() == RecordingController::CaptureMode::Region
        && (m_region.width() < 16 || m_region.height() < 16)) {
        m_startAfterRegion = true;
        beginRegionSelect();
        return;
    }

    actuallyStart();
}

void MainWindow::actuallyStart()
{
    m_audioMonitor->stop();
    const RecordingController::Request request = currentRequest();
    if (request.countdownSeconds > 0) {
        m_countdownShown = request.countdownSeconds;
        m_overlay->showOnScreen(request.screen ? request.screen : QGuiApplication::primaryScreen(),
                                request.countdownSeconds);
    }

    m_controller->start(request);
}

void MainWindow::onPauseClicked()
{
    if (m_controller->state() == RecordingController::State::Paused)
        m_controller->resume();
    else
        m_controller->pause();
}

void MainWindow::onStopClicked()
{
    m_overlay->dismiss();
    m_controller->stop();
}

void MainWindow::onBrowseClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("选择保存目录"), m_pathEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        m_pathEdit->setText(QDir::toNativeSeparators(dir));
        saveSettings();
    }
}

void MainWindow::onOpenFolderClicked()
{
    QString dir = m_pathEdit->text().trimmed();
    if (!m_lastSavedPath.isEmpty()) {
        const QFileInfo info(m_lastSavedPath);
        if (info.exists())
            dir = info.absolutePath();
    }
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void MainWindow::onOpenLastFile()
{
    if (m_lastSavedPath.isEmpty() || !QFileInfo::exists(m_lastSavedPath)) {
        QMessageBox::information(this, tr("找不到文件"), tr("录制文件不存在或已被移动。"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastSavedPath));
}

void MainWindow::onSelectRegionClicked()
{
    m_startAfterRegion = false;
    beginRegionSelect();
}

void MainWindow::onClearRegionClicked()
{
    m_region = QRect();
    updateRegionLabel();
    updatePreview();
    updateSummary();
    saveSettings();
}

void MainWindow::beginRegionSelect()
{
    hide();
    m_regionSelector->start(selectedScreen(), m_region);
}

void MainWindow::onModeChanged()
{
    updateModeRows();
    syncPreviewTimer();
    updatePreview();
    updateSummary();
}

void MainWindow::onScreenSelectionChanged()
{
    clampRegion();
    updatePreview();
    updateSummary();
}

void MainWindow::onPreviewClicked()
{
    if (currentMode() == RecordingController::CaptureMode::Region)
        onSelectRegionClicked();
}

void MainWindow::showSettingsPage()
{
    m_stack->setCurrentWidget(m_settingsPage);
    syncPreviewTimer();
    syncAudioMonitor();
}

void MainWindow::showRecorderPage()
{
    saveSettings();
    updateHint();
    updateSummary();
    m_stack->setCurrentWidget(m_recorderPage);
    syncPreviewTimer();
    syncAudioMonitor();
    updatePreview();
}

void MainWindow::showLastResult()
{
    if (m_lastSavedPath.isEmpty())
        return;
    const QFileInfo info(m_lastSavedPath);
    QStringList meta;
    meta << info.fileName();
    if (m_lastDurationMs > 0)
        meta << formatDuration(m_lastDurationMs);
    if (info.exists())
        meta << formatSize(info.size());
    m_preview->showResult(tr("已保存"), meta.join(QStringLiteral("  ·  ")));
}

void MainWindow::resetHotkeys()
{
    m_startHotkeyEdit->setKeySequence(AppSettings::defaultStartHotkey());
    m_stopHotkeyEdit->setKeySequence(AppSettings::defaultStopHotkey());
    m_pauseHotkeyEdit->setKeySequence(AppSettings::defaultPauseHotkey());
    applyHotkeys();
    saveSettings();
}

void MainWindow::clampRegion()
{
    QScreen *screen = selectedScreen();
    if (!screen)
        return;
    const QRect bounds(QPoint(0, 0), screen->size());
    m_region = m_region.intersected(bounds);
    updateRegionLabel();
}

void MainWindow::syncPreviewTimer()
{
    if (!m_previewTimer)
        return;
    const bool run = isVisible() && !isMinimized()
        && m_controller->state() == RecordingController::State::Idle
        && m_stack->currentWidget() == m_recorderPage;
    if (run) {
        if (!m_previewTimer->isActive())
            m_previewTimer->start();
    } else {
        m_previewTimer->stop();
    }
}

void MainWindow::syncAudioMonitor()
{
    if (!m_audioMonitor)
        return;
    const bool run = isVisible() && !isMinimized()
        && m_controller->state() == RecordingController::State::Idle
        && m_stack->currentWidget() == m_recorderPage;
    m_audioMonitor->setActive(run && m_micCheck->isChecked(),
                              run && m_systemAudioCheck->isChecked());
}

void MainWindow::updatePreview()
{
    if (!m_preview)
        return;
    if (!isVisible() || isMinimized())
        return;
    if (m_controller->state() != RecordingController::State::Idle) {
        m_preview->setPlaceholder(tr("录制中"));
        m_preview->setClickable(false);
        return;
    }

    const bool regionMode = currentMode() == RecordingController::CaptureMode::Region;
    const bool hasRegion = m_region.width() >= 16 && m_region.height() >= 16;
    m_preview->setClickable(regionMode && !m_preview->hasResult());

    if (regionMode && !hasRegion) {
        m_preview->setPlaceholder(tr("选择录制区域"), tr("点击此处拖拽选择"));
        return;
    }

    QScreen *screen = selectedScreen();
    if (!screen) {
        m_preview->setPlaceholder(tr("没有可用显示器"));
        m_preview->setClickable(false);
        return;
    }

    QPixmap grab;
    if (regionMode)
        grab = screen->grabWindow(0, m_region.x(), m_region.y(), m_region.width(), m_region.height());
    else
        grab = screen->grabWindow(0);

    if (grab.isNull()) {
        m_preview->setPlaceholder(tr("无法抓取预览"));
        return;
    }

    if (regionMode) {
        const qreal dpr = screen->devicePixelRatio();
        const QSize physicalScreen(qRound(screen->size().width() * dpr),
                                   qRound(screen->size().height() * dpr));
        const bool looksLikeFullScreen =
            grab.size() == screen->size() || grab.size() == physicalScreen;
        if (looksLikeFullScreen) {
            QRect crop = m_region;
            if (grab.size() != screen->size()) {
                crop = QRect(qRound(m_region.x() * dpr), qRound(m_region.y() * dpr),
                             qRound(m_region.width() * dpr), qRound(m_region.height() * dpr));
            }
            crop = crop.intersected(grab.rect());
            if (crop.width() >= 8 && crop.height() >= 8)
                grab = grab.copy(crop);
        }
        m_preview->setFrame(grab);
        m_preview->setBadge(tr("%1 × %2").arg(m_region.width()).arg(m_region.height()));
        return;
    }

    m_preview->setFrame(grab);
    const QSize px = captureSize();
    m_preview->setBadge(tr("%1 × %2").arg(px.width()).arg(px.height()));
}

void MainWindow::onHotkey(int id)
{
    const auto state = m_controller->state();
    switch (id) {
    case HotkeyManager::Start:
        if (state == RecordingController::State::Idle
            || state == RecordingController::State::Countdown)
            onStartClicked();
        break;
    case HotkeyManager::Stop:
        if (state != RecordingController::State::Idle)
            onStopClicked();
        break;
    case HotkeyManager::Pause:
        if (state == RecordingController::State::Recording
            || state == RecordingController::State::Paused)
            onPauseClicked();
        break;
    default:
        break;
    }
}

void MainWindow::applyHotkeys()
{
    updateHint();
    if (m_hotkeyRow && m_hotkeyCheck) {
        const bool idle = m_controller->state() == RecordingController::State::Idle;
        m_hotkeyRow->setEnabled(idle && m_hotkeyCheck->isChecked());
    }
    if (!m_hotkeysReady)
        return;

    QString error;
    const bool ok = m_hotkeys->setEnabled(m_hotkeyCheck->isChecked(),
                                          m_startHotkeyEdit->keySequence(),
                                          m_stopHotkeyEdit->keySequence(),
                                          m_pauseHotkeyEdit->keySequence(), &error);
    if (m_hotkeyError) {
        if (!ok && m_hotkeyCheck->isChecked()) {
            m_hotkeyError->setText(error);
            m_hotkeyError->show();
            statusBar()->showMessage(error, 6000);
            if (m_tray)
                m_tray->showMessage(QStringLiteral("ScreenRec"), error, QSystemTrayIcon::Warning, 4000);
        } else {
            m_hotkeyError->clear();
            m_hotkeyError->hide();
        }
    }
}

void MainWindow::restoreMainWindow()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::quitApp()
{
    const auto state = m_controller->state();
    if (state != RecordingController::State::Idle) {
        m_closeAfterStop = true;
        m_overlay->dismiss();
        m_controller->stop();
        return;
    }
    saveSettings();
    qApp->quit();
}

QScreen *MainWindow::selectedScreen() const
{
    const QString name = m_screenCombo->currentData().toString();
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen->name() == name)
            return screen;
    }
    return QGuiApplication::primaryScreen();
}

RecordingController::CaptureMode MainWindow::currentMode() const
{
    const int id = m_modeGroup ? m_modeGroup->checkedId() : 0;
    if (id < 0)
        return RecordingController::CaptureMode::Screen;
    return RecordingController::CaptureMode(id);
}

RecordingController::Request MainWindow::currentRequest() const
{
    RecordingController::Request request;
    request.mode = currentMode();
    request.screen = selectedScreen();
    request.region = m_region;
    request.outputDirectory = m_pathEdit->text().trimmed();
    request.recordMicrophone = m_micCheck->isChecked();
    request.recordSystemAudio = m_systemAudioCheck->isChecked();
    request.countdownSeconds = m_countdownCombo->currentData().toInt();
    request.frameRate = m_fpsCombo->currentData().toInt();
    request.quality = m_qualityCombo->currentData().toInt();
    request.maxResolution = m_resolutionCombo->currentData().toSize();
    return request;
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (!m_geometryRestored) {
        restoreWindowGeometry();
        m_geometryRestored = true;
    }
    excludeWidgetFromCapture(this);
    if (!m_hotkeysReady) {
        m_hotkeys->setNativeHandle(quintptr(winId()));
        m_hotkeysReady = true;
        applyHotkeys();
    }
    syncPreviewTimer();
    syncAudioMonitor();
    updatePreview();
}

void MainWindow::hideEvent(QHideEvent *event)
{
    persistWindowGeometry();
    QMainWindow::hideEvent(event);
    syncPreviewTimer();
    syncAudioMonitor();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        syncPreviewTimer();
        syncAudioMonitor();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    const auto state = m_controller->state();
    if (state == RecordingController::State::Idle) {
        saveSettings();
        m_overlay->dismiss();
        m_regionSelector->dismiss();
        m_floatingBar->hide();
        m_audioMonitor->stop();
        event->accept();
        return;
    }

    if (state == RecordingController::State::Stopping) {
        m_closeAfterStop = true;
        event->ignore();
        return;
    }

    const auto answer = QMessageBox::question(
        this, tr("正在录制"), tr("正在录制或倒计时中，确定要停止并退出吗？"));
    if (answer != QMessageBox::Yes) {
        event->ignore();
        return;
    }

    m_closeAfterStop = true;
    m_overlay->dismiss();
    m_controller->stop();
    event->ignore();
}
