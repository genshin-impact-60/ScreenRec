#include "mainwindow.h"
#include "captureexclude.h"
#include "countdownoverlay.h"
#include "floatingbar.h"
#include "hotkeymanager.h"
#include "regionselector.h"
#include "settings.h"

#include <QAction>
#include <QApplication>
#include <QCapturableWindow>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QEvent>
#include <QHideEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QSize>
#include <QShowEvent>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindowCapture>
#include <QMediaRecorder>

namespace {

QString formatDuration(qint64 milliseconds)
{
    const qint64 total = qMax(qint64(0), milliseconds) / 1000;
    const int hours = int(total / 3600);
    const int minutes = int((total % 3600) / 60);
    const int seconds = int(total % 60);
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
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
{
    setWindowIcon(QIcon(QStringLiteral(":/assets/app.png")));
    setupUi();
    setupTray();
    connectSignals();
    refreshScreens();
    loadSettings();
    refreshWindows();
    updateModeRows();
    updateRegionLabel();
    updateUi();

    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(800);
    connect(m_previewTimer, &QTimer::timeout, this, &MainWindow::updatePreview);
    syncPreviewTimer();
}

MainWindow::~MainWindow()
{
    delete m_overlay;
    delete m_regionSelector;
    delete m_floatingBar;
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("ScreenRec  Ver %1").arg(QApplication::applicationVersion()));
    resize(580, 620);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *formBox = new QGroupBox(tr("录制设置"), central);
    m_form = new QFormLayout(formBox);
    m_form->setContentsMargins(12, 12, 12, 12);
    m_form->setSpacing(8);

    m_modeCombo = new QComboBox(formBox);
    m_modeCombo->addItem(tr("整屏"), int(RecordingController::CaptureMode::Screen));
    m_modeCombo->addItem(tr("区域"), int(RecordingController::CaptureMode::Region));
    m_modeCombo->addItem(tr("窗口"), int(RecordingController::CaptureMode::Window));
    m_form->addRow(tr("模式"), m_modeCombo);

    m_screenCombo = new QComboBox(formBox);
    m_form->addRow(tr("显示器"), m_screenCombo);

    m_regionRow = new QWidget(formBox);
    auto *regionLayout = new QHBoxLayout(m_regionRow);
    regionLayout->setContentsMargins(0, 0, 0, 0);
    regionLayout->setSpacing(6);
    m_regionLabel = new QLabel(m_regionRow);
    m_regionButton = new QPushButton(tr("选择区域"), m_regionRow);
    regionLayout->addWidget(m_regionLabel, 1);
    regionLayout->addWidget(m_regionButton);
    m_form->addRow(tr("区域"), m_regionRow);

    m_windowRow = new QWidget(formBox);
    auto *windowLayout = new QHBoxLayout(m_windowRow);
    windowLayout->setContentsMargins(0, 0, 0, 0);
    windowLayout->setSpacing(6);
    m_windowCombo = new QComboBox(m_windowRow);
    m_windowCombo->setMinimumContentsLength(24);
    m_refreshWindowsButton = new QPushButton(tr("刷新"), m_windowRow);
    windowLayout->addWidget(m_windowCombo, 1);
    windowLayout->addWidget(m_refreshWindowsButton);
    m_form->addRow(tr("窗口"), m_windowRow);

    auto *audioRow = new QWidget(formBox);
    auto *audioLayout = new QHBoxLayout(audioRow);
    audioLayout->setContentsMargins(0, 0, 0, 0);
    audioLayout->setSpacing(12);
    m_micCheck = new QCheckBox(tr("麦克风"), audioRow);
    m_systemAudioCheck = new QCheckBox(tr("系统声音"), audioRow);
    audioLayout->addWidget(m_micCheck);
    audioLayout->addWidget(m_systemAudioCheck);
    audioLayout->addStretch(1);
    m_form->addRow(tr("音频"), audioRow);

    auto *videoRow = new QWidget(formBox);
    auto *videoLayout = new QHBoxLayout(videoRow);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(6);
    m_resolutionCombo = new QComboBox(videoRow);
    m_resolutionCombo->addItem(tr("原始"), QSize());
    m_resolutionCombo->addItem(QStringLiteral("1080p"), QSize(1920, 1080));
    m_resolutionCombo->addItem(QStringLiteral("720p"), QSize(1280, 720));
    m_fpsCombo = new QComboBox(videoRow);
    m_fpsCombo->addItem(QStringLiteral("24 fps"), 24);
    m_fpsCombo->addItem(QStringLiteral("30 fps"), 30);
    m_fpsCombo->addItem(QStringLiteral("60 fps"), 60);
    m_qualityCombo = new QComboBox(videoRow);
    m_qualityCombo->addItem(tr("低"), int(QMediaRecorder::LowQuality));
    m_qualityCombo->addItem(tr("中"), int(QMediaRecorder::NormalQuality));
    m_qualityCombo->addItem(tr("高"), int(QMediaRecorder::HighQuality));
    m_qualityCombo->addItem(tr("很高"), int(QMediaRecorder::VeryHighQuality));
    videoLayout->addWidget(m_resolutionCombo, 1);
    videoLayout->addWidget(m_fpsCombo);
    videoLayout->addWidget(m_qualityCombo);
    m_form->addRow(tr("画面"), videoRow);

    auto *pathRow = new QWidget(formBox);
    auto *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(6);
    m_pathEdit = new QLineEdit(pathRow);
    m_browseButton = new QPushButton(tr("浏览"), pathRow);
    m_openFolderButton = new QPushButton(tr("打开目录"), pathRow);
    pathLayout->addWidget(m_pathEdit, 1);
    pathLayout->addWidget(m_browseButton);
    pathLayout->addWidget(m_openFolderButton);
    m_form->addRow(tr("保存到"), pathRow);

    m_countdownCombo = new QComboBox(formBox);
    m_countdownCombo->addItem(tr("立即开始"), 0);
    m_countdownCombo->addItem(tr("3 秒"), 3);
    m_countdownCombo->addItem(tr("5 秒"), 5);
    m_form->addRow(tr("倒计时"), m_countdownCombo);

    m_hotkeyCheck = new QCheckBox(tr("启用全局热键"), formBox);
    m_form->addRow(tr("热键"), m_hotkeyCheck);

    m_hotkeyRow = new QWidget(formBox);
    auto *hotkeyLayout = new QHBoxLayout(m_hotkeyRow);
    hotkeyLayout->setContentsMargins(0, 0, 0, 0);
    hotkeyLayout->setSpacing(6);
    m_startHotkeyEdit = new QKeySequenceEdit(m_hotkeyRow);
    m_stopHotkeyEdit = new QKeySequenceEdit(m_hotkeyRow);
    m_pauseHotkeyEdit = new QKeySequenceEdit(m_hotkeyRow);
    m_startHotkeyEdit->setMaximumSequenceLength(1);
    m_stopHotkeyEdit->setMaximumSequenceLength(1);
    m_pauseHotkeyEdit->setMaximumSequenceLength(1);
    hotkeyLayout->addWidget(new QLabel(tr("开始"), m_hotkeyRow));
    hotkeyLayout->addWidget(m_startHotkeyEdit, 1);
    hotkeyLayout->addWidget(new QLabel(tr("停止"), m_hotkeyRow));
    hotkeyLayout->addWidget(m_stopHotkeyEdit, 1);
    hotkeyLayout->addWidget(new QLabel(tr("暂停"), m_hotkeyRow));
    hotkeyLayout->addWidget(m_pauseHotkeyEdit, 1);
    m_form->addRow(QString(), m_hotkeyRow);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    m_startButton = new QPushButton(tr("开始录制"), central);
    m_pauseButton = new QPushButton(tr("暂停"), central);
    m_stopButton = new QPushButton(tr("停止"), central);
    m_startButton->setMinimumHeight(36);
    m_pauseButton->setMinimumHeight(36);
    m_stopButton->setMinimumHeight(36);
    m_startButton->setDefault(true);
    buttons->addWidget(m_startButton, 1);
    buttons->addWidget(m_pauseButton, 1);
    buttons->addWidget(m_stopButton, 1);

    auto *statusRow = new QHBoxLayout;
    m_statusLabel = new QLabel(tr("就绪"), central);
    m_durationLabel = new QLabel(QStringLiteral("00:00:00"), central);
    QFont durationFont = m_durationLabel->font();
    durationFont.setPointSize(durationFont.pointSize() + 4);
    durationFont.setBold(true);
    m_durationLabel->setFont(durationFont);
    m_durationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusRow->addWidget(m_statusLabel, 1);
    statusRow->addWidget(m_durationLabel);

    auto *fileRow = new QWidget(central);
    auto *fileLayout = new QHBoxLayout(fileRow);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    m_fileLabel = new QLabel(fileRow);
    m_fileLabel->setWordWrap(true);
    m_fileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_fileLabel->setForegroundRole(QPalette::PlaceholderText);
    m_openFileButton = new QPushButton(tr("打开文件"), fileRow);
    m_openFileButton->setEnabled(false);
    fileLayout->addWidget(m_fileLabel, 1);
    fileLayout->addWidget(m_openFileButton);

    auto *previewBox = new QGroupBox(tr("预览"), central);
    auto *previewLayout = new QVBoxLayout(previewBox);
    previewLayout->setContentsMargins(8, 8, 8, 8);
    m_previewLabel = new QLabel(previewBox);
    m_previewLabel->setMinimumHeight(160);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet(QStringLiteral("background: #111; color: #aaa;"));
    m_previewLabel->setText(tr("空闲时每秒刷新一次缩略图，录制中关闭以免套娃。"));
    previewLayout->addWidget(m_previewLabel);

    auto *hint = new QLabel(
        tr("录制中主窗口会隐藏。系统声音走 WASAPI 内录。悬浮条仍可能出现在整屏/窗口录像里。"),
        central);
    hint->setWordWrap(true);
    hint->setForegroundRole(QPalette::PlaceholderText);

    root->addWidget(formBox);
    root->addLayout(buttons);
    root->addLayout(statusRow);
    root->addWidget(fileRow);
    root->addWidget(previewBox, 1);
    root->addWidget(hint);

    setCentralWidget(central);
    statusBar()->showMessage(tr("选择模式后点击开始录制"));
}

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(trayIcon(), this);
    m_tray->setToolTip(QStringLiteral("ScreenRec"));

    auto *menu = new QMenu(this);
    auto *showAction = menu->addAction(tr("显示主窗口"));
    auto *pauseAction = menu->addAction(tr("暂停 / 继续"));
    auto *stopAction = menu->addAction(tr("停止"));
    menu->addSeparator();
    auto *quitAction = menu->addAction(tr("退出"));
    m_tray->setContextMenu(menu);
    m_tray->show();

    connect(showAction, &QAction::triggered, this, &MainWindow::restoreMainWindow);
    connect(pauseAction, &QAction::triggered, this, &MainWindow::onPauseClicked);
    connect(stopAction, &QAction::triggered, this, &MainWindow::onStopClicked);
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
    connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(m_openFolderButton, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);
    connect(m_openFileButton, &QPushButton::clicked, this, &MainWindow::onOpenFileClicked);
    connect(m_screenCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onScreenSelectionChanged);
    connect(m_regionButton, &QPushButton::clicked, this, &MainWindow::onSelectRegionClicked);
    connect(m_refreshWindowsButton, &QPushButton::clicked, this, &MainWindow::refreshWindows);
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onModeChanged);
    connect(m_hotkeyCheck, &QCheckBox::toggled, this, &MainWindow::applyHotkeys);
    connect(m_startHotkeyEdit, &QKeySequenceEdit::editingFinished, this, &MainWindow::applyHotkeys);
    connect(m_stopHotkeyEdit, &QKeySequenceEdit::editingFinished, this, &MainWindow::applyHotkeys);
    connect(m_pauseHotkeyEdit, &QKeySequenceEdit::editingFinished, this, &MainWindow::applyHotkeys);

    connect(qGuiApp, &QGuiApplication::screenAdded, this, &MainWindow::refreshScreens);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &MainWindow::refreshScreens);

    connect(m_regionSelector, &RegionSelector::selected, this, [this](const QRect &rect) {
        m_region = rect;
        updateRegionLabel();
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
    connect(m_hotkeys, &HotkeyManager::hotkeyPressed, this, &MainWindow::onHotkey);

    connect(m_controller, &RecordingController::stateChanged, this, [this](RecordingController::State state) {
        updateUi();
        updateFloatingBar();
        syncPreviewTimer();

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
        const QString text = formatDuration(ms);
        m_durationLabel->setText(text);
        m_floatingBar->setDurationText(text);
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
        m_lastFile = path;
        m_openFileButton->setEnabled(true);
        m_fileLabel->setText(tr("已保存：%1").arg(QDir::toNativeSeparators(path)));
        statusBar()->showMessage(tr("录制完成"), 5000);
        if (m_tray)
            m_tray->showMessage(QStringLiteral("ScreenRec"),
                                tr("已保存到 %1").arg(QDir::toNativeSeparators(path)),
                                QSystemTrayIcon::Information, 3000);
        restoreMainWindow();
        if (m_closeAfterStop)
            return;

        QMessageBox box(this);
        box.setWindowTitle(tr("录制完成"));
        box.setText(tr("已保存到：\n%1").arg(QDir::toNativeSeparators(path)));
        auto *openFile = box.addButton(tr("打开文件"), QMessageBox::AcceptRole);
        auto *openDir = box.addButton(tr("打开目录"), QMessageBox::ActionRole);
        box.addButton(tr("关闭"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == openFile)
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        else if (box.clickedButton() == openDir)
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    });
}

void MainWindow::loadSettings()
{
    m_pathEdit->setText(QDir::toNativeSeparators(m_settings->outputDirectory()));
    m_micCheck->setChecked(m_settings->recordMicrophone());
    m_systemAudioCheck->setChecked(m_settings->recordSystemAudio());
    m_region = m_settings->region();
    m_hotkeyCheck->setChecked(m_settings->hotkeysEnabled());
    m_startHotkeyEdit->setKeySequence(m_settings->startHotkey());
    m_stopHotkeyEdit->setKeySequence(m_settings->stopHotkey());
    m_pauseHotkeyEdit->setKeySequence(m_settings->pauseHotkey());

    const int countdown = m_settings->countdownSeconds();
    const int countdownIndex = m_countdownCombo->findData(countdown);
    m_countdownCombo->setCurrentIndex(countdownIndex >= 0 ? countdownIndex : 1);

    const int modeIndex = m_modeCombo->findData(m_settings->captureMode());
    if (modeIndex >= 0)
        m_modeCombo->setCurrentIndex(modeIndex);

    const int resIndex = m_resolutionCombo->findData(m_settings->maxResolution());
    m_resolutionCombo->setCurrentIndex(resIndex >= 0 ? resIndex : 0);
    const int fpsIndex = m_fpsCombo->findData(m_settings->frameRate());
    m_fpsCombo->setCurrentIndex(fpsIndex >= 0 ? fpsIndex : 1);
    const int qualityIndex = m_qualityCombo->findData(m_settings->quality());
    m_qualityCombo->setCurrentIndex(qualityIndex >= 0 ? qualityIndex : 2);
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
        const QString name = screen->name().isEmpty() ? tr("显示器 %1").arg(i + 1) : screen->name();
        const QString text = tr("%1 (%2×%3)%4")
                                 .arg(name)
                                 .arg(qRound(g.width() * dpr))
                                 .arg(qRound(g.height() * dpr))
                                 .arg(screen == QGuiApplication::primaryScreen() ? tr(" · 主屏")
                                                                                 : QString());
        m_screenCombo->addItem(text, screen->name());
        if (screen->name() == preferred)
            restore = i;
    }
    if (m_screenCombo->count() > 0)
        m_screenCombo->setCurrentIndex(restore);
    clampRegion();
}

void MainWindow::refreshWindows()
{
    const QString previous = m_windowCombo->currentText();
    const QString preferred = previous.isEmpty() ? m_settings->lastWindowDescription() : previous;
    const QString selfTitle = windowTitle();

    m_windowCombo->clear();
    const QList<QCapturableWindow> windows = QWindowCapture::capturableWindows();
    int restore = 0;
    for (const QCapturableWindow &window : windows) {
        if (!window.isValid())
            continue;
        const QString desc = window.description();
        if (desc == selfTitle)
            continue;
        const QString text = desc.isEmpty() ? tr("未命名窗口") : desc;
        m_windowCombo->addItem(text, QVariant::fromValue(window));
        if (text == preferred)
            restore = m_windowCombo->count() - 1;
    }
    if (m_windowCombo->count() > 0)
        m_windowCombo->setCurrentIndex(restore);
}

void MainWindow::saveSettings()
{
    m_settings->setOutputDirectory(m_pathEdit->text().trimmed());
    m_settings->setRecordMicrophone(m_micCheck->isChecked());
    m_settings->setRecordSystemAudio(m_systemAudioCheck->isChecked());
    m_settings->setFrameRate(m_fpsCombo->currentData().toInt());
    m_settings->setQuality(m_qualityCombo->currentData().toInt());
    m_settings->setMaxResolution(m_resolutionCombo->currentData().toSize());
    m_settings->setLastScreenName(m_screenCombo->currentData().toString());
    m_settings->setCountdownSeconds(m_countdownCombo->currentData().toInt());
    m_settings->setCaptureMode(m_modeCombo->currentData().toInt());
    m_settings->setRegion(m_region);
    m_settings->setLastWindowDescription(m_windowCombo->currentText());
    m_settings->setHotkeysEnabled(m_hotkeyCheck->isChecked());
    m_settings->setStartHotkey(m_startHotkeyEdit->keySequence());
    m_settings->setStopHotkey(m_stopHotkeyEdit->keySequence());
    m_settings->setPauseHotkey(m_pauseHotkeyEdit->keySequence());
}

void MainWindow::updateModeRows()
{
    const auto mode = currentMode();
    const bool region = mode == RecordingController::CaptureMode::Region;
    const bool window = mode == RecordingController::CaptureMode::Window;

    m_regionRow->setVisible(region);
    if (QWidget *label = m_form->labelForField(m_regionRow))
        label->setVisible(region);

    m_windowRow->setVisible(window);
    if (QWidget *label = m_form->labelForField(m_windowRow))
        label->setVisible(window);

    m_screenCombo->setVisible(!window);
    if (QWidget *label = m_form->labelForField(m_screenCombo))
        label->setVisible(!window);
}

void MainWindow::updateRegionLabel()
{
    if (m_region.width() >= 16 && m_region.height() >= 16) {
        m_regionLabel->setText(tr("%1, %2  %3×%4")
                                   .arg(m_region.x())
                                   .arg(m_region.y())
                                   .arg(m_region.width())
                                   .arg(m_region.height()));
    } else {
        m_regionLabel->setText(tr("未选择"));
    }
}

void MainWindow::updateUi()
{
    const auto state = m_controller->state();
    const bool idle = state == RecordingController::State::Idle;
    const bool countdown = state == RecordingController::State::Countdown;
    const bool recording = state == RecordingController::State::Recording;
    const bool paused = state == RecordingController::State::Paused;

    m_modeCombo->setEnabled(idle);
    m_screenCombo->setEnabled(idle);
    m_windowCombo->setEnabled(idle);
    m_refreshWindowsButton->setEnabled(idle);
    m_regionButton->setEnabled(idle);
    m_micCheck->setEnabled(idle);
    m_systemAudioCheck->setEnabled(idle);
    m_resolutionCombo->setEnabled(idle);
    m_fpsCombo->setEnabled(idle);
    m_qualityCombo->setEnabled(idle);
    m_pathEdit->setEnabled(idle);
    m_browseButton->setEnabled(idle);
    m_countdownCombo->setEnabled(idle);
    m_hotkeyCheck->setEnabled(idle);
    m_hotkeyRow->setEnabled(idle);

    m_startButton->setEnabled(idle || countdown);
    m_startButton->setText(countdown ? tr("取消倒计时") : tr("开始录制"));
    m_pauseButton->setEnabled(recording || paused);
    m_pauseButton->setText(paused ? tr("继续") : tr("暂停"));
    m_stopButton->setEnabled(recording || paused);

    switch (state) {
    case RecordingController::State::Idle:
        m_statusLabel->setText(tr("就绪"));
        m_durationLabel->setText(formatDuration(0));
        m_floatingBar->setDurationText(formatDuration(0));
        break;
    case RecordingController::State::Countdown:
        m_statusLabel->setText(tr("倒计时 %1").arg(m_countdownShown));
        break;
    case RecordingController::State::Recording:
        m_statusLabel->setText(tr("录制中"));
        break;
    case RecordingController::State::Paused:
        m_statusLabel->setText(tr("已暂停"));
        break;
    case RecordingController::State::Stopping:
        m_statusLabel->setText(tr("正在保存…"));
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
        m_controller->cancelCountdown();
        return;
    }

    saveSettings();

    if (currentMode() == RecordingController::CaptureMode::Region
        && (m_region.width() < 16 || m_region.height() < 16)) {
        m_startAfterRegion = true;
        beginRegionSelect();
        return;
    }

    if (currentMode() == RecordingController::CaptureMode::Window)
        refreshWindows();

    actuallyStart();
}

void MainWindow::actuallyStart()
{
    const RecordingController::Request request = currentRequest();
    if (request.countdownSeconds > 0) {
        m_countdownShown = request.countdownSeconds;
        m_overlay->showOnScreen(request.screen ? request.screen : QGuiApplication::primaryScreen(),
                                request.countdownSeconds);
    }

    m_fileLabel->clear();
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
    if (!dir.isEmpty())
        m_pathEdit->setText(QDir::toNativeSeparators(dir));
}

void MainWindow::onOpenFolderClicked()
{
    const QString dir = m_pathEdit->text().trimmed();
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void MainWindow::onSelectRegionClicked()
{
    m_startAfterRegion = false;
    beginRegionSelect();
}

void MainWindow::beginRegionSelect()
{
    hide();
    m_regionSelector->start(selectedScreen());
}

void MainWindow::onModeChanged()
{
    updateModeRows();
    if (currentMode() == RecordingController::CaptureMode::Window)
        refreshWindows();
    syncPreviewTimer();
    updatePreview();
}

void MainWindow::onScreenSelectionChanged()
{
    clampRegion();
    updatePreview();
}

void MainWindow::onOpenFileClicked()
{
    if (m_lastFile.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastFile));
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
        && currentMode() != RecordingController::CaptureMode::Window;
    if (run) {
        if (!m_previewTimer->isActive())
            m_previewTimer->start();
    } else {
        m_previewTimer->stop();
    }
}

void MainWindow::updatePreview()
{
    if (!m_previewLabel)
        return;
    if (!isVisible() || isMinimized())
        return;
    if (m_controller->state() != RecordingController::State::Idle) {
        m_previewLabel->setPixmap({});
        m_previewLabel->setText(tr("录制中"));
        return;
    }
    if (currentMode() == RecordingController::CaptureMode::Window) {
        m_previewLabel->setPixmap({});
        m_previewLabel->setText(tr("窗口模式不抓取实时预览"));
        return;
    }

    QScreen *screen = selectedScreen();
    if (!screen) {
        m_previewLabel->setText(tr("没有可用显示器"));
        return;
    }

    const bool region = currentMode() == RecordingController::CaptureMode::Region
        && m_region.width() >= 16 && m_region.height() >= 16;

    QPixmap grab;
    if (region)
        grab = screen->grabWindow(0, m_region.x(), m_region.y(), m_region.width(), m_region.height());
    else
        grab = screen->grabWindow(0);

    if (grab.isNull()) {
        m_previewLabel->setText(tr("无法抓取预览"));
        return;
    }

    if (region) {
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
    }

    m_previewLabel->setPixmap(grab.scaled(m_previewLabel->size(), Qt::KeepAspectRatio,
                                          Qt::FastTransformation));
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
    if (!m_hotkeysReady)
        return;

    QString error;
    const bool ok = m_hotkeys->setEnabled(m_hotkeyCheck->isChecked(),
                                          m_startHotkeyEdit->keySequence(),
                                          m_stopHotkeyEdit->keySequence(),
                                          m_pauseHotkeyEdit->keySequence(), &error);
    if (!ok && m_hotkeyCheck->isChecked()) {
        statusBar()->showMessage(error, 6000);
        if (m_tray)
            m_tray->showMessage(QStringLiteral("ScreenRec"), error, QSystemTrayIcon::Warning, 4000);
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
    return RecordingController::CaptureMode(m_modeCombo->currentData().toInt());
}

RecordingController::Request MainWindow::currentRequest() const
{
    RecordingController::Request request;
    request.mode = currentMode();
    request.screen = selectedScreen();
    request.region = m_region;
    request.window = m_windowCombo->currentData().value<QCapturableWindow>();
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
    excludeWidgetFromCapture(this);
    if (!m_hotkeysReady) {
        m_hotkeys->setNativeHandle(quintptr(winId()));
        m_hotkeysReady = true;
        applyHotkeys();
    }
    syncPreviewTimer();
    updatePreview();
}

void MainWindow::hideEvent(QHideEvent *event)
{
    QMainWindow::hideEvent(event);
    syncPreviewTimer();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
        syncPreviewTimer();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    const auto state = m_controller->state();
    if (state == RecordingController::State::Idle) {
        saveSettings();
        m_overlay->dismiss();
        m_regionSelector->dismiss();
        m_floatingBar->hide();
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
