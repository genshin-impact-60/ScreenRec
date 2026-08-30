#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "recordingcontroller.h"

#include <QMainWindow>
#include <QRect>

class AppSettings;
class CountdownOverlay;
class FloatingBar;
class HotkeyManager;
class RegionSelector;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QEvent;
class QFormLayout;
class QHideEvent;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QSystemTrayIcon;
class QTimer;

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
    void setupTray();
    void connectSignals();
    void refreshScreens();
    void refreshWindows();
    void saveSettings();
    void loadSettings();
    void updateUi();
    void updateModeRows();
    void updateRegionLabel();
    void updateFloatingBar();
    void placeFloatingBar();
    void applyHotkeys();
    void onStartClicked();
    void onPauseClicked();
    void onStopClicked();
    void onBrowseClicked();
    void onOpenFolderClicked();
    void onSelectRegionClicked();
    void onModeChanged();
    void onHotkey(int id);
    void beginRegionSelect();
    void actuallyStart();
    void restoreMainWindow();
    void quitApp();
    void updatePreview();
    void syncPreviewTimer();
    void clampRegion();
    void onOpenFileClicked();
    void onScreenSelectionChanged();
    QScreen *selectedScreen() const;
    RecordingController::CaptureMode currentMode() const;
    RecordingController::Request currentRequest() const;
    QIcon trayIcon() const;

    AppSettings *m_settings = nullptr;
    RecordingController *m_controller = nullptr;
    CountdownOverlay *m_overlay = nullptr;
    RegionSelector *m_regionSelector = nullptr;
    FloatingBar *m_floatingBar = nullptr;
    HotkeyManager *m_hotkeys = nullptr;
    QSystemTrayIcon *m_tray = nullptr;

    QFormLayout *m_form = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_screenCombo = nullptr;
    QComboBox *m_windowCombo = nullptr;
    QComboBox *m_countdownCombo = nullptr;
    QCheckBox *m_micCheck = nullptr;
    QCheckBox *m_systemAudioCheck = nullptr;
    QCheckBox *m_hotkeyCheck = nullptr;
    QComboBox *m_resolutionCombo = nullptr;
    QComboBox *m_fpsCombo = nullptr;
    QComboBox *m_qualityCombo = nullptr;
    QWidget *m_regionRow = nullptr;
    QWidget *m_windowRow = nullptr;
    QWidget *m_hotkeyRow = nullptr;
    QLabel *m_regionLabel = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QPushButton *m_browseButton = nullptr;
    QPushButton *m_openFolderButton = nullptr;
    QPushButton *m_regionButton = nullptr;
    QPushButton *m_refreshWindowsButton = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QKeySequenceEdit *m_startHotkeyEdit = nullptr;
    QKeySequenceEdit *m_stopHotkeyEdit = nullptr;
    QKeySequenceEdit *m_pauseHotkeyEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_durationLabel = nullptr;
    QLabel *m_fileLabel = nullptr;
    QLabel *m_previewLabel = nullptr;
    QPushButton *m_openFileButton = nullptr;
    QTimer *m_previewTimer = nullptr;
    QString m_lastFile;

    QRect m_region;
    bool m_closeAfterStop = false;
    bool m_hotkeysReady = false;
    bool m_startAfterRegion = false;
    int m_countdownShown = 0;
};

#endif
