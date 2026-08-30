#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "recordingcontroller.h"

#include <QMainWindow>
#include <QRect>

class AppSettings;
class AudioChip;
class AudioLevelMonitor;
class CountdownOverlay;
class FloatingBar;
class HotkeyManager;
class PreviewWidget;
class RegionSelector;
class QAction;
class QButtonGroup;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QEvent;
class QHideEvent;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QStackedWidget;
class QSystemTrayIcon;
class QTimer;
class QToolButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void setupRecorderPage();
    void setupSettingsPage();
    void setupTray();
    void connectSignals();
    void refreshScreens();
    void saveSettings();
    void loadSettings();
    void updateUi();
    void updateModeRows();
    void updateRegionLabel();
    void updateFloatingBar();
    void updateHint();
    void updateSummary();
    void updateIdleStatus();
    void placeFloatingBar();
    void applyHotkeys();
    void onStartClicked();
    void onPauseClicked();
    void onStopClicked();
    void onBrowseClicked();
    void onOpenFolderClicked();
    void onSelectRegionClicked();
    void onClearRegionClicked();
    void restoreWindowGeometry();
    void persistWindowGeometry();
    void onModeChanged();
    void onHotkey(int id);
    void beginRegionSelect();
    void actuallyStart();
    void restoreMainWindow();
    void quitApp();
    void updatePreview();
    void syncPreviewTimer();
    void syncAudioMonitor();
    void clampRegion();
    void onPreviewClicked();
    void onScreenSelectionChanged();
    void showSettingsPage();
    void showRecorderPage();
    void showLastResult();
    void onOpenLastFile();
    void resetHotkeys();
    QScreen *selectedScreen() const;
    RecordingController::CaptureMode currentMode() const;
    RecordingController::Request currentRequest() const;
    QSize captureSize() const;
    QIcon trayIcon() const;

    AppSettings *m_settings = nullptr;
    RecordingController *m_controller = nullptr;
    CountdownOverlay *m_overlay = nullptr;
    RegionSelector *m_regionSelector = nullptr;
    FloatingBar *m_floatingBar = nullptr;
    HotkeyManager *m_hotkeys = nullptr;
    AudioLevelMonitor *m_audioMonitor = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QAction *m_trayStart = nullptr;
    QAction *m_trayPause = nullptr;
    QAction *m_trayStop = nullptr;

    QStackedWidget *m_stack = nullptr;
    QWidget *m_recorderPage = nullptr;
    QWidget *m_settingsPage = nullptr;
    QButtonGroup *m_modeGroup = nullptr;
    QToolButton *m_modeScreenBtn = nullptr;
    QToolButton *m_modeRegionBtn = nullptr;
    QToolButton *m_backButton = nullptr;
    PreviewWidget *m_preview = nullptr;

    QComboBox *m_screenCombo = nullptr;
    QComboBox *m_countdownCombo = nullptr;
    AudioChip *m_micCheck = nullptr;
    AudioChip *m_systemAudioCheck = nullptr;
    QCheckBox *m_hotkeyCheck = nullptr;
    QComboBox *m_resolutionCombo = nullptr;
    QComboBox *m_fpsCombo = nullptr;
    QComboBox *m_qualityCombo = nullptr;
    QWidget *m_screenRow = nullptr;
    QWidget *m_regionRow = nullptr;
    QWidget *m_hotkeyRow = nullptr;
    QLabel *m_regionLabel = nullptr;
    QLabel *m_hotkeyError = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QPushButton *m_browseButton = nullptr;
    QPushButton *m_openFolderButton = nullptr;
    QPushButton *m_regionButton = nullptr;
    QPushButton *m_clearRegionButton = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_summaryButton = nullptr;
    QPushButton *m_resetHotkeysButton = nullptr;
    QKeySequenceEdit *m_startHotkeyEdit = nullptr;
    QKeySequenceEdit *m_stopHotkeyEdit = nullptr;
    QKeySequenceEdit *m_pauseHotkeyEdit = nullptr;
    QLabel *m_hintLabel = nullptr;
    QTimer *m_previewTimer = nullptr;

    QRect m_region;
    QString m_lastSavedPath;
    qint64 m_lastDurationMs = 0;
    bool m_closeAfterStop = false;
    bool m_hotkeysReady = false;
    bool m_startAfterRegion = false;
    bool m_geometryRestored = false;
    bool m_loadingSettings = false;
    int m_countdownShown = 0;
};

#endif
