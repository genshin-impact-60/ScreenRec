#include "mainwindow.h"
#include "appstyle.h"
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
#include <QCapturableWindow>
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
#include <QMouseEvent>
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
#include <QWindowCapture>
#include <QMediaRecorder>

#include <functional>

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

void polishWidget(QWidget *widget)
{
    if (!widget || !widget->style())
        return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

class SettingsFold : public QWidget
{
public:
    explicit SettingsFold(const QString &title, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("settingsCard"));
        setAttribute(Qt::WA_StyledBackground, true);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        m_header = new QWidget(this);
        m_header->setObjectName(QStringLiteral("foldHeader"));
        m_header->setAttribute(Qt::WA_StyledBackground, true);
        m_header->setAttribute(Qt::WA_Hover, true);
        m_header->setCursor(Qt::PointingHandCursor);
        m_header->setToolTip(tr("点击展开或折叠"));
        auto *headerLayout = new QHBoxLayout(m_header);
        headerLayout->setContentsMargins(14, 12, 14, 12);
        headerLayout->setSpacing(6);
        auto *titleLabel = new QLabel(title, m_header);
        titleLabel->setObjectName(QStringLiteral("cardTitle"));
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        m_arrow = new QLabel(m_header);
        m_arrow->setFixedSize(18, 18);
        m_arrow->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        headerLayout->addWidget(titleLabel, 0, Qt::AlignVCenter);
        headerLayout->addWidget(m_arrow, 0, Qt::AlignVCenter);
        headerLayout->addStretch(1);

        m_body = new QWidget(this);
        m_bodyLayout = new QVBoxLayout(m_body);
        m_bodyLayout->setContentsMargins(14, 2, 14, 14);
        m_bodyLayout->setSpacing(10);

        root->addWidget(m_header);
        root->addWidget(m_body);
        m_header->installEventFilter(this);
        setExpanded(true);
    }

    QVBoxLayout *bodyLayout() { return m_bodyLayout; }

    bool isExpanded() const { return m_expanded; }

    void setExpanded(bool expanded)
    {
        m_expanded = expanded;
        m_body->setVisible(expanded);
        const QIcon icon = expanded ? AppIcons::chevronDown() : AppIcons::chevronRight();
        m_arrow->setPixmap(icon.pixmap(18, 18));
    }

    void onToggled(std::function<void(bool)> callback) { m_onToggled = std::move(callback); }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_header && event->type() == QEvent::MouseButtonRelease) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                setExpanded(!m_expanded);
                if (m_onToggled)
                    m_onToggled(m_expanded);
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    QWidget *m_header = nullptr;
    QLabel *m_arrow = nullptr;
    QWidget *m_body = nullptr;
    QVBoxLayout *m_bodyLayout = nullptr;
    bool m_expanded = true;
    std::function<void(bool)> m_onToggled;
};

void bindFold(SettingsFold *fold, AppSettings *settings, const QString &id)
{
    fold->setExpanded(settings->sectionExpanded(id, true));
    fold->onToggled([settings, id](bool expanded) {
        settings->setSectionExpanded(id, expanded);
    });
}

void addSettingsField(QVBoxLayout *layout, QWidget *parent, const QString &text, QWidget *field)
{
    auto *row = new QWidget(parent);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(12);
    auto *label = new QLabel(text, row);
    label->setObjectName(QStringLiteral("fieldLabel"));
    label->setFixedWidth(56);
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
    updateHint();
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
    setWindowTitle(QStringLiteral("ScreenRec"));
    resize(460, 640);
    setMinimumSize(400, 560);

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

    auto *header = new QHBoxLayout;
    header->setSpacing(8);
    m_settingsButton = new QToolButton(m_recorderPage);
    m_settingsButton->setObjectName(QStringLiteral("iconButton"));
    m_settingsButton->setIcon(AppIcons::settings());
    m_settingsButton->setIconSize(QSize(20, 20));
    m_settingsButton->setCursor(Qt::PointingHandCursor);
    m_settingsButton->setToolTip(tr("设置"));
    header->addStretch(1);
    header->addWidget(m_settingsButton);
    layout->addLayout(header);

    m_preview = new PreviewWidget(m_recorderPage);
    m_preview->setPlaceholder(tr("预览"), tr("选择模式后将显示画面"));
    layout->addWidget(m_preview, 1);

    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);
    auto makeSeg = [this](const QString &text, const QString &pos, int id) {
        auto *button = new QToolButton(m_recorderPage);
        button->setText(text);
        button->setCheckable(true);
        button->setObjectName(QStringLiteral("segBtn"));
        button->setProperty("pos", pos);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        m_modeGroup->addButton(button, id);
        polishWidget(button);
        return button;
    };
    m_modeScreenBtn = makeSeg(tr("整屏"), QStringLiteral("first"),
                              int(RecordingController::CaptureMode::Screen));
    m_modeRegionBtn = makeSeg(tr("区域"), QStringLiteral("middle"),
                              int(RecordingController::CaptureMode::Region));
    m_modeWindowBtn = makeSeg(tr("窗口"), QStringLiteral("last"),
                              int(RecordingController::CaptureMode::Window));
    m_modeScreenBtn->setChecked(true);
    m_modeScreenBtn->setToolTip(tr("录制整个显示器"));
    m_modeRegionBtn->setToolTip(tr("拖拽选择一部分屏幕"));
    m_modeWindowBtn->setToolTip(tr("录制指定窗口"));

    auto *modeRow = new QHBoxLayout;
    modeRow->setSpacing(0);
    modeRow->addWidget(m_modeScreenBtn);
    modeRow->addWidget(m_modeRegionBtn);
    modeRow->addWidget(m_modeWindowBtn);
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

    m_windowRow = new QWidget(m_recorderPage);
    auto *windowLayout = new QHBoxLayout(m_windowRow);
    windowLayout->setContentsMargins(0, 0, 0, 0);
    windowLayout->setSpacing(8);
    m_windowCombo = AppStyle::createComboBox(m_windowRow);
    m_windowCombo->setMinimumContentsLength(18);
    m_refreshWindowsButton = new QPushButton(tr("刷新"), m_windowRow);
    m_refreshWindowsButton->setCursor(Qt::PointingHandCursor);
    windowLayout->addWidget(m_windowCombo, 1);
    windowLayout->addWidget(m_refreshWindowsButton);
    layout->addWidget(m_windowRow);

    auto *audioRow = new QHBoxLayout;
    audioRow->setSpacing(8);
    m_micCheck = new QCheckBox(tr("麦克风"), m_recorderPage);
    m_systemAudioCheck = new QCheckBox(tr("系统声音"), m_recorderPage);
    for (QCheckBox *box : {m_micCheck, m_systemAudioCheck}) {
        box->setObjectName(QStringLiteral("chipCheck"));
        box->setAttribute(Qt::WA_StyledBackground, true);
        box->setCursor(Qt::PointingHandCursor);
        box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        polishWidget(box);
    }
    m_micCheck->setIcon(AppIcons::mic());
    m_systemAudioCheck->setIcon(AppIcons::speaker());
    m_micCheck->setIconSize(QSize(16, 16));
    m_systemAudioCheck->setIconSize(QSize(16, 16));
    m_micCheck->setToolTip(tr("录制麦克风输入"));
    m_systemAudioCheck->setToolTip(
        tr("录制电脑正在播放的声音。开始前请先让内容出声。"));
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

    auto *saveFold = new SettingsFold(tr("保存位置"), inner);
    innerLayout->addWidget(saveFold);
    bindFold(saveFold, m_settings, QStringLiteral("save"));
    auto *saveLayout = saveFold->bodyLayout();
    m_pathEdit = new QLineEdit(inner);
    m_pathEdit->setPlaceholderText(tr("视频保存目录"));
    saveLayout->addWidget(m_pathEdit);
    auto *pathButtons = new QWidget(inner);
    auto *pathButtonLayout = new QHBoxLayout(pathButtons);
    pathButtonLayout->setContentsMargins(0, 0, 0, 0);
    pathButtonLayout->setSpacing(8);
    m_browseButton = new QPushButton(tr("浏览"), pathButtons);
    m_openFolderButton = new QPushButton(tr("打开"), pathButtons);
    m_browseButton->setCursor(Qt::PointingHandCursor);
    m_openFolderButton->setCursor(Qt::PointingHandCursor);
    m_openFolderButton->setIcon(AppIcons::folder());
    m_openFolderButton->setIconSize(QSize(14, 14));
    m_browseButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_openFolderButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    pathButtonLayout->addWidget(m_browseButton);
    pathButtonLayout->addWidget(m_openFolderButton);
    saveLayout->addWidget(pathButtons);

    auto *pictureFold = new SettingsFold(tr("画面"), inner);
    innerLayout->addWidget(pictureFold);
    bindFold(pictureFold, m_settings, QStringLiteral("picture"));
    auto *pictureLayout = pictureFold->bodyLayout();
    m_resolutionCombo = AppStyle::createComboBox(inner);
    m_resolutionCombo->addItem(tr("原始"), QSize());
    m_resolutionCombo->addItem(QStringLiteral("1080p"), QSize(1920, 1080));
    m_resolutionCombo->addItem(QStringLiteral("720p"), QSize(1280, 720));
    m_fpsCombo = AppStyle::createComboBox(inner);
    m_fpsCombo->addItem(QStringLiteral("24 FPS"), 24);
    m_fpsCombo->addItem(QStringLiteral("30 FPS"), 30);
    m_fpsCombo->addItem(QStringLiteral("60 FPS"), 60);
    m_qualityCombo = AppStyle::createComboBox(inner);
    m_qualityCombo->addItem(tr("低"), int(QMediaRecorder::LowQuality));
    m_qualityCombo->addItem(tr("标准"), int(QMediaRecorder::NormalQuality));
    m_qualityCombo->addItem(tr("高"), int(QMediaRecorder::HighQuality));
    m_qualityCombo->addItem(tr("最高"), int(QMediaRecorder::VeryHighQuality));
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

    auto *hotkeyFold = new SettingsFold(tr("热键"), inner);
    innerLayout->addWidget(hotkeyFold);
    bindFold(hotkeyFold, m_settings, QStringLiteral("hotkey"));
    auto *hotkeyLayout = hotkeyFold->bodyLayout();
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
    connect(m_browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(m_openFolderButton, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);
    connect(m_screenCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onScreenSelectionChanged);
    connect(m_regionButton, &QPushButton::clicked, this, &MainWindow::onSelectRegionClicked);
    connect(m_clearRegionButton, &QPushButton::clicked, this, &MainWindow::onClearRegionClicked);
    connect(m_refreshWindowsButton, &QPushButton::clicked, this, &MainWindow::refreshWindows);
    connect(m_modeGroup, &QButtonGroup::idClicked, this, [this](int) { onModeChanged(); });
    connect(m_hotkeyCheck, &QCheckBox::toggled, this, &MainWindow::applyHotkeys);
    connect(m_startHotkeyEdit, &QKeySequenceEdit::editingFinished, this, &MainWindow::applyHotkeys);
    connect(m_stopHotkeyEdit, &QKeySequenceEdit::editingFinished, this, &MainWindow::applyHotkeys);
    connect(m_pauseHotkeyEdit, &QKeySequenceEdit::editingFinished, this, &MainWindow::applyHotkeys);
    connect(m_settingsButton, &QToolButton::clicked, this, &MainWindow::showSettingsPage);
    connect(m_backButton, &QToolButton::clicked, this, &MainWindow::showRecorderPage);
    connect(m_preview, &PreviewWidget::clicked, this, &MainWindow::onPreviewClicked);
    connect(m_windowCombo, &QComboBox::currentIndexChanged, this, &MainWindow::updatePreview);
    connect(m_countdownCombo, &QComboBox::currentIndexChanged, this, &MainWindow::updateHint);

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
        const QString fileName = QFileInfo(path).fileName();
        statusBar()->showMessage(tr("已保存  %1").arg(fileName), 5000);
        if (m_tray)
            m_tray->showMessage(QStringLiteral("ScreenRec"),
                                tr("已保存到 %1").arg(QDir::toNativeSeparators(path)),
                                QSystemTrayIcon::Information, 3000);
        restoreMainWindow();
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

    restoreWindowGeometry();
}

void MainWindow::restoreWindowGeometry()
{
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
    m_settings->setCaptureMode(int(currentMode()));
    m_settings->setRegion(m_region);
    m_settings->setLastWindowDescription(m_windowCombo->currentText());
    m_settings->setHotkeysEnabled(m_hotkeyCheck->isChecked());
    m_settings->setStartHotkey(m_startHotkeyEdit->keySequence());
    m_settings->setStopHotkey(m_stopHotkeyEdit->keySequence());
    m_settings->setPauseHotkey(m_pauseHotkeyEdit->keySequence());
    if (!isMinimized()) {
        m_settings->setWindowSize(isMaximized() ? normalGeometry().size() : size());
        m_settings->setWindowMaximized(isMaximized());
    }
}

void MainWindow::updateModeRows()
{
    const auto mode = currentMode();
    const bool region = mode == RecordingController::CaptureMode::Region;
    const bool window = mode == RecordingController::CaptureMode::Window;

    m_screenRow->setVisible(!window);
    m_regionRow->setVisible(region);
    m_windowRow->setVisible(window);
}

void MainWindow::updateRegionLabel()
{
    const bool hasRegion = m_region.width() >= 16 && m_region.height() >= 16;
    if (hasRegion) {
        m_regionLabel->setText(tr("%1 × %2").arg(m_region.width()).arg(m_region.height()));
        m_regionLabel->setToolTip(tr("位置 %1, %2").arg(m_region.x()).arg(m_region.y()));
    } else {
        m_regionLabel->setText(tr("未选择区域"));
        m_regionLabel->setToolTip(QString());
    }
    if (m_clearRegionButton)
        m_clearRegionButton->setVisible(hasRegion);
}

void MainWindow::updateHint()
{
    if (!m_hintLabel || !m_countdownCombo)
        return;

    QStringList parts;
    const int countdown = m_countdownCombo->currentData().toInt();
    if (countdown > 0)
        parts << tr("%1 秒倒计时").arg(countdown);
    if (m_hotkeyCheck->isChecked() && !m_startHotkeyEdit->keySequence().isEmpty()) {
        parts << tr("%1 开始")
                     .arg(m_startHotkeyEdit->keySequence().toString(QKeySequence::NativeText));
    }
    m_hintLabel->setText(parts.join(QStringLiteral("  ·  ")));
    m_hintLabel->setVisible(!parts.isEmpty());
}

void MainWindow::updateUi()
{
    const auto state = m_controller->state();
    const bool idle = state == RecordingController::State::Idle;

    m_modeScreenBtn->setEnabled(idle);
    m_modeRegionBtn->setEnabled(idle);
    m_modeWindowBtn->setEnabled(idle);
    m_screenCombo->setEnabled(idle);
    m_windowCombo->setEnabled(idle);
    m_refreshWindowsButton->setEnabled(idle);
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
    m_settingsButton->setEnabled(idle);

    m_startButton->setEnabled(idle);

    switch (state) {
    case RecordingController::State::Idle:
        m_floatingBar->setDurationText(formatDuration(0));
        statusBar()->showMessage(tr("就绪"));
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

void MainWindow::onClearRegionClicked()
{
    m_region = QRect();
    updateRegionLabel();
    updatePreview();
    saveSettings();
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

void MainWindow::onPreviewClicked()
{
    if (currentMode() == RecordingController::CaptureMode::Region)
        onSelectRegionClicked();
}

void MainWindow::showSettingsPage()
{
    m_stack->setCurrentWidget(m_settingsPage);
    syncPreviewTimer();
}

void MainWindow::showRecorderPage()
{
    saveSettings();
    updateHint();
    m_stack->setCurrentWidget(m_recorderPage);
    syncPreviewTimer();
    updatePreview();
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
        && currentMode() != RecordingController::CaptureMode::Window
        && m_stack->currentWidget() == m_recorderPage;
    if (run) {
        if (!m_previewTimer->isActive())
            m_previewTimer->start();
    } else {
        m_previewTimer->stop();
    }
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
    if (currentMode() == RecordingController::CaptureMode::Window) {
        const QString name = m_windowCombo->currentText();
        m_preview->setPlaceholder(name.isEmpty() ? tr("没有可录制的窗口") : name,
                                  tr("窗口模式不显示实时画面"));
        m_preview->setClickable(false);
        return;
    }

    const bool regionMode = currentMode() == RecordingController::CaptureMode::Region;
    const bool hasRegion = m_region.width() >= 16 && m_region.height() >= 16;
    m_preview->setClickable(regionMode);

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
    m_preview->setBadge(QString());
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
